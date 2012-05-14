
/* $Id$ */
/** @file
 * VBoxModAPIMonitor - API monitor module for detecting host isolation.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifndef VBOX_ONLY_DOCS
# include <iprt/message.h>
# include <VBox/com/errorprint.h>
#endif /* !VBOX_ONLY_DOCS */

#include "VBoxWatchdogInternal.h"

using namespace com;

#define VBOX_MOD_APIMON_NAME "apimon"

/**
 * The module's RTGetOpt-IDs for the command line.
 */
enum GETOPTDEF_APIMON
{
    GETOPTDEF_APIMON_ISLN_RESPONSE = 3000,
    GETOPTDEF_APIMON_ISLN_TIMEOUT,
    GETOPTDEF_APIMON_GROUPS
};

/**
 * The module's command line arguments.
 */
static const RTGETOPTDEF g_aAPIMonitorOpts[] = {
    { "--apimon-isln-response",  GETOPTDEF_APIMON_ISLN_RESPONSE,  RTGETOPT_REQ_STRING },
    { "--apimon-isln-timeout",   GETOPTDEF_APIMON_ISLN_TIMEOUT,   RTGETOPT_REQ_UINT32 },
    { "--apimon-groups",         GETOPTDEF_APIMON_GROUPS,         RTGETOPT_REQ_STRING }
};

enum APIMON_RESPONSE
{
    /** Unknown / unhandled response. */
    APIMON_RESPONSE_NONE       = 0,
    /** Does a hard power off. */
    APIMON_RESPONSE_POWEROFF   = 200,
    /** Tries to save the current machine state. */
    APIMON_RESPONSE_SAVE       = 250,
    /** Tries to shut down all running VMs in
     *  a gentle manner. */
    APIMON_RESPONSE_SHUTDOWN   = 300
};

/** The VM group(s) the API monitor handles. If none, all VMs get handled. */
static mapGroups                    g_vecAPIMonGroups;
static APIMON_RESPONSE              g_enmAPIMonIslnResp     = APIMON_RESPONSE_NONE;
static unsigned long                g_ulAPIMonIslnTimeoutMS = 0;
static Bstr                         g_strAPIMonIslnLastBeat;
static uint64_t                     g_uAPIMonIslnLastBeatMS = 0;

