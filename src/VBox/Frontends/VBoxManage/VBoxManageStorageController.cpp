/* $Id$ */
/** @file
 * VBoxManage - The storage controller related commands.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_ONLY_DOCS

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/ctype.h>
#include <iprt/stream.h>
#include <iprt/getopt.h>
#include <VBox/log.h>

#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////


static const RTGETOPTDEF g_aStorageAttachOptions[] =
{
    { "--storagectl",     's', RTGETOPT_REQ_STRING },
    { "--port",           'p', RTGETOPT_REQ_UINT32 },
    { "--device",         'd', RTGETOPT_REQ_UINT32 },
    { "--medium",         'm', RTGETOPT_REQ_STRING },
    { "--type",           't', RTGETOPT_REQ_STRING },
    { "--passthrough",    'h', RTGETOPT_REQ_STRING },
    { "--forceunmount",   'f', RTGETOPT_REQ_NOTHING },
};

int handleStorageAttach(HandlerArg *a)
{
    int c = VERR_INTERNAL_ERROR;        /* initialized to shut up gcc */
    HRESULT rc = S_OK;
    ULONG port   = ~0U;
    ULONG device = ~0U;
    bool fForceUnmount = false;
    const char *pszCtl  = NULL;
    const char *pszType = NULL;
    const char *pszMedium = NULL;
    const char *pszPassThrough = NULL;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    ComPtr<IMachine> machine;
    ComPtr<IStorageController> storageCtl;
    ComPtr<ISystemProperties> systemProperties;

    if (a->argc < 9)
        return errorSyntax(USAGE_STORAGEATTACH, "Too few parameters");
    else if (a->argc > 13)
        return errorSyntax(USAGE_STORAGEATTACH, "Too many parameters");

    RTGetOptInit(&GetState, a->argc, a->argv, g_aStorageAttachOptions,
                 RT_ELEMENTS(g_aStorageAttachOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);

    while (   SUCCEEDED(rc)
           && (c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 's':   // storage controller name
            {
                if (ValueUnion.psz)
                    pszCtl = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'p':   // port
            {
                port = ValueUnion.u32;
                break;
            }

            case 'd':   // device
            {
                device = ValueUnion.u32;
                break;
            }

            case 'm':   // medium <none|emptydrive|uuid|filename|host:<drive>>
            {
                if (ValueUnion.psz)
                    pszMedium = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 't':   // type <dvddrive|hdd|fdd>
            {
                if (ValueUnion.psz)
                    pszType = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'h':   // passthrough <on|off>
            {
                if (ValueUnion.psz)
                    pszPassThrough = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'f':   // force unmount medium during runtime
            {
                fForceUnmount = true;
                break;
            }

            default:
            {
                errorGetOpt(USAGE_STORAGEATTACH, c, &ValueUnion);
                rc = E_FAIL;
                break;
            }
        }
    }

    if (FAILED(rc))
        return 1;

    if (!pszCtl)
        return errorSyntax(USAGE_STORAGEATTACH, "Storage controller name not specified");
    if (port == ~0U)
        return errorSyntax(USAGE_STORAGEATTACH, "Port not specified");
    if (device == ~0U)
        return errorSyntax(USAGE_STORAGEATTACH, "Device not specified");

    /* get the virtualbox system properties */
    CHECK_ERROR_RET(a->virtualBox, COMGETTER(SystemProperties)(systemProperties.asOutParam()), 1);

    /* try to find the given machine */
    CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()), 1);

    /* open a session for the VM (new or shared) */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), 1);
    SessionType_T st;
    CHECK_ERROR_RET(a->session, COMGETTER(Type)(&st), 1);
    bool fRunTime = (st == SessionType_Shared);

    if (fRunTime && !RTStrICmp(pszType, "hdd"))
    {
        errorArgument("Hard disk drives cannot be changed while the VM is running\n");
        goto leave;
    }

    if (fRunTime && !RTStrICmp(pszMedium, "none"))
    {
        errorArgument("Drives cannot be removed while the VM is running\n");
        goto leave;
    }

    if (fRunTime && pszPassThrough)
    {
        errorArgument("Drive passthrough state can't be changed while the VM is running\n");
        goto leave;
    }

    /* get the mutable session machine */
    a->session->COMGETTER(Machine)(machine.asOutParam());

    /* check if the storage controller is present */
    rc = machine->GetStorageControllerByName(Bstr(pszCtl).raw(),
                                             storageCtl.asOutParam());
    if (FAILED(rc))
    {
        errorSyntax(USAGE_STORAGEATTACH, "Couldn't find the controller with the name: '%s'\n", pszCtl);
        goto leave;
    }

    /* for sata controller check if the port count is big enough
     * to accommodate the current port which is being assigned
     * else just increase the port count
     */
    {
        ULONG ulPortCount = 0;
        ULONG ulMaxPortCount = 0;

        CHECK_ERROR(storageCtl, COMGETTER(MaxPortCount)(&ulMaxPortCount));
        CHECK_ERROR(storageCtl, COMGETTER(PortCount)(&ulPortCount));

        if (   (ulPortCount != ulMaxPortCount)
            && (port >= ulPortCount)
            && (port < ulMaxPortCount))
            CHECK_ERROR(storageCtl, COMSETTER(PortCount)(port + 1));
    }

    if (!RTStrICmp(pszMedium, "none"))
    {
        CHECK_ERROR(machine, DetachDevice(Bstr(pszCtl).raw(), port, device));
    }
    else if (!RTStrICmp(pszMedium, "emptydrive"))
    {
        if (fRunTime)
        {
            ComPtr<IMediumAttachment> mediumAttachment;
            DeviceType_T deviceType = DeviceType_Null;
            rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(), port, device,
                                              mediumAttachment.asOutParam());
            if (SUCCEEDED(rc))
            {
                mediumAttachment->COMGETTER(Type)(&deviceType);

                if (   (deviceType == DeviceType_DVD)
                    || (deviceType == DeviceType_Floppy))
                {
                    /* just unmount the floppy/dvd */
                    CHECK_ERROR(machine, MountMedium(Bstr(pszCtl).raw(),
                                                     port,
                                                     device,
                                                     NULL,
                                                     fForceUnmount));
                }
            }

            if (   FAILED(rc)
                || !(   deviceType == DeviceType_DVD
                     || deviceType == DeviceType_Floppy))
            {
                errorArgument("No DVD/Floppy Drive attached to the controller '%s'"
                              "at the port: %u, device: %u", pszCtl, port, device);
                goto leave;
            }

        }
        else
        {
            StorageBus_T storageBus = StorageBus_Null;
            DeviceType_T deviceType = DeviceType_Null;
            com::SafeArray <DeviceType_T> saDeviceTypes;
            ULONG driveCheck = 0;

            /* check if the device type is supported by the controller */
            CHECK_ERROR(storageCtl, COMGETTER(Bus)(&storageBus));
            CHECK_ERROR(systemProperties, GetDeviceTypesForStorageBus(storageBus, ComSafeArrayAsOutParam(saDeviceTypes)));
            for (size_t i = 0; i < saDeviceTypes.size(); ++ i)
            {
                if (   (saDeviceTypes[i] == DeviceType_DVD)
                    || (saDeviceTypes[i] == DeviceType_Floppy))
                    driveCheck++;
            }

            if (!driveCheck)
            {
                errorArgument("The Attachment is not supported by the Storage Controller: '%s'", pszCtl);
                goto leave;
            }

            if (storageBus == StorageBus_Floppy)
                deviceType = DeviceType_Floppy;
            else
                deviceType = DeviceType_DVD;

            /* attach a empty floppy/dvd drive after removing previous attachment */
            machine->DetachDevice(Bstr(pszCtl).raw(), port, device);
            CHECK_ERROR(machine, AttachDevice(Bstr(pszCtl).raw(), port, device,
                                              deviceType, NULL));
        }
    }
    else
    {
        {
            /*
             * try to determine the type of the drive from the
             * storage controller chipset, the attachment and
             * the medium being attached
             */
            StorageControllerType_T ctlType = StorageControllerType_Null;
            CHECK_ERROR(storageCtl, COMGETTER(ControllerType)(&ctlType));
            if (ctlType == StorageControllerType_I82078)
            {
                /*
                 * floppy controller found so lets assume the medium
                 * given by the user is also a floppy image or floppy
                 * host drive
                 */
                pszType = "fdd";
            }
            else
            {
                /*
                 * for SATA/SCSI/IDE it is hard to tell if it is a harddisk or
                 * a dvd being attached so lets check if the medium attachment
                 * and the medium, both are of same type. if yes then we are
                 * sure of its type and don't need the user to enter it manually
                 * else ask the user for the type.
                 */
                ComPtr<IMediumAttachment> mediumAttachement;
                rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                  device,
                                                  mediumAttachement.asOutParam());
                if (SUCCEEDED(rc))
                {
                    DeviceType_T deviceType;
                    mediumAttachement->COMGETTER(Type)(&deviceType);

                    if (deviceType == DeviceType_DVD)
                    {
                        ComPtr<IMedium> dvdMedium;
                        rc = a->virtualBox->FindMedium(Bstr(pszMedium).raw(),
                                                       DeviceType_DVD,
                                                       dvdMedium.asOutParam());
                        if (dvdMedium)
                            /*
                             * ok so the medium and attachment both are DVD's so it is
                             * safe to assume that we are dealing with a DVD here
                             */
                            pszType = "dvddrive";
                    }
                    else if (deviceType == DeviceType_HardDisk)
                    {
                        ComPtr<IMedium> hardDisk;
                        rc = a->virtualBox->FindMedium(Bstr(pszMedium).raw(),
                                                       DeviceType_HardDisk,
                                                       hardDisk.asOutParam());
                        if (hardDisk)
                            /*
                             * ok so the medium and attachment both are hdd's so it is
                             * safe to assume that we are dealing with a hdd here
                             */
                            pszType = "hdd";
                    }
                }
            }
            /* for all other cases lets ask the user what type of drive it is */
        }

        if (!pszType)
        {
            errorSyntax(USAGE_STORAGEATTACH, "Argument --type not specified\n");
            goto leave;
        }

        /* check if the device type is supported by the controller */
        {
            StorageBus_T storageBus = StorageBus_Null;
            com::SafeArray <DeviceType_T> saDeviceTypes;

            CHECK_ERROR(storageCtl, COMGETTER(Bus)(&storageBus));
            CHECK_ERROR(systemProperties, GetDeviceTypesForStorageBus(storageBus, ComSafeArrayAsOutParam(saDeviceTypes)));
            if (SUCCEEDED(rc))
            {
                ULONG driveCheck = 0;
                for (size_t i = 0; i < saDeviceTypes.size(); ++ i)
                {
                    if (   !RTStrICmp(pszType, "dvddrive")
                        && (saDeviceTypes[i] == DeviceType_DVD))
                        driveCheck++;

                    if (   !RTStrICmp(pszType, "hdd")
                        && (saDeviceTypes[i] == DeviceType_HardDisk))
                        driveCheck++;

                    if (   !RTStrICmp(pszType, "fdd")
                        && (saDeviceTypes[i] == DeviceType_Floppy))
                        driveCheck++;
                }
                if (!driveCheck)
                {
                    errorArgument("The Attachment is not supported by the Storage Controller: '%s'", pszCtl);
                    goto leave;
                }
            }
            else
                goto leave;
        }

        if (!RTStrICmp(pszType, "dvddrive"))
        {
            ComPtr<IMedium> dvdMedium;

            if (!fRunTime)
            {
                ComPtr<IMediumAttachment> mediumAttachement;

                /* check if there is a dvd drive at the given location, if not attach one */
                rc = machine->GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                  device,
                                                  mediumAttachement.asOutParam());
                if (SUCCEEDED(rc))
                {
                    DeviceType_T deviceType;
                    mediumAttachement->COMGETTER(Type)(&deviceType);

                    if (deviceType != DeviceType_DVD)
                    {
                        machine->DetachDevice(Bstr(pszCtl).raw(), port, device);
                        rc = machine->AttachDevice(Bstr(pszCtl).raw(), port,
                                                   device, DeviceType_DVD, NULL);
                    }
                }
                else
                {
                    rc = machine->AttachDevice(Bstr(pszCtl).raw(), port,
                                               device, DeviceType_DVD, NULL);
                }
            }

            /* Attach/Detach the dvd medium now */
            do
            {
                /* host drive? */
                if (!RTStrNICmp(pszMedium, "host:", 5))
                {
                    ComPtr<IHost> host;
                    CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                    rc = host->FindHostDVDDrive(Bstr(pszMedium + 5).raw(),
                                                dvdMedium.asOutParam());
                    if (!dvdMedium)
                    {
                        /* 2nd try: try with the real name, important on Linux+libhal */
                        char szPathReal[RTPATH_MAX];
                        if (RT_FAILURE(RTPathReal(pszMedium + 5, szPathReal, sizeof(szPathReal))))
                        {
                            errorArgument("Invalid host DVD drive name \"%s\"", pszMedium + 5);
                            rc = E_FAIL;
                            break;
                        }
                        rc = host->FindHostDVDDrive(Bstr(szPathReal).raw(),
                                                    dvdMedium.asOutParam());
                        if (!dvdMedium)
                        {
                            errorArgument("Invalid host DVD drive name \"%s\"", pszMedium + 5);
                            rc = E_FAIL;
                            break;
                        }
                    }
                }
                else
                {
                    rc = a->virtualBox->FindMedium(Bstr(pszMedium).raw(),
                                                   DeviceType_DVD,
                                                   dvdMedium.asOutParam());
                    if (FAILED(rc) || !dvdMedium)
                    {
                        /* not registered, do that on the fly */
                        Bstr emptyUUID;
                        CHECK_ERROR(a->virtualBox,
                                    OpenMedium(Bstr(pszMedium).raw(),
                                               DeviceType_DVD,
                                               AccessMode_ReadWrite,
                                               dvdMedium.asOutParam()));
                    }
                    if (!dvdMedium)
                    {
                        errorArgument("Invalid UUID or filename \"%s\"", pszMedium);
                        rc = E_FAIL;
                        break;
                    }
                }
            } while (0);

            if (dvdMedium)
            {
                CHECK_ERROR(machine, MountMedium(Bstr(pszCtl).raw(), port,
                                                 device, dvdMedium,
                                                 fForceUnmount));
            }
        }
        else if (   !RTStrICmp(pszType, "hdd")
                 && !fRunTime)
        {
            ComPtr<IMediumAttachment> mediumAttachement;

            /* if there is anything attached at the given location, remove it */
            machine->DetachDevice(Bstr(pszCtl).raw(), port, device);

            /* first guess is that it's a UUID */
            ComPtr<IMedium> hardDisk;
            rc = a->virtualBox->FindMedium(Bstr(pszMedium).raw(),
                                           DeviceType_HardDisk,
                                           hardDisk.asOutParam());

            /* not successful? Then it must be a filename */
            if (!hardDisk)
            {
                /* open the new hard disk object */
                CHECK_ERROR(a->virtualBox,
                            OpenMedium(Bstr(pszMedium).raw(),
                                       DeviceType_HardDisk,
                                       AccessMode_ReadWrite,
                                       hardDisk.asOutParam()));
            }

            if (hardDisk)
            {
                CHECK_ERROR(machine, AttachDevice(Bstr(pszCtl).raw(), port,
                                                  device, DeviceType_HardDisk,
                                                  hardDisk));
            }
            else
            {
                errorArgument("Invalid UUID or filename \"%s\"", pszMedium);
                rc = E_FAIL;
            }
        }
        else if (!RTStrICmp(pszType, "fdd"))
        {
            Bstr uuid;
            ComPtr<IMedium> floppyMedium;
            ComPtr<IMediumAttachment> floppyAttachment;
            machine->GetMediumAttachment(Bstr(pszCtl).raw(), port, device,
                                         floppyAttachment.asOutParam());

            if (   !fRunTime
                && !floppyAttachment)
                CHECK_ERROR(machine, AttachDevice(Bstr(pszCtl).raw(), port,
                                                  device, DeviceType_Floppy,
                                                  NULL));

            /* host drive? */
            if (!RTStrNICmp(pszMedium, "host:", 5))
            {
                ComPtr<IHost> host;

                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                rc = host->FindHostFloppyDrive(Bstr(pszMedium + 5).raw(),
                                               floppyMedium.asOutParam());
                if (!floppyMedium)
                {
                    errorArgument("Invalid host floppy drive name \"%s\"", pszMedium + 5);
                    rc = E_FAIL;
                }
            }
            else
            {
                /* first assume it's a UUID */
                rc = a->virtualBox->FindMedium(Bstr(pszMedium).raw(),
                                               DeviceType_Floppy,
                                               floppyMedium.asOutParam());
                if (FAILED(rc) || !floppyMedium)
                {
                    /* not registered, do that on the fly */
                    Bstr emptyUUID;
                    CHECK_ERROR(a->virtualBox,
                                 OpenMedium(Bstr(pszMedium).raw(),
                                            DeviceType_Floppy,
                                            AccessMode_ReadWrite,
                                            floppyMedium.asOutParam()));
                }

                if (!floppyMedium)
                {
                    errorArgument("Invalid UUID or filename \"%s\"", pszMedium);
                    rc = E_FAIL;
                }
            }

            if (floppyMedium)
            {
                CHECK_ERROR(machine, MountMedium(Bstr(pszCtl).raw(), port,
                                                 device,
                                                 floppyMedium,
                                                 fForceUnmount));
            }
        }
        else
        {
            errorArgument("Invalid --type argument '%s'", pszType);
            rc = E_FAIL;
        }
    }

    if (   pszPassThrough
        && (SUCCEEDED(rc)))
    {
        ComPtr<IMediumAttachment> mattach;

        CHECK_ERROR(machine, GetMediumAttachment(Bstr(pszCtl).raw(), port,
                                                 device, mattach.asOutParam()));

        if (SUCCEEDED(rc))
        {
            if (!RTStrICmp(pszPassThrough, "on"))
            {
                CHECK_ERROR(machine, PassthroughDevice(Bstr(pszCtl).raw(),
                                                       port, device, TRUE));
            }
            else if (!RTStrICmp(pszPassThrough, "off"))
            {
                CHECK_ERROR(machine, PassthroughDevice(Bstr(pszCtl).raw(),
                                                       port, device, FALSE));
            }
            else
            {
                errorArgument("Invalid --passthrough argument '%s'", pszPassThrough);
                rc = E_FAIL;
            }
        }
        else
        {
            errorArgument("Couldn't find the controller attachment for the controller '%s'\n", pszCtl);
            rc = E_FAIL;
        }
    }

    /* commit changes */
    if (SUCCEEDED(rc))
        CHECK_ERROR(machine, SaveSettings());

