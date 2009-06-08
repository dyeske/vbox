/** @file
 *
 * Simple VBox HDD container test utility. Compares two images and prints
 * differences. Mainly used to check cloning of disk images.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <VBox/err.h>
#include <VBox/VBoxHDD.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/initterm.h>
#include <iprt/rand.h>
#include "stdio.h"
#include "stdlib.h"

/* from VBoxHDD.cpp */
#define VD_MERGE_BUFFER_SIZE    (16 * _1M)

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The error count. */
unsigned g_cErrors = 0;


static void tstVDError(void *pvUser, int rc, RT_SRC_POS_DECL,
                       const char *pszFormat, va_list va)
{
    g_cErrors++;
    RTPrintf("tstVD: Error %Rrc at %s:%u (%s): ", rc, RT_SRC_POS_ARGS);
    RTPrintfV(pszFormat, va);
    RTPrintf("\n");
}

int main(int argc, char *argv[])
{
    int rc;

    RTR3Init();

    if (argc != 3)
    {
        RTPrintf("Usage: ./tstVDCopy <hdd1> <hdd2>\n");
        return 1;
    }

    RTPrintf("tstVDCopy: TESTING...\n");

    PVBOXHDD         pVD1 = NULL;
    PVBOXHDD         pVD2 = NULL;
    PVDINTERFACE     pVDIfs = NULL;
    VDINTERFACE      VDIError;
    VDINTERFACEERROR VDIErrorCallbacks;
    char *pszVD1 = NULL;
    char *pszVD2 = NULL;
    char *pbBuf1 = NULL;
    char *pbBuf2 = NULL;

#define CHECK(str) \
    do \
    { \
        if (RT_FAILURE(rc)) \
        { \
            RTPrintf("%s rc=%Rrc\n", str, rc); \
            if (pVD1) \
                VDCloseAll(pVD1); \
            if (pVD2) \
                VDCloseAll(pVD2); \
            return rc; \
        } \
    } while (0)

    pbBuf1 = (char *)RTMemAllocZ(VD_MERGE_BUFFER_SIZE);
    pbBuf2 = (char *)RTMemAllocZ(VD_MERGE_BUFFER_SIZE);

    /* Create error interface. */
    VDIErrorCallbacks.cbSize = sizeof(VDINTERFACEERROR);
    VDIErrorCallbacks.enmInterface = VDINTERFACETYPE_ERROR;
    VDIErrorCallbacks.pfnError = tstVDError;

    rc = VDInterfaceAdd(&VDIError, "tstVD_Error", VDINTERFACETYPE_ERROR, &VDIErrorCallbacks,
                        NULL, &pVDIfs);
    AssertRC(rc);

    rc = VDGetFormat(argv[1], &pszVD1);
    CHECK("VDGetFormat() hdd1");

    rc = VDGetFormat(argv[2], &pszVD2);
    CHECK("VDGetFormat() hdd2");

    rc = VDCreate(&VDIError, &pVD1);
    CHECK("VDCreate() hdd1");

    rc = VDCreate(&VDIError, &pVD2);
    CHECK("VDCreate() hdd1");

    rc = VDOpen(pVD1, pszVD1, argv[1], VD_OPEN_FLAGS_NORMAL, NULL);
    CHECK("VDOpen() hdd1");

    rc = VDOpen(pVD2, pszVD2, argv[2], VD_OPEN_FLAGS_NORMAL, NULL);
    CHECK("VDOpen() hdd2");

    uint64_t cbSize1 = 0;
    uint64_t cbSize2 = 0;

    cbSize1 = VDGetSize(pVD1, 0);
    Assert(cbSize1 != 0);
    cbSize2 = VDGetSize(pVD1, 0);
    Assert(cbSize1 != 0);

    if (cbSize1 == cbSize2)
    {
        uint64_t uOffCurr = 0;

        /* Compare block by block. */
        while (uOffCurr < cbSize1)
        {
            size_t cbRead = RT_MIN((cbSize1 - uOffCurr), VD_MERGE_BUFFER_SIZE);

            rc = VDRead(pVD1, uOffCurr, pbBuf1, cbRead);
            CHECK("VDRead() hdd1");

            rc = VDRead(pVD2, uOffCurr, pbBuf2, cbRead);
            CHECK("VDRead() hdd2");

            if (memcmp(pbBuf1, pbBuf2, cbRead))
            {
                RTPrintf("tstVDCopy: Images differ uOffCurr=%llu\n", uOffCurr);
                /* Do byte by byte comaprison. */
                for (size_t i = 0; i < cbRead; i++)
                {
                    if (pbBuf1[i] != pbBuf2[i])
                    {
                        RTPrintf("tstVDCopy: First different byte is at offset %llu\n", uOffCurr + i);
                        break;
                    }
                }
                break;
            }

            uOffCurr += cbRead;
        }
    }
    else
        RTPrintf("tstVDCopy: Images have different size hdd1=%llu hdd2=%llu\n", cbSize1, cbSize2);

    VDClose(pVD1, false);
    CHECK("VDClose() hdd1");

    VDClose(pVD2, false);
    CHECK("VDClose() hdd2");

    VDDestroy(pVD1);
    VDDestroy(pVD2);
    RTMemFree(pbBuf1);
    RTMemFree(pbBuf2);
#undef CHECK

    rc = VDShutdown();
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstVD-2: unloading backends failed! rc=%Rrc\n", rc);
        g_cErrors++;
    }
    /*
     * Summary
     */
    if (!g_cErrors)
        RTPrintf("tstVD-2: SUCCESS\n");
    else
        RTPrintf("tstVD-2: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}