static int apimonResponseToEnum(const char *pszResponse, APIMON_RESPONSE *pResp)
{
    AssertPtrReturn(pszResponse, VERR_INVALID_POINTER);
    AssertPtrReturn(pResp, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    if (   !RTStrICmp(pszResponse, "poweroff")
        || !RTStrICmp(pszResponse, "powerdown"))
    {
        *pResp = APIMON_RESPONSE_POWEROFF;
    }
    else if (   !RTStrICmp(pszResponse, "shutdown")
             || !RTStrICmp(pszResponse, "shutoff"))
    {
        *pResp = APIMON_RESPONSE_SHUTDOWN;
    }
    else if (!RTStrICmp(pszResponse, "save"))
    {
        *pResp = APIMON_RESPONSE_SAVE;
    }
    else
    {
        *pResp = APIMON_RESPONSE_NONE;
        rc = VERR_INVALID_PARAMETER;
    }

    return rc;
}

static const char* apimonResponseToStr(APIMON_RESPONSE enmResp)
{
    if (APIMON_RESPONSE_POWEROFF == enmResp)
        return "powering off";
    else if (APIMON_RESPONSE_SHUTDOWN == enmResp)
        return "shutting down";
    else if (APIMON_RESPONSE_SAVE == enmResp)
        return "saving state";
    else if (APIMON_RESPONSE_NONE == enmResp)
        return "none";

    return "unknown";
}

/* Copied from VBoxManageInfo.cpp. */
static const char *apimonMachineStateToName(MachineState_T machineState, bool fShort)
{
    switch (machineState)
    {
        case MachineState_PoweredOff:
            return fShort ? "poweroff"             : "powered off";
        case MachineState_Saved:
            return "saved";
        case MachineState_Aborted:
            return "aborted";
        case MachineState_Teleported:
            return "teleported";
        case MachineState_Running:
            return "running";
        case MachineState_Paused:
            return "paused";
        case MachineState_Stuck:
            return fShort ? "gurumeditation"       : "guru meditation";
        case MachineState_LiveSnapshotting:
            return fShort ? "livesnapshotting"     : "live snapshotting";
        case MachineState_Teleporting:
            return "teleporting";
        case MachineState_Starting:
            return "starting";
        case MachineState_Stopping:
            return "stopping";
        case MachineState_Saving:
            return "saving";
        case MachineState_Restoring:
            return "restoring";
        case MachineState_TeleportingPausedVM:
            return fShort ? "teleportingpausedvm"  : "teleporting paused vm";
        case MachineState_TeleportingIn:
            return fShort ? "teleportingin"        : "teleporting (incoming)";
        case MachineState_RestoringSnapshot:
            return fShort ? "restoringsnapshot"    : "restoring snapshot";
        case MachineState_DeletingSnapshot:
            return fShort ? "deletingsnapshot"     : "deleting snapshot";
        case MachineState_DeletingSnapshotOnline:
            return fShort ? "deletingsnapshotlive" : "deleting snapshot live";
        case MachineState_DeletingSnapshotPaused:
            return fShort ? "deletingsnapshotlivepaused" : "deleting snapshot live paused";
        case MachineState_SettingUp:
            return fShort ? "settingup"           : "setting up";
        default:
            break;
    }
    return "unknown";
}

static int apimonMachineControl(const Bstr &strUuid, PVBOXWATCHDOG_MACHINE pMachine,
                                APIMON_RESPONSE enmResp, unsigned long ulTimeout)
{
    /** @todo Add other commands (with enmResp) here. */
    AssertPtrReturn(pMachine, VERR_INVALID_POINTER);

    serviceLogVerbose(("apimon: Triggering \"%s\" (%RU32ms timeout) for machine \"%ls\"\n",
                      apimonResponseToStr(enmResp), ulTimeout, strUuid.raw()));

    if (   enmResp == APIMON_RESPONSE_NONE
        || g_fDryrun)
        return VINF_SUCCESS; /* Nothing to do. */

    HRESULT rc;
    ComPtr <IMachine> machine;
    CHECK_ERROR_RET(g_pVirtualBox, FindMachine(strUuid.raw(),
                                               machine.asOutParam()), VERR_NOT_FOUND);

    /* Open a session for the VM. */
    CHECK_ERROR_RET(machine, LockMachine(g_pSession, LockType_Shared), VERR_ACCESS_DENIED);

    do
    {

        /* Get the associated console. */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(g_pSession, COMGETTER(Console)(console.asOutParam()));

        /* Query the machine's state to avoid unnecessary IPC. */
        MachineState_T machineState;
        CHECK_ERROR_BREAK(console, COMGETTER(State)(&machineState));
        if (   machineState == MachineState_Running
            || machineState == MachineState_Paused)
        {
            ComPtr<IProgress> progress;

            switch (enmResp)
            {
                case APIMON_RESPONSE_POWEROFF:
                    CHECK_ERROR_BREAK(console, PowerDown(progress.asOutParam()));
                    serviceLogVerbose(("apimon: Waiting for powering off machine \"%ls\" ...\n",
                                       strUuid.raw()));
                    progress->WaitForCompletion(ulTimeout);
                    CHECK_PROGRESS_ERROR(progress, ("Failed to power off machine \"%ls\"",
                                         strUuid.raw()));
                    break;

                case APIMON_RESPONSE_SAVE:
                {
                    /* First pause so we don't trigger a live save which needs more time/resources. */
                    bool fPaused = false;
                    rc = console->Pause();
                    if (FAILED(rc))
                    {
                        bool fError = true;
                        if (rc == VBOX_E_INVALID_VM_STATE)
                        {
                            /* Check if we are already paused. */
                            MachineState_T machineState;
                            CHECK_ERROR_BREAK(console, COMGETTER(State)(&machineState));
                            /* The error code was lost by the previous instruction. */
                            rc = VBOX_E_INVALID_VM_STATE;
                            if (machineState != MachineState_Paused)
                            {
                                serviceLog("apimon: Machine \"%s\" in invalid state %d -- %s\n",
                                           strUuid.raw(), machineState, apimonMachineStateToName(machineState, false));
                            }
                            else
                            {
                                fError = false;
                                fPaused = true;
                            }
                        }
                        if (fError)
                            break;
                    }

                    serviceLogVerbose(("apimon: Waiting for saving state of machine \"%ls\" ...\n",
                                       strUuid.raw()));

                    ComPtr<IProgress> progress;
                    CHECK_ERROR(console, SaveState(progress.asOutParam()));
                    if (FAILED(rc))
                    {
                        if (!fPaused)
                            console->Resume();
                        break;
                    }

                    progress->WaitForCompletion(ulTimeout);
                    CHECK_PROGRESS_ERROR(progress, ("Failed to save machine state of machine \"%ls\"",
                                         strUuid.raw()));
                    if (FAILED(rc))
                    {
                        if (!fPaused)
                            console->Resume();
                    }

                    break;
                }

                case APIMON_RESPONSE_SHUTDOWN:
                    CHECK_ERROR_BREAK(console, PowerButton());
                    serviceLogVerbose(("apimon: Waiting for shutdown of machine \"%ls\" ...\n",
                                       strUuid.raw()));
                    progress->WaitForCompletion(ulTimeout);
                    CHECK_PROGRESS_ERROR(progress, ("Failed to shutdown machine \"%ls\"",
                                         strUuid.raw()));
                    break;

                default:
                    AssertMsgFailed(("Response %d not implemented", enmResp));
                    break;
            }
        }
        else
            serviceLog("apimon: Machine \"%s\" is in invalid state \"%s\" (%d) for triggering \"%s\"\n",
                       strUuid.raw(), apimonMachineStateToName(machineState, false), machineState,
                       apimonResponseToStr(enmResp));
    } while (0);

    /* Unlock the machine again. */
    g_pSession->UnlockMachine();

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_COM_IPRT_ERROR;
}

static bool apimonHandleVM(const PVBOXWATCHDOG_MACHINE pMachine)
{
    bool fHandleVM = false;

    try
    {
        mapGroupsIterConst itVMGroup = pMachine->groups.begin();
        while (   itVMGroup != pMachine->groups.end()
               && !fHandleVM)
        {
            mapGroupsIterConst itInGroup = g_vecAPIMonGroups.find(itVMGroup->first);
            if (itInGroup != g_vecAPIMonGroups.end())
                fHandleVM = true;

            itVMGroup++;
        }
    }
    catch (...)
    {
        AssertFailed();
    }

    return fHandleVM;
}

static int apimonTrigger(APIMON_RESPONSE enmResp)
{
    int rc = VINF_SUCCESS;

    bool fAllGroups = g_vecAPIMonGroups.empty();
    mapVMIter it = g_mapVM.begin();

    if (it == g_mapVM.end())
    {
        serviceLog("apimon: No machines in list, skipping ...\n");
        return rc;
    }

    while (it != g_mapVM.end())
    {
        bool fHandleVM = fAllGroups;
        try
        {
            if (!fHandleVM)
                fHandleVM = apimonHandleVM(&it->second);

            if (fHandleVM)
            {
                int rc2 = apimonMachineControl(it->first /* Uuid */,
                                               &it->second /* Machine */, enmResp, 30 * 1000 /* 30s timeout */);
                if (RT_FAILURE(rc2))
                    serviceLog("apimon: Controlling machine \"%ls\" (action: %s) failed with rc=%Rrc",
                               it->first.raw(), apimonResponseToStr(enmResp), rc);

                if (RT_SUCCESS(rc))
                    rc = rc2; /* Store original error. */
                /* Keep going. */
            }
        }
        catch (...)
        {
            AssertFailed();
        }

        it++;
    }

    return rc;
}

/* Callbacks. */
static DECLCALLBACK(int) VBoxModAPIMonitorPreInit(void)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModAPIMonitorOption(int argc, char **argv)
{
    if (!argc) /* Take a shortcut. */
        return -1;

    AssertPtrReturn(argv, VERR_INVALID_PARAMETER);

    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, argc, argv,
                          g_aAPIMonitorOpts, RT_ELEMENTS(g_aAPIMonitorOpts),
                          0 /* First */, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return rc;

    rc = 0; /* Set default parsing result to valid. */

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case GETOPTDEF_APIMON_ISLN_RESPONSE:
                rc = apimonResponseToEnum(ValueUnion.psz, &g_enmAPIMonIslnResp);
                if (RT_FAILURE(rc))
                    rc = -1; /* Option unknown. */
                break;

            case GETOPTDEF_APIMON_ISLN_TIMEOUT:
                g_ulAPIMonIslnTimeoutMS = ValueUnion.u32;
                if (g_ulAPIMonIslnTimeoutMS < 1000) /* Don't allow timeouts < 1s. */
                    g_ulAPIMonIslnTimeoutMS = 1000;
                break;

            case GETOPTDEF_APIMON_GROUPS:
            {
                rc = groupAdd(g_vecAPIMonGroups, ValueUnion.psz, 0 /* Flags */);
                if (RT_FAILURE(rc))
                    rc = -1; /* Option unknown. */
                break;
            }

            default:
                rc = -1; /* We don't handle this option, skip. */
                break;
        }
    }

    return rc;
}

