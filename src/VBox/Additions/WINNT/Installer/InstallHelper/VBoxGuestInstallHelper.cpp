/* $Id$ */
/** @file
 * VBoxGuestInstallHelper - Various helper routines for Windows guest installer.
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

#include <windows.h>
#include <stdlib.h>
#include <Strsafe.h>
#include "exdll.h"

/* Required structures/defines of VBoxTray. */
#include "../../VBoxTray/VBoxTrayMsg.h"

HINSTANCE g_hInstance;
HWND g_hwndParent;

typedef DWORD (__stdcall *fnSfcFileException) (DWORD param1, PWCHAR param2, DWORD param3);
fnSfcFileException g_pfnSfcFileException = NULL;

#define VBOXINSTALLHELPER_EXPORT extern "C" void __declspec(dllexport)

/**
 * Pops (gets) a value from the internal NSIS stack.
 * Since the supplied popstring() method easily can cause buffer
 * overflows, use VBoxPopString() instead!
 *
 * @return  HRESULT
 * @param   pszDest     Pointer to pre-allocated string to store result.
 * @param   cchDest     Size (in characters) of pre-allocated string.
 */
static HRESULT VBoxPopString(TCHAR *pszDest, size_t cchDest)
{
    HRESULT hr = S_OK;
    if (!g_stacktop || !*g_stacktop)
        hr = ERROR_EMPTY;
    else
    {
        stack_t *pStack = (*g_stacktop);
        if (pStack)
        {
            hr = StringCchCopy(pszDest, cchDest, pStack->text);
            if (SUCCEEDED(hr))
            {
                *g_stacktop = pStack->next;
                GlobalFree((HGLOBAL)pStack);
            }
        }
    }
    return hr;
}

static HRESULT VBoxPopULong(ULONG *pulValue)
{
    HRESULT hr = S_OK;
    if (!g_stacktop || !*g_stacktop)
        hr = ERROR_EMPTY;
    else
    {
        stack_t *pStack = (*g_stacktop);
        if (pStack)
        {
            *pulValue = strtoul(pStack->text, NULL, 10 /* Base */);

            *g_stacktop = pStack->next;
            GlobalFree((HGLOBAL)pStack);
        }
    }
    return hr;
}

HANDLE VBoIPCConnect()
{
    HANDLE hPipe = NULL;
    while (1)
    {
        hPipe = CreateFile(VBOXTRAY_PIPE_IPC,   /* Pipe name. */
                           GENERIC_READ |       /* Read and write access. */
                           GENERIC_WRITE,
                           0,                   /* No sharing. */
                           NULL,                /* Default security attributes. */
                           OPEN_EXISTING,       /* Opens existing pipe. */
                           0,                   /* Default attributes. */
                           NULL);               /* No template file. */

        /* Break if the pipe handle is valid. */
        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        /* Exit if an error other than ERROR_PIPE_BUSY occurs. */
        if (GetLastError() != ERROR_PIPE_BUSY)
            return NULL;

        /* All pipe instances are busy, so wait for 20 seconds. */
        if (!WaitNamedPipe(VBOXTRAY_PIPE_IPC, 20000))
            return NULL;
    }

    /* The pipe connected; change to message-read mode. */
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    BOOL fSuccess = SetNamedPipeHandleState(hPipe,    /* Pipe handle. */
                                            &dwMode,  /* New pipe mode. */
                                            NULL,     /* Don't set maximum bytes. */
                                            NULL);    /* Don't set maximum time. */
    if (!fSuccess)
        return NULL;
    return hPipe;
}

void VBoxIPCDisconnect(HANDLE hPipe)
{
    CloseHandle(hPipe);
}

HRESULT VBoxIPCWriteMessage(HANDLE hPipe, BYTE *pMessage, DWORD cbMessage)
{
    HRESULT hr = S_OK;
    DWORD cbWritten = 0;
    if (!WriteFile(hPipe, pMessage, cbMessage - cbWritten, &cbWritten, 0))
        hr = HRESULT_FROM_WIN32(GetLastError());
    return hr;
}

