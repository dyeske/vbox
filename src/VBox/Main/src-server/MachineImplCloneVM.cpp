/* $Id$ */
/** @file
 * Implementation of MachineCloneVM
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "MachineImplCloneVM.h"

#include "VirtualBoxImpl.h"
#include "MediumImpl.h"

#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/cpp/utils.h>

#include <VBox/com/list.h>
#include <VBox/com/MultiResult.h>

// typedefs
/////////////////////////////////////////////////////////////////////////////

typedef struct
{
    ComPtr<IMedium>         pMedium;
    uint64_t                uSize;
}MEDIUMTASK;

typedef struct
{
    RTCList<MEDIUMTASK>     chain;
    bool                    fCreateDiffs;
}MEDIUMTASKCHAIN;

typedef struct
{
    Guid                    snapshotUuid;
    Utf8Str                 strSaveStateFile;
    uint64_t                cbSize;
}SAVESTATETASK;

// The private class
/////////////////////////////////////////////////////////////////////////////

struct MachineCloneVMPrivate
{
    MachineCloneVMPrivate(MachineCloneVM *a_q, ComObjPtr<Machine> &a_pSrcMachine, ComObjPtr<Machine> &a_pTrgMachine, CloneMode_T a_mode, bool a_fFullClone)
      : q_ptr(a_q)
      , p(a_pSrcMachine)
      , pSrcMachine(a_pSrcMachine)
      , pTrgMachine(a_pTrgMachine)
      , mode(a_mode)
      , fLinkDisks(!a_fFullClone)
    {}

    /* Thread management */
    int startWorker()
    {
        return RTThreadCreate(NULL,
                              MachineCloneVMPrivate::workerThread,
                              static_cast<void*>(this),
                              0,
                              RTTHREADTYPE_MAIN_WORKER,
                              0,
                              "MachineClone");
    }

    static int workerThread(RTTHREAD /* Thread */, void *pvUser)
    {
        MachineCloneVMPrivate *pTask = static_cast<MachineCloneVMPrivate*>(pvUser);
        AssertReturn(pTask, VERR_INVALID_POINTER);

        HRESULT rc = pTask->q_ptr->run();

        pTask->pProgress->notifyComplete(rc);

        pTask->q_ptr->destroy();

        return VINF_SUCCESS;
    }

    /* Private helper methods */
    HRESULT cloneCreateMachineList(const ComPtr<ISnapshot> &pSnapshot, RTCList< ComObjPtr<Machine> > &machineList) const;
    settings::Snapshot cloneFindSnapshot(settings::MachineConfigFile *pMCF, const settings::SnapshotsList &snl, const Guid &id) const;
    void cloneUpdateStorageLists(settings::StorageControllersList &sc, const Bstr &bstrOldId, const Bstr &bstrNewId) const;
    void cloneUpdateSnapshotStorageLists(settings::SnapshotsList &sl, const Bstr &bstrOldId, const Bstr &bstrNewId) const;
    void cloneUpdateStateFile(settings::SnapshotsList &snl, const Guid &id, const Utf8Str &strFile) const;
    static int cloneCopyStateFileProgress(unsigned uPercentage, void *pvUser);

    /* Private q and parent pointer */
    MachineCloneVM             *q_ptr;
    ComObjPtr<Machine>          p;

    /* Private helper members */
    ComObjPtr<Machine>          pSrcMachine;
    ComObjPtr<Machine>          pTrgMachine;
    ComPtr<IMachine>            pOldMachineState;
    ComObjPtr<Progress>         pProgress;
    Guid                        snapshotId;
    CloneMode_T                 mode;
    bool                        fLinkDisks;
    RTCList<MEDIUMTASKCHAIN>    llMedias;
    RTCList<SAVESTATETASK>      llSaveStateFiles; /* Snapshot UUID -> File path */
};