static DECLCALLBACK(int) VBoxModAPIMonitorInit(void)
{
    HRESULT rc = S_OK;

    do
    {
        Bstr strValue;

        /* Host isolation timeout (in ms). */
        if (!g_ulAPIMonIslnTimeoutMS) /* Not set by command line? */
        {
            CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("Watchdog/APIMonitor/IsolationTimeout").raw(),
                                                          strValue.asOutParam()));
            if (!strValue.isEmpty())
                g_ulAPIMonIslnTimeoutMS = Utf8Str(strValue).toUInt32();
        }
        if (!g_ulAPIMonIslnTimeoutMS) /* Still not set? Use a default. */
        {
            serviceLogVerbose(("apimon: API monitor isolation timeout not given, defaulting to 30s\n"));

            /* Default is 30 seconds timeout. */
            g_ulAPIMonIslnTimeoutMS = 30 * 1000;
        }

        /* VM groups to watch for. */
        if (g_vecAPIMonGroups.empty()) /* Not set by command line? */
        {
            CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("Watchdog/APIMonitor/Groups").raw(),
                                                          strValue.asOutParam()));
            if (!strValue.isEmpty())
            {
                int rc2 = groupAdd(g_vecAPIMonGroups, Utf8Str(strValue).c_str(), 0 /* Flags */);
                if (RT_FAILURE(rc2))
                    serviceLog("apimon: Warning: API monitor groups string invalid (%ls)\n", strValue.raw());
            }
        }

        /* Host isolation command response. */
        if (g_enmAPIMonIslnResp == APIMON_RESPONSE_NONE) /* Not set by command line? */
        {
            CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("Watchdog/APIMonitor/IsolationResponse").raw(),
                                                          strValue.asOutParam()));
            if (!strValue.isEmpty())
            {
                int rc2 = apimonResponseToEnum(Utf8Str(strValue).c_str(), &g_enmAPIMonIslnResp);
                if (RT_FAILURE(rc2))
                    serviceLog("apimon: Warning: API monitor response string invalid (%ls), defaulting to no action\n",
                               strValue.raw());
            }
        }
    } while (0);

    if (SUCCEEDED(rc))
    {
        g_uAPIMonIslnLastBeatMS = 0;
    }

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_COM_IPRT_ERROR; /* @todo Find a better rc! */
}

