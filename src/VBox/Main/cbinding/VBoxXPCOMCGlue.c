/* $Revision$ */
/** @file
 * Glue code for dynamically linking to VBoxXPCOMC.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxXPCOMCGlue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#if defined(__linux__) || defined(__linux_gnu__) || defined(__sun__)
# define DYNLIB_NAME    "VBoxXPCOMC.so"
#elif defined(__APPLE__)
# define DYNLIB_NAME    "VBoxXPCOMC.dylib"
#elif defined(_MSC_VER) || defined(__OS2__)
# define DYNLIB_NAME    "VBoxXPCOMC.dll"
#else
# error "Port me"
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The dlopen handle for VBoxXPCOMC. */
void *g_hVBoxXPCOMC = NULL;
/** The last load error. */
char g_szVBoxErrMsg[256];
/** Pointer to the VBoxXPCOMC function table.  */
PCVBOXXPCOM g_pVBoxFuncs = NULL;


/**
 * Try load VBoxXPCOMC.so/dylib/dll from the specified location and resolve all
 * the symbols we need.
 *
 * @returns 0 on success, -1 on failure.
 * @param   pszHome         The director where to try load VBoxXPCOMC from. Can be NULL.
 * @param   pszMsgPrefix    Error message prefix. NULL means no error messages.
 */
static int tryLoadOne(const char *pszHome, const char *pszMsgPrefix)
{
    size_t      cchHome = pszHome ? strlen(pszHome) : 0;
    size_t      cbBuf;
    char *      pszBuf;
    int         rc = -1;

    /*
     * Construct the full name.
     */
    cbBuf = cchHome + sizeof("/" DYNLIB_NAME);
    pszBuf = (char *)malloc(cbBuf);
    if (!pszBuf)
    {
        sprintf(g_szVBoxErrMsg, "malloc(%u) failed", (unsigned)cbBuf);
        if (pszMsgPrefix)
            fprintf(stderr, "%s%s\n", pszMsgPrefix, g_szVBoxErrMsg);
        return -1;
    }
    if (pszHome)
    {
        memcpy(pszBuf, pszHome, cchHome);
        pszBuf[cchHome] = '/';
        cchHome++;
    }
    memcpy(&pszBuf[cchHome], DYNLIB_NAME, sizeof(DYNLIB_NAME));

    /*
     * Try load it by that name, setting the VBOX_APP_HOME first (for now).
     * Then resolve and call the function table getter.
     */
    setenv("VBOX_APP_HOME", pszHome, 0 /* no need to overwrite */);
    g_hVBoxXPCOMC = dlopen(pszBuf, RTLD_NOW | RTLD_LOCAL);
    if (g_hVBoxXPCOMC)
    {
        PFNVBOXGETXPCOMCFUNCTIONS pfnGetFunctions;
        pfnGetFunctions = (PFNVBOXGETXPCOMCFUNCTIONS)
            dlsym(g_hVBoxXPCOMC, VBOX_GET_XPCOMC_FUNCTIONS_SYMBOL_NAME);
        if (pfnGetFunctions)
        {
            g_pVBoxFuncs = pfnGetFunctions(VBOX_XPCOMC_VERSION);
            if (g_pVBoxFuncs)
                rc = 0;
            else
                sprintf(g_szVBoxErrMsg, "%.80s: pfnGetFunctions(%#x) failed",
                        pszBuf, VBOX_XPCOMC_VERSION);
        }
        else
            sprintf(g_szVBoxErrMsg, "dlsym(%.80s/%.32s): %128s",
                    pszBuf, VBOX_GET_XPCOMC_FUNCTIONS_SYMBOL_NAME, dlerror());
        if (rc != 0)
            dlclose(g_hVBoxXPCOMC);
        g_hVBoxXPCOMC = NULL;
    }
    else
        sprintf(g_szVBoxErrMsg, "dlopen(%.80s): %128s", pszBuf, dlerror());
    free(pszBuf);
    return rc;
}


/**
 * Tries to locate and load VBoxXPCOMC.so/dylib/dll, resolving all the related
 * function pointers.
 *
 * @returns 0 on success, -1 on failure.
 * @param   pszMsgPrefix    Error message prefix. NULL means no error messages.
 *
 * @remark  This should be considered moved into a separate glue library since
 *          its its going to be pretty much the same for any user of VBoxXPCOMC
 *          and it will just cause trouble to have duplicate versions of this
 *          source code all around the place.
 */
int VBoxCGlueInit(const char *pszMsgPrefix)
{
    /*
     * If the user specifies the location, try only that.
     */
    const char *pszHome = getenv("VBOX_APP_HOME");
    if (pszHome)
        return tryLoadOne(pszHome, pszMsgPrefix);

    /*
     * Try the known standard locations.
     */
#if defined(__gnu__linux__) || defined(__linux__)
    if (tryLoadOne("/opt/VirtualBox", pszMsgPrefix) == 0)
        return 0;
    if (tryLoadOne("/usr/lib/virtualbox", pszMsgPrefix) == 0)
        return 0;
#elif defined(__sun__)
    if (tryLoadOne("/opt/VirtualBox/amd64", pszMsgPrefix) == 0)
        return 0;
    if (tryLoadOne("/opt/VirtualBox/i386", pszMsgPrefix) == 0)
        return 0;
#elif defined(__APPLE__)
    if (tryLoadOne("/Application/VirtualBox.app/Contents/MacOS", pszMsgPrefix) == 0)
        return 0;
#else
# error "port me"
#endif

    /*
     * Finally try the dynamic linker search path.
     */
    if (tryLoadOne(NULL, pszMsgPrefix) == 0)
        return 0;

    /* No luck, return failure. */
    if (pszMsgPrefix)
        fprintf(stderr, "%sFailed to locate VBoxXPCOMC\n", pszMsgPrefix);
    return -1;
}


/**
 * Terminate the C glue library.
 */
void VBoxCGlueTerm(void)
{
    if (g_hVBoxXPCOMC)
    {
        dlclose(g_hVBoxXPCOMC);
        g_hVBoxXPCOMC = NULL;
    }
    g_pVBoxFuncs = NULL;
}