HRESULT MachineCloneVMPrivate::cloneCreateMachineList(const ComPtr<ISnapshot> &pSnapshot, RTCList< ComObjPtr<Machine> > &machineList) const
{
    HRESULT rc = S_OK;
    Bstr name;
    rc = pSnapshot->COMGETTER(Name)(name.asOutParam());
    if (FAILED(rc)) return rc;

    ComPtr<IMachine> pMachine;
    rc = pSnapshot->COMGETTER(Machine)(pMachine.asOutParam());
    if (FAILED(rc)) return rc;
    machineList.append((Machine*)(IMachine*)pMachine);

    SafeIfaceArray<ISnapshot> sfaChilds;
    rc = pSnapshot->COMGETTER(Children)(ComSafeArrayAsOutParam(sfaChilds));
    if (FAILED(rc)) return rc;
    for (size_t i = 0; i < sfaChilds.size(); ++i)
    {
        rc = cloneCreateMachineList(sfaChilds[i], machineList);
        if (FAILED(rc)) return rc;
    }

    return rc;
}

settings::Snapshot MachineCloneVMPrivate::cloneFindSnapshot(settings::MachineConfigFile *pMCF, const settings::SnapshotsList &snl, const Guid &id) const
{
    settings::SnapshotsList::const_iterator it;
    for (it = snl.begin(); it != snl.end(); ++it)
    {
        if (it->uuid == id)
            return *it;
        else if (!it->llChildSnapshots.empty())
            return cloneFindSnapshot(pMCF, it->llChildSnapshots, id);
    }
    return settings::Snapshot();
}

void MachineCloneVMPrivate::cloneUpdateStorageLists(settings::StorageControllersList &sc, const Bstr &bstrOldId, const Bstr &bstrNewId) const
{
    settings::StorageControllersList::iterator it3;
    for (it3 = sc.begin();
         it3 != sc.end();
         ++it3)
    {
        settings::AttachedDevicesList &llAttachments = it3->llAttachedDevices;
        settings::AttachedDevicesList::iterator it4;
        for (it4 = llAttachments.begin();
             it4 != llAttachments.end();
             ++it4)
        {
            if (   it4->deviceType == DeviceType_HardDisk
                && it4->uuid == bstrOldId)
            {
                it4->uuid = bstrNewId;
            }
        }
    }
}

void MachineCloneVMPrivate::cloneUpdateSnapshotStorageLists(settings::SnapshotsList &sl, const Bstr &bstrOldId, const Bstr &bstrNewId) const
{
    settings::SnapshotsList::iterator it;
    for (  it  = sl.begin();
           it != sl.end();
         ++it)
    {
        cloneUpdateStorageLists(it->storage.llStorageControllers, bstrOldId, bstrNewId);
        if (!it->llChildSnapshots.empty())
            cloneUpdateSnapshotStorageLists(it->llChildSnapshots, bstrOldId, bstrNewId);
    }
}

void MachineCloneVMPrivate::cloneUpdateStateFile(settings::SnapshotsList &snl, const Guid &id, const Utf8Str &strFile) const
{
    settings::SnapshotsList::iterator it;
    for (it = snl.begin(); it != snl.end(); ++it)
    {
        if (it->uuid == id)
            it->strStateFile = strFile;
        else if (!it->llChildSnapshots.empty())
            cloneUpdateStateFile(it->llChildSnapshots, id, strFile);
    }
}

/* static */
int MachineCloneVMPrivate::cloneCopyStateFileProgress(unsigned uPercentage, void *pvUser)
{
    ComObjPtr<Progress> pProgress = *static_cast< ComObjPtr<Progress>* >(pvUser);

    BOOL fCanceled = false;
    HRESULT rc = pProgress->COMGETTER(Canceled)(&fCanceled);
    if (FAILED(rc)) return VERR_GENERAL_FAILURE;
    /* If canceled by the user tell it to the copy operation. */
    if (fCanceled) return VERR_CANCELLED;
    /* Set the new process. */
    rc = pProgress->SetCurrentOperationProgress(uPercentage);
    if (FAILED(rc)) return VERR_GENERAL_FAILURE;

    return VINF_SUCCESS;
}


// The public class
/////////////////////////////////////////////////////////////////////////////