static DECLCALLBACK(int) VBoxModAPIMonitorMain(void)
{
    static uint64_t uLastRun = 0;
    uint64_t uNow = RTTimeProgramMilliTS();
    uint64_t uDelta = uNow - uLastRun;
    if (uDelta < 1000) /* Only check every second (or later). */
        return VINF_SUCCESS;
    uLastRun = uNow;

    int vrc = VINF_SUCCESS;
    HRESULT rc;

#ifdef DEBUG
    serviceLogVerbose(("apimon: Checking for API heartbeat (%RU64ms) ...\n",
                       g_ulAPIMonIslnTimeoutMS));
#endif

    do
    {
        Bstr strHeartbeat;
        CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("Watchdog/APIMonitor/Heartbeat").raw(),
                                                      strHeartbeat.asOutParam()));
        if (   SUCCEEDED(rc)
            && !strHeartbeat.isEmpty()
            && g_strAPIMonIslnLastBeat.compare(strHeartbeat, Bstr::CaseSensitive))
        {
            serviceLogVerbose(("apimon: API heartbeat received, resetting timeout\n"));

            g_uAPIMonIslnLastBeatMS = 0;
            g_strAPIMonIslnLastBeat = strHeartbeat;
        }
        else
        {
            g_uAPIMonIslnLastBeatMS += uDelta;
            if (g_uAPIMonIslnLastBeatMS > g_ulAPIMonIslnTimeoutMS)
            {
                serviceLogVerbose(("apimon: No API heartbeat within time received (%RU64ms)\n",
                                   g_ulAPIMonIslnTimeoutMS));

                vrc = apimonTrigger(g_enmAPIMonIslnResp);
                g_uAPIMonIslnLastBeatMS = 0;
            }
        }
    } while (0);

    if (FAILED(rc))
        vrc = VERR_COM_IPRT_ERROR;

    return vrc;
}