/**
 * Shows a balloon message using VBoxTray's notification area in the
 * Windows task bar.
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 */
VBOXINSTALLHELPER_EXPORT VBoxTrayShowBallonMsg(HWND hwndParent, int string_size,
                                               TCHAR *variables, stack_t **stacktop)
{
    EXDLL_INIT();

    VBOXTRAYIPCHEADER hdr;
    hdr.ulMsg = VBOXTRAYIPCMSGTYPE_SHOWBALLOONMSG;
    hdr.cbBody = sizeof(VBOXTRAYIPCMSG_SHOWBALLOONMSG);

    VBOXTRAYIPCMSG_SHOWBALLOONMSG msg;
    HRESULT hr = VBoxPopString(msg.szContent, sizeof(msg.szContent) / sizeof(TCHAR));
    if (SUCCEEDED(hr))
        hr = VBoxPopString(msg.szTitle, sizeof(msg.szTitle) / sizeof(TCHAR));
    if (SUCCEEDED(hr))
        hr = VBoxPopULong(&msg.ulType);
    if (SUCCEEDED(hr))
        hr = VBoxPopULong(&msg.ulShowMS);

    if (SUCCEEDED(hr))
    {
        msg.ulFlags = 0;

        HANDLE hPipe = VBoIPCConnect();
        if (hPipe)
        {
            hr = VBoxIPCWriteMessage(hPipe, (BYTE*)&hdr, sizeof(VBOXTRAYIPCHEADER));
            if (SUCCEEDED(hr))
                hr = VBoxIPCWriteMessage(hPipe, (BYTE*)&msg, sizeof(VBOXTRAYIPCMSG_SHOWBALLOONMSG));
            VBoxIPCDisconnect(hPipe);
        }
    }

    /* Push simple return value on stack. */
    SUCCEEDED(hr) ? pushstring("0") : pushstring("1");
}

BOOL WINAPI DllMain(HANDLE hInst, ULONG uReason, LPVOID lpReserved)
{
    g_hInstance = (HINSTANCE)hInst;
    return TRUE;
}

/**
 * Disables the Windows File Protection for a specified file
 * using an undocumented SFC API call. Don't try this at home!
 *
 * @param   hwndParent          Window handle of parent.
 * @param   string_size         Size of variable string.
 * @param   variables           The actual variable string.
 * @param   stacktop            Pointer to a pointer to the current stack.
 */
VBOXINSTALLHELPER_EXPORT DisableWFP(HWND hwndParent, int string_size,
                                    TCHAR *variables, stack_t **stacktop)
{
    EXDLL_INIT();

    TCHAR szFile[MAX_PATH + 1];
    HRESULT hr = VBoxPopString(szFile, sizeof(szFile) / sizeof(TCHAR));
    if (SUCCEEDED(hr))
    {
        HMODULE hSFC = LoadLibrary("sfc_os.dll");
        if (NULL != hSFC)
        {
            g_pfnSfcFileException = (fnSfcFileException)GetProcAddress(hSFC, "SfcFileException");
            if (g_pfnSfcFileException == NULL)
            {
                /* If we didn't get the proc address with the call above, try it harder with
                 * the (zero based) index of the function list. */
                g_pfnSfcFileException = (fnSfcFileException)GetProcAddress(hSFC, (LPCSTR)5);
                if (g_pfnSfcFileException == NULL)
                    hr = HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
            }
        }
        else
            hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

        if (SUCCEEDED(hr))
        {
            WCHAR wszFile[MAX_PATH + 1];
            hr = StringCchPrintfW(wszFile, sizeof(wszFile), L"%s", szFile);
            if (SUCCEEDED(hr))
                hr = HRESULT_FROM_WIN32(g_pfnSfcFileException(0, wszFile, -1));
        }

        if (hSFC)
            FreeLibrary(hSFC);
    }

    TCHAR szErr[MAX_PATH + 1];
    if (FAILED(hr))
    {
        if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, hr, 0, szErr, MAX_PATH, NULL))
            szErr[MAX_PATH] = '\0';
        else
            StringCchPrintf(szErr, sizeof(szErr),
                            "FormatMessage failed! Error = %ld", GetLastError());
    }
    else
        StringCchPrintf(szErr, sizeof(szErr), "0");
    pushstring(szErr);
}