MachineCloneVM::MachineCloneVM(ComObjPtr<Machine> pSrcMachine, ComObjPtr<Machine> pTrgMachine, CloneMode_T mode, bool fFullClone)
    : d_ptr(new MachineCloneVMPrivate(this, pSrcMachine, pTrgMachine, mode, fFullClone))
{
}

MachineCloneVM::~MachineCloneVM()
{
    delete d_ptr;
}

HRESULT MachineCloneVM::start(IProgress **pProgress)
{
    DPTR(MachineCloneVM);
    ComObjPtr<Machine> &p = d->p;

    HRESULT rc;
    try
    {
        /* Lock the target machine early (so nobody mess around with it in the meantime). */
        AutoWriteLock trgLock(d->pTrgMachine COMMA_LOCKVAL_SRC_POS);

        if (d->pSrcMachine->isSnapshotMachine())
            d->snapshotId = d->pSrcMachine->getSnapshotId();

        /* Add the current machine and all snapshot machines below this machine
         * in a list for further processing. */
        RTCList< ComObjPtr<Machine> > machineList;

        /* Include current state? */
        if (   d->mode == CloneMode_MachineState)
//            || d->mode == CloneMode_AllStates)
            machineList.append(d->pSrcMachine);
        /* Should be done a depth copy with all child snapshots? */
        if (   d->mode == CloneMode_MachineAndChildStates
            || d->mode == CloneMode_AllStates)
        {
            ULONG cSnapshots = 0;
            rc = d->pSrcMachine->COMGETTER(SnapshotCount)(&cSnapshots);
            if (FAILED(rc)) throw rc;
            if (cSnapshots > 0)
            {
                Utf8Str id;
                if (    d->mode == CloneMode_MachineAndChildStates
                    && !d->snapshotId.isEmpty())
                    id = d->snapshotId.toString();
                ComPtr<ISnapshot> pSnapshot;
                rc = d->pSrcMachine->FindSnapshot(Bstr(id).raw(), pSnapshot.asOutParam());
                if (FAILED(rc)) throw rc;
                rc = d->cloneCreateMachineList(pSnapshot, machineList);
                if (FAILED(rc)) throw rc;
                rc = pSnapshot->COMGETTER(Machine)(d->pOldMachineState.asOutParam());
                if (FAILED(rc)) throw rc;
            }
        }

        /* Go over every machine and walk over every attachment this machine has. */
        ULONG uCount       = 2; /* One init task and the machine creation. */
        ULONG uTotalWeight = 2; /* The init task and the machine creation is worth one. */
        for (size_t i = 0; i < machineList.size(); ++i)
        {
            ComObjPtr<Machine> machine = machineList.at(i);
            /* If this is the Snapshot Machine we want to clone, we need to
             * create a new diff file for the new "current state". */
            bool fCreateDiffs = false;
            if (machine == d->pOldMachineState)
                fCreateDiffs = true;
            SafeIfaceArray<IMediumAttachment> sfaAttachments;
            rc = machine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(sfaAttachments));
            if (FAILED(rc)) throw rc;
            /* Add all attachments (and there parents) of the different
             * machines to a worker list. */
            for (size_t a = 0; a < sfaAttachments.size(); ++a)
            {
                const ComPtr<IMediumAttachment> &pAtt = sfaAttachments[a];
                DeviceType_T type;
                rc = pAtt->COMGETTER(Type)(&type);
                if (FAILED(rc)) throw rc;

                /* Only harddisk's are of interest. */
                if (type != DeviceType_HardDisk)
                    continue;

                /* Valid medium attached? */
                ComPtr<IMedium> pSrcMedium;
                rc = pAtt->COMGETTER(Medium)(pSrcMedium.asOutParam());
                if (FAILED(rc)) throw rc;
                if (pSrcMedium.isNull())
                    continue;

                /* Build up a child->parent list of this attachment. (Note: we are
                 * not interested of any child's not attached to this VM. So this
                 * will not create a full copy of the base/child relationship.) */
                MEDIUMTASKCHAIN mtc;
                mtc.fCreateDiffs = fCreateDiffs;
                while(!pSrcMedium.isNull())
                {
                    /* Refresh the state so that the file size get read. */
                    MediumState_T e;
                    rc = pSrcMedium->RefreshState(&e);
                    if (FAILED(rc)) throw rc;
                    LONG64 lSize;
                    rc = pSrcMedium->COMGETTER(Size)(&lSize);
                    if (FAILED(rc)) throw rc;

                    /* Save the current medium, for later cloning. */
                    MEDIUMTASK mt;
                    mt.pMedium = pSrcMedium;
                    mt.uSize   = lSize;
                    mtc.chain.append(mt);

                    /* Calculate progress data */
                    ++uCount;
                    uTotalWeight += lSize;

                    /* Query next parent. */
                    rc = pSrcMedium->COMGETTER(Parent)(pSrcMedium.asOutParam());
                    if (FAILED(rc)) throw rc;
                };
                d->llMedias.append(mtc);
            }
            Bstr bstrSrcSaveStatePath;
            rc = machine->COMGETTER(StateFilePath)(bstrSrcSaveStatePath.asOutParam());
            if (FAILED(rc)) throw rc;
            if (!bstrSrcSaveStatePath.isEmpty())
            {
                SAVESTATETASK sst;
                sst.snapshotUuid     = machine->getSnapshotId();
                sst.strSaveStateFile = bstrSrcSaveStatePath;
                int vrc = RTFileQuerySize(sst.strSaveStateFile.c_str(), &sst.cbSize);
                if (RT_FAILURE(vrc))
                    throw p->setError(VBOX_E_IPRT_ERROR, p->tr("Could not query file size of '%s' (%Rrc)"), sst.strSaveStateFile.c_str(), vrc);
                d->llSaveStateFiles.append(sst);
                ++uCount;
                uTotalWeight += sst.cbSize;
            }
        }

        rc = d->pProgress.createObject();
        if (FAILED(rc)) throw rc;
        rc = d->pProgress->init(p->getVirtualBox(),
                                static_cast<IMachine*>(d->pSrcMachine) /* aInitiator */,
                                Bstr(p->tr("Cloning Machine")).raw(),
                                true /* fCancellable */,
                                uCount,
                                uTotalWeight,
                                Bstr(p->tr("Initialize Cloning")).raw(),
                                1);
        if (FAILED(rc)) throw rc;

        int vrc = d->startWorker();

        if (RT_FAILURE(vrc))
            p->setError(VBOX_E_IPRT_ERROR, "Could not create machine clone thread (%Rrc)", vrc);
    }
    catch (HRESULT rc2)
    {
        rc = rc2;
    }

    if (SUCCEEDED(rc))
        d->pProgress.queryInterfaceTo(pProgress);

    return rc;
}