static DECLCALLBACK(int) VBoxModAPIMonitorStop(void)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) VBoxModAPIMonitorTerm(void)
{
}

static DECLCALLBACK(int) VBoxModAPIMonitorOnMachineRegistered(const Bstr &strUuid)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModAPIMonitorOnMachineUnregistered(const Bstr &strUuid)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModAPIMonitorOnMachineStateChanged(const Bstr &strUuid,
                                                                MachineState_T enmState)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModAPIMonitorOnServiceStateChanged(bool fAvailable)
{
    if (!fAvailable)
    {
        serviceLog(("apimon: VBoxSVC became unavailable, triggering action\n"));
        return apimonTrigger(g_enmAPIMonIslnResp);
    }
    return VINF_SUCCESS;
}

/**
 * The 'apimonitor' module description.
 */
VBOXMODULE g_ModAPIMonitor =
{
    /* pszName. */
    VBOX_MOD_APIMON_NAME,
    /* pszDescription. */
    "API monitor for host isolation detection",
    /* pszDepends. */
    NULL,
    /* uPriority. */
    0 /* Not used */,
    /* pszUsage. */
    " [--apimon-isln-response=<cmd>] [--apimon-isln-timeout=<ms>]\n"
    " [--apimon-groups=<string>]\n",
    /* pszOptions. */
    "--apimon-isln-response Sets the isolation response (shutdown VM).\n"
    "--apimon-isln-timeout  Sets the isolation timeout in ms (30s).\n"
    "--apimon-groups        Sets the VM groups for monitoring (none).\n",
    /* methods. */
    VBoxModAPIMonitorPreInit,
    VBoxModAPIMonitorOption,
    VBoxModAPIMonitorInit,
    VBoxModAPIMonitorMain,
    VBoxModAPIMonitorStop,
    VBoxModAPIMonitorTerm,
    /* callbacks. */
    VBoxModAPIMonitorOnMachineRegistered,
    VBoxModAPIMonitorOnMachineUnregistered,
    VBoxModAPIMonitorOnMachineStateChanged,
    VBoxModAPIMonitorOnServiceStateChanged
};