leave:
    /* it's important to always close sessions */
    a->session->UnlockMachine();

    return SUCCEEDED(rc) ? 0 : 1;
}


static const RTGETOPTDEF g_aStorageControllerOptions[] =
{
    { "--name",             'n', RTGETOPT_REQ_STRING },
    { "--add",              'a', RTGETOPT_REQ_STRING },
    { "--controller",       'c', RTGETOPT_REQ_STRING },
    { "--sataideemulation", 'e', RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_INDEX },
    { "--sataportcount",    'p', RTGETOPT_REQ_UINT32 },
    { "--remove",           'r', RTGETOPT_REQ_NOTHING },
    { "--hostiocache",      'i', RTGETOPT_REQ_STRING },
};

int handleStorageController(HandlerArg *a)
{
    int               c;
    HRESULT           rc             = S_OK;
    const char       *pszCtl         = NULL;
    const char       *pszBusType     = NULL;
    const char       *pszCtlType     = NULL;
    const char       *pszHostIOCache = NULL;
    ULONG             satabootdev    = ~0U;
    ULONG             sataidedev     = ~0U;
    ULONG             sataportcount  = ~0U;
    bool              fRemoveCtl     = false;
    ComPtr<IMachine>  machine;
    RTGETOPTUNION     ValueUnion;
    RTGETOPTSTATE     GetState;

    if (a->argc < 4)
        return errorSyntax(USAGE_STORAGECONTROLLER, "Too few parameters");

    RTGetOptInit (&GetState, a->argc, a->argv, g_aStorageControllerOptions,
                  RT_ELEMENTS(g_aStorageControllerOptions), 1, RTGETOPTINIT_FLAGS_NO_STD_OPTS);

    while (   SUCCEEDED(rc)
           && (c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'n':   // controller name
            {
                if (ValueUnion.psz)
                    pszCtl = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'a':   // controller bus type <ide/sata/scsi/floppy>
            {
                if (ValueUnion.psz)
                    pszBusType = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'c':   // controller <lsilogic/buslogic/intelahci/piix3/piix4/ich6/i82078>
            {
                if (ValueUnion.psz)
                    pszCtlType = ValueUnion.psz;
                else
                    rc = E_FAIL;
                break;
            }

            case 'e':   // sataideemulation
            {
                satabootdev = GetState.uIndex;
                sataidedev = ValueUnion.u32;
                break;
            }

            case 'p':   // sataportcount
            {
                sataportcount = ValueUnion.u32;
                break;
            }

            case 'r':   // remove controller
            {
                fRemoveCtl = true;
                break;
            }

            case 'i':
            {
                pszHostIOCache = ValueUnion.psz;
                break;
            }

            default:
            {
                errorGetOpt(USAGE_STORAGECONTROLLER, c, &ValueUnion);
                rc = E_FAIL;
                break;
            }
        }
    }

    if (FAILED(rc))
        return 1;

    /* try to find the given machine */
    CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()), 1);

    /* open a session for the VM */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Write), 1);

    /* get the mutable session machine */
    a->session->COMGETTER(Machine)(machine.asOutParam());

    if (!pszCtl)
    {
        /* it's important to always close sessions */
        a->session->UnlockMachine();
        errorSyntax(USAGE_STORAGECONTROLLER, "Storage controller name not specified\n");
        return 1;
    }

    if (fRemoveCtl)
    {
        com::SafeIfaceArray<IMediumAttachment> mediumAttachments;

        CHECK_ERROR(machine,
                     GetMediumAttachmentsOfController(Bstr(pszCtl).raw(),
                                                      ComSafeArrayAsOutParam(mediumAttachments)));
        for (size_t i = 0; i < mediumAttachments.size(); ++ i)
        {
            ComPtr<IMediumAttachment> mediumAttach = mediumAttachments[i];
            LONG port = 0;
            LONG device = 0;

            CHECK_ERROR(mediumAttach, COMGETTER(Port)(&port));
            CHECK_ERROR(mediumAttach, COMGETTER(Device)(&device));
            CHECK_ERROR(machine, DetachDevice(Bstr(pszCtl).raw(), port, device));
        }

        if (SUCCEEDED(rc))
            CHECK_ERROR(machine, RemoveStorageController(Bstr(pszCtl).raw()));
        else
            errorArgument("Can't detach the devices connected to '%s' Controller\n"
                          "and thus its removal failed.", pszCtl);
    }
    else
    {
        if (pszBusType)
        {
            ComPtr<IStorageController> ctl;

            if (!RTStrICmp(pszBusType, "ide"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_IDE,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "sata"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_SATA,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "scsi"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_SCSI,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "floppy"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_Floppy,
                                                          ctl.asOutParam()));
            }
            else if (!RTStrICmp(pszBusType, "sas"))
            {
                CHECK_ERROR(machine, AddStorageController(Bstr(pszCtl).raw(),
                                                          StorageBus_SAS,
                                                          ctl.asOutParam()));
            }
            else
            {
                errorArgument("Invalid --add argument '%s'", pszBusType);
                rc = E_FAIL;
            }
        }

        if (   pszCtlType
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszCtlType, "lsilogic"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_LsiLogic));
                }
                else if (!RTStrICmp(pszCtlType, "buslogic"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_BusLogic));
                }
                else if (!RTStrICmp(pszCtlType, "intelahci"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_IntelAhci));
                }
                else if (!RTStrICmp(pszCtlType, "piix3"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_PIIX3));
                }
                else if (!RTStrICmp(pszCtlType, "piix4"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_PIIX4));
                }
                else if (!RTStrICmp(pszCtlType, "ich6"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_ICH6));
                }
                else if (!RTStrICmp(pszCtlType, "i82078"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_I82078));
                }
                else if (!RTStrICmp(pszCtlType, "lsilogicsas"))
                {
                    CHECK_ERROR(ctl, COMSETTER(ControllerType)(StorageControllerType_LsiLogicSas));
                }
                else
                {
                    errorArgument("Invalid --type argument '%s'", pszCtlType);
                    rc = E_FAIL;
                }
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

        if (   (sataportcount != ~0U)
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(ctl, COMSETTER(PortCount)(sataportcount));
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

        if (   (sataidedev != ~0U)
            && (satabootdev != ~0U)
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                CHECK_ERROR(ctl, SetIDEEmulationPort(satabootdev, sataidedev));
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }

        if (   pszHostIOCache
            && SUCCEEDED(rc))
        {
            ComPtr<IStorageController> ctl;

            CHECK_ERROR(machine, GetStorageControllerByName(Bstr(pszCtl).raw(),
                                                            ctl.asOutParam()));

            if (SUCCEEDED(rc))
            {
                if (!RTStrICmp(pszHostIOCache, "on"))
                {
                    CHECK_ERROR(ctl, COMSETTER(UseHostIOCache)(TRUE));
                }
                else if (!RTStrICmp(pszHostIOCache, "off"))
                {
                    CHECK_ERROR(ctl, COMSETTER(UseHostIOCache)(FALSE));
                }
                else
                {
                    errorArgument("Invalid --hostiocache argument '%s'", pszHostIOCache);
                    rc = E_FAIL;
                }
            }
            else
            {
                errorArgument("Couldn't find the controller with the name: '%s'\n", pszCtl);
                rc = E_FAIL;
            }
        }
    }

    /* commit changes */
    if (SUCCEEDED(rc))
        CHECK_ERROR(machine, SaveSettings());

    /* it's important to always close sessions */
    a->session->UnlockMachine();

    return SUCCEEDED(rc) ? 0 : 1;
}

#endif /* !VBOX_ONLY_DOCS */