HRESULT MachineCloneVM::run()
{
    DPTR(MachineCloneVM);
    ComObjPtr<Machine> &p = d->p;

    AutoCaller autoCaller(p);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock  srcLock(p COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock trgLock(d->pTrgMachine COMMA_LOCKVAL_SRC_POS);

    MultiResult rc = S_OK;

    /*
     * Todo:
     * - Regardless where the old media comes from (e.g. snapshots folder) it
     *   goes to the new main VM folder. Maybe we like to be a little bit
     *   smarter here.
     * - Snapshot diffs (can) have the uuid as name. After cloning this isn't
     *   right anymore. Is it worth to change to the new uuid? Or should the
     *   cloned disks called exactly as the original one or should all new disks
     *   get a new name with the new VM name in it.
     */

    /* Where should all the media go? */
    Utf8Str strTrgMachineFolder = d->pTrgMachine->getSettingsFileFull();
    strTrgMachineFolder.stripFilename();

    RTCList< ComObjPtr<Medium> > newMedias; /* All created images */
    RTCList<Utf8Str> newFiles;              /* All extra created files (save states, ...) */
    try
    {
        /* Copy all the configuration from this machine to an empty
         * configuration dataset. */
        settings::MachineConfigFile trgMCF = *d->pSrcMachine->mData->pMachineConfigFile;

        /* Reset media registry. */
        trgMCF.mediaRegistry.llHardDisks.clear();
        /* If we got a valid snapshot id, replace the hardware/storage section
         * with the stuff from the snapshot. */
        settings::Snapshot sn;
        if (!d->snapshotId.isEmpty())
            sn = d->cloneFindSnapshot(&trgMCF, trgMCF.llFirstSnapshot, d->snapshotId);

        if (d->mode == CloneMode_MachineState)
        {
            if (!sn.uuid.isEmpty())
            {
                trgMCF.hardwareMachine = sn.hardware;
                trgMCF.storageMachine  = sn.storage;
            }

            /* Remove any hint on snapshots. */
            trgMCF.llFirstSnapshot.clear();
            trgMCF.uuidCurrentSnapshot.clear();
        }else
        if (   d->mode == CloneMode_MachineAndChildStates
            && !sn.uuid.isEmpty())
        {
            /* Copy the snapshot data to the current machine. */
            trgMCF.hardwareMachine = sn.hardware;
            trgMCF.storageMachine  = sn.storage;

            /* The snapshot will be the root one. */
            trgMCF.uuidCurrentSnapshot = sn.uuid;
            trgMCF.llFirstSnapshot.clear();
            trgMCF.llFirstSnapshot.push_back(sn);
        }

        /* When the current snapshot folder is absolute we reset it to the
         * default relative folder. */
        if (RTPathStartsWithRoot(trgMCF.machineUserData.strSnapshotFolder.c_str()))
            trgMCF.machineUserData.strSnapshotFolder = "Snapshots";
        trgMCF.strStateFile = "";
        /* Force writing of setting file. */
        trgMCF.fCurrentStateModified = true;
        /* Set the new name. */
        trgMCF.machineUserData.strName = d->pTrgMachine->mUserData->s.strName;
        trgMCF.uuid = d->pTrgMachine->mData->mUuid;

        Bstr bstrSrcSnapshotFolder;
        rc = d->pSrcMachine->COMGETTER(SnapshotFolder)(bstrSrcSnapshotFolder.asOutParam());
        if (FAILED(rc)) throw rc;
        /* The absolute name of the snapshot folder. */
        Utf8Str strTrgSnapshotFolder = Utf8StrFmt("%s%c%s%c", strTrgMachineFolder.c_str(), RTPATH_DELIMITER, trgMCF.machineUserData.strSnapshotFolder.c_str(), RTPATH_DELIMITER);

        /* We need to create a map with the already created medias. This is
         * necessary, cause different snapshots could have the same
         * parents/parent chain. If a medium is in this map already, it isn't
         * cloned a second time, but simply used. */
        typedef std::map<Utf8Str, ComObjPtr<Medium> > TStrMediumMap;
        typedef std::pair<Utf8Str, ComObjPtr<Medium> > TStrMediumPair;
        TStrMediumMap map;
        for (size_t i = 0; i < d->llMedias.size(); ++i)
        {
            const MEDIUMTASKCHAIN &mtc = d->llMedias.at(i);
            ComObjPtr<Medium> pNewParent;
            for (size_t a = mtc.chain.size(); a > 0; --a)
            {
                const MEDIUMTASK &mt = mtc.chain.at(a - 1);
                ComPtr<IMedium> pMedium = mt.pMedium;

                Bstr bstrSrcName;
                rc = pMedium->COMGETTER(Name)(bstrSrcName.asOutParam());
                if (FAILED(rc)) throw rc;

                rc = d->pProgress->SetNextOperation(BstrFmt(p->tr("Cloning Disk '%ls' ..."), bstrSrcName.raw()).raw(), mt.uSize);
                if (FAILED(rc)) throw rc;

                Bstr bstrSrcId;
                rc = pMedium->COMGETTER(Id)(bstrSrcId.asOutParam());
                if (FAILED(rc)) throw rc;

                /* Is a clone already there? */
                TStrMediumMap::iterator it = map.find(Utf8Str(bstrSrcId));
                if (it != map.end())
                    pNewParent = it->second;
                else
                {
                    ComPtr<IMediumFormat> pSrcFormat;
                    rc = pMedium->COMGETTER(MediumFormat)(pSrcFormat.asOutParam());
                    ULONG uSrcCaps = 0;
                    rc = pSrcFormat->COMGETTER(Capabilities)(&uSrcCaps);
                    if (FAILED(rc)) throw rc;

                    Bstr bstrSrcFormat = "VDI";
                    ULONG srcVar = MediumVariant_Standard;
                    /* Is the source file based? */
                    if ((uSrcCaps & MediumFormatCapabilities_File) == MediumFormatCapabilities_File)
                    {
                        /* Yes, just use the source format. Otherwise the defaults
                         * will be used. */
                        rc = pMedium->COMGETTER(Format)(bstrSrcFormat.asOutParam());
                        if (FAILED(rc)) throw rc;
                        rc = pMedium->COMGETTER(Variant)(&srcVar);
                        if (FAILED(rc)) throw rc;
                    }

                    /* Start creating the clone. */
                    ComObjPtr<Medium> pTarget;
                    rc = pTarget.createObject();
                    if (FAILED(rc)) throw rc;

                    Utf8Str strFile = Utf8StrFmt("%s%c%lS", strTrgMachineFolder.c_str(), RTPATH_DELIMITER, bstrSrcName.raw());
                    rc = pTarget->init(p->mParent,
                                       Utf8Str(bstrSrcFormat),
                                       strFile,
                                       d->pTrgMachine->mData->mUuid,  /* media registry */
                                       NULL                           /* llRegistriesThatNeedSaving */);
                    if (FAILED(rc)) throw rc;

                    /* Do the disk cloning. */
                    ComPtr<IProgress> progress2;
                    rc = pMedium->CloneTo(pTarget,
                                          srcVar,
                                          pNewParent,
                                          progress2.asOutParam());
                    if (FAILED(rc)) throw rc;

                    /* Wait until the asynchrony process has finished. */
                    srcLock.release();
                    rc = d->pProgress->WaitForAsyncProgressCompletion(progress2);
                    srcLock.acquire();
                    if (FAILED(rc)) throw rc;

                    /* Check the result of the asynchrony process. */
                    LONG iRc;
                    rc = progress2->COMGETTER(ResultCode)(&iRc);
                    if (FAILED(rc)) throw rc;
                    if (FAILED(iRc))
                    {
                        /* If the thread of the progress object has an error, then
                         * retrieve the error info from there, or it'll be lost. */
                        ProgressErrorInfo info(progress2);
                        throw p->setError(iRc, Utf8Str(info.getText()).c_str());
                    }

                    map.insert(TStrMediumPair(Utf8Str(bstrSrcId), pTarget));

                    /* Remember created medias. */
                    newMedias.append(pTarget);
                    /* This medium becomes the parent of the next medium in the
                     * chain. */
                    pNewParent = pTarget;
                }
            }

            /* Create diffs for the last image chain. */
            if (mtc.fCreateDiffs)
            {
                Bstr bstrSrcId;
                rc = pNewParent->COMGETTER(Id)(bstrSrcId.asOutParam());
                if (FAILED(rc)) throw rc;
                GuidList *pllRegistriesThatNeedSaving;
                ComObjPtr<Medium> diff;
                diff.createObject();
                rc = diff->init(p->mParent,
                                pNewParent->getPreferredDiffFormat(),
                                strTrgSnapshotFolder,
                                d->pTrgMachine->mData->mUuid,
                                NULL); // pllRegistriesThatNeedSaving
                if (FAILED(rc)) throw rc;
                MediumLockList *pMediumLockList(new MediumLockList()); /* todo: deleteeeeeeeee */
                rc = diff->createMediumLockList(true /* fFailIfInaccessible */,
                                                true /* fMediumLockWrite */,
                                                pNewParent,
                                                *pMediumLockList);
                if (FAILED(rc)) throw rc;
                rc = pMediumLockList->Lock();
                if (FAILED(rc)) throw rc;
                rc = pNewParent->createDiffStorage(diff, MediumVariant_Standard,
                                                   pMediumLockList,
                                                   NULL /* aProgress */,
                                                   true /* aWait */,
                                                   NULL); // pllRegistriesThatNeedSaving
                delete pMediumLockList;
                if (FAILED(rc)) throw rc;
                pNewParent = diff;
            }
            Bstr bstrSrcId;
            rc = mtc.chain.first().pMedium->COMGETTER(Id)(bstrSrcId.asOutParam());
            if (FAILED(rc)) throw rc;
            Bstr bstrTrgId;
            rc = pNewParent->COMGETTER(Id)(bstrTrgId.asOutParam());
            if (FAILED(rc)) throw rc;
            /* We have to patch the configuration, so it contains the new
             * medium uuid instead of the old one. */
            d->cloneUpdateStorageLists(trgMCF.storageMachine.llStorageControllers, bstrSrcId, bstrTrgId);
            d->cloneUpdateSnapshotStorageLists(trgMCF.llFirstSnapshot, bstrSrcId, bstrTrgId);
        }
        /* Clone all save state files. */
        for (size_t i = 0; i < d->llSaveStateFiles.size(); ++i)
        {
            SAVESTATETASK sst = d->llSaveStateFiles.at(i);
            const Utf8Str &strTrgSaveState = Utf8StrFmt("%s%s", strTrgSnapshotFolder.c_str(), RTPathFilename(sst.strSaveStateFile.c_str()));

            /* Move to next sub-operation. */
            rc = d->pProgress->SetNextOperation(BstrFmt(p->tr("Copy save state file '%s' ..."), RTPathFilename(sst.strSaveStateFile.c_str())).raw(), sst.cbSize);
            if (FAILED(rc)) throw rc;
            /* Copy the file only if it was not copied already. */
            if (!newFiles.contains(strTrgSaveState.c_str()))
            {
                int vrc = RTFileCopyEx(sst.strSaveStateFile.c_str(), strTrgSaveState.c_str(), 0, MachineCloneVMPrivate::cloneCopyStateFileProgress, &d->pProgress);
                if (RT_FAILURE(vrc))
                    throw p->setError(VBOX_E_IPRT_ERROR,
                                      p->tr("Could not copy state file '%s' to '%s' (%Rrc)"), sst.strSaveStateFile.c_str(), strTrgSaveState.c_str(), vrc);
                newFiles.append(strTrgSaveState);
            }
            /* Update the path in the configuration either for the current
             * machine state or the snapshots. */
            if (sst.snapshotUuid.isEmpty())
                trgMCF.strStateFile = strTrgSaveState;
            else
                d->cloneUpdateStateFile(trgMCF.llFirstSnapshot, sst.snapshotUuid, strTrgSaveState);
        }

        if (false)
//        if (!d->pOldMachineState.isNull())
        {
            SafeIfaceArray<IMediumAttachment> sfaAttachments;
            rc = d->pOldMachineState->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(sfaAttachments));
            if (FAILED(rc)) throw rc;
            for (size_t a = 0; a < sfaAttachments.size(); ++a)
            {
                const ComPtr<IMediumAttachment> &pAtt = sfaAttachments[a];
                DeviceType_T type;
                rc = pAtt->COMGETTER(Type)(&type);
                if (FAILED(rc)) throw rc;

                /* Only harddisk's are of interest. */
                if (type != DeviceType_HardDisk)
                    continue;

                /* Valid medium attached? */
                ComPtr<IMedium> pSrcMedium;
                rc = pAtt->COMGETTER(Medium)(pSrcMedium.asOutParam());
                if (FAILED(rc)) throw rc;
                if (pSrcMedium.isNull())
                    continue;

//                ComObjPtr<Medium> pMedium = static_cast<Medium*>((IMedium*)pSrcMedium);
//                ComObjPtr<Medium> diff;
//                diff.createObject();
                // store this diff in the same registry as the parent
//                Guid uuidRegistryParent;
//                if (!medium->getFirstRegistryMachineId(uuidRegistryParent))
//                {
                    // parent image has no registry: this can happen if we're attaching a new immutable
                    // image that has not yet been attached (medium then points to the base and we're
                    // creating the diff image for the immutable, and the parent is not yet registered);
                    // put the parent in the machine registry then
//                    addMediumToRegistry(medium, llRegistriesThatNeedSaving, &uuidRegistryParent);
//                }
//                rc = diff->init(mParent,
//                                pMedium->getPreferredDiffFormat(),
//                                strFullSnapshotFolder.append(RTPATH_SLASH_STR),
//                                uuidRegistryParent,
//                                pllRegistriesThatNeedSaving);
//                if (FAILED(rc)) throw rc;
//
//                rc = pMedium->createDiffStorage(diff, MediumVariant_Standard,
//                                                pMediumLockList,
//                                                NULL /* aProgress */,
//                                                true /* aWait */,
//                                                pllRegistriesThatNeedSaving);
            }
        }

        {
            rc = d->pProgress->SetNextOperation(BstrFmt(p->tr("Create Machine Clone '%s' ..."), trgMCF.machineUserData.strName.c_str()).raw(), 1);
            if (FAILED(rc)) throw rc;
            /* After modifying the new machine config, we can copy the stuff
             * over to the new machine. The machine have to be mutable for
             * this. */
            rc = d->pTrgMachine->checkStateDependency(p->MutableStateDep);
            if (FAILED(rc)) throw rc;
            rc = d->pTrgMachine->loadMachineDataFromSettings(trgMCF,
                                                             &d->pTrgMachine->mData->mUuid);
            if (FAILED(rc)) throw rc;
        }

        /* The medias are created before the machine was there. We have to make
         * sure the new medias know of there new parent or we get in trouble
         * when the media registry is saved for this VM, especially in case of
         * difference image chain's. See VirtualBox::saveMediaRegistry.*/
//        for (size_t i = 0; i < newBaseMedias.size(); ++i)
//        {
//            rc = newBaseMedias.at(i)->addRegistry(d->pTrgMachine->mData->mUuid, true /* fRecursive */);
//            if (FAILED(rc)) throw rc;
//        }

        /* Now save the new configuration to disk. */
        rc = d->pTrgMachine->SaveSettings();
        if (FAILED(rc)) throw rc;
    }
    catch(HRESULT rc2)
    {
        rc = rc2;
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
    }

    /* Cleanup on failure (CANCEL also) */
    if (FAILED(rc))
    {
        int vrc = VINF_SUCCESS;
        /* Delete all created files. */
        for (size_t i = 0; i < newFiles.size(); ++i)
        {
            vrc = RTFileDelete(newFiles.at(i).c_str());
            if (RT_FAILURE(vrc))
                rc = p->setError(VBOX_E_IPRT_ERROR, p->tr("Could not delete file '%s' (%Rrc)"), newFiles.at(i).c_str(), vrc);
        }
        /* Delete all already created medias. (Reverse, cause there could be
         * parent->child relations.) */
        for (size_t i = newMedias.size(); i > 0; --i)
        {
            ComObjPtr<Medium> &pMedium = newMedias.at(i - 1);
            AutoCaller mac(pMedium);
            if (FAILED(mac.rc())) { continue; rc = mac.rc(); }
            AutoReadLock mlock(pMedium COMMA_LOCKVAL_SRC_POS);
            bool fFile = pMedium->isMediumFormatFile();
            Utf8Str strLoc = pMedium->getLocationFull();
            mlock.release();
            /* Close the medium. If this succeed, delete it finally from the
             * disk. */
            rc = pMedium->close(NULL, mac);
            if (FAILED(rc)) continue;
            if (fFile)
            {
                vrc = RTFileDelete(strLoc.c_str());
                if (RT_FAILURE(vrc))
                    rc = p->setError(VBOX_E_IPRT_ERROR, p->tr("Could not delete file '%s' (%Rrc)"), strLoc.c_str(), vrc);
            }
        }
        /* Delete the machine folder when not empty. */
        RTDirRemove(strTrgMachineFolder.c_str());
    }

    return rc;
}

void MachineCloneVM::destroy()
{
    delete this;
}

