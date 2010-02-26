/* $Id$ */
/** @file
 * IPRT Testcase - RTSystemQueryOSInfo.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/system.h>

#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static int g_cErrors = 0;


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTSystemQueryOsInfo", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * Simple stuff.
     */
    char szInfo[_4K];

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szInfo, sizeof(szInfo));
    RTTestIPrintf(RTTESTLVL_ALWAYS, "PRODUCT: \"%s\", rc=%Rrc\n", szInfo, rc);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szInfo, sizeof(szInfo));
    RTTestIPrintf(RTTESTLVL_ALWAYS, "RELEASE: \"%s\", rc=%Rrc\n", szInfo, rc);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szInfo, sizeof(szInfo));
    RTTestIPrintf(RTTESTLVL_ALWAYS, "VERSION: \"%s\", rc=%Rrc\n", szInfo, rc);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szInfo, sizeof(szInfo));
    RTTestIPrintf(RTTESTLVL_ALWAYS, "SERVICE_PACK: \"%s\", rc=%Rrc\n", szInfo, rc);

    /*
     * Check that unsupported stuff is terminated correctly.
     */
    for (int i = RTSYSOSINFO_INVALID + 1; i < RTSYSOSINFO_END; i++)
    {
        memset(szInfo, ' ', sizeof(szInfo));
        rc = RTSystemQueryOSInfo((RTSYSOSINFO)i, szInfo, sizeof(szInfo));
        if (    rc == VERR_NOT_SUPPORTED
            &&  szInfo[0] != '\0')
            RTTestIFailed("level=%d; unterminated buffer on VERR_NOT_SUPPORTED\n", i);
        else if (RT_SUCCESS(rc) || rc == VERR_BUFFER_OVERFLOW)
            RTTESTI_CHECK(memchr(szInfo, '\0', sizeof(szInfo)) != NULL);
        else if (rc != VERR_NOT_SUPPORTED)
            RTTestIFailed("level=%d unexpected rc=%Rrc\n", i, rc);
    }

    /*
     * Check buffer overflow
     */
    RTAssertSetQuiet(true);
    RTAssertSetMayPanic(false);
    for (int i = RTSYSDMISTR_INVALID + 1; i < RTSYSDMISTR_END; i++)
    {
        RTTESTI_CHECK_RC(RTSystemQueryDmiString((RTSYSDMISTR)i, szInfo, 0), VERR_INVALID_PARAMETER);

        /* Get the length of the info and check that we get overflow errors for
           everything less that it.  */
        rc = RTSystemQueryOSInfo((RTSYSOSINFO)i, szInfo, sizeof(szInfo));
        if (RT_FAILURE(rc))
            continue;
        size_t const cchInfo = strlen(szInfo);

        for (size_t cch = 1; cch < sizeof(szInfo) && cch < cchInfo; cch++)
        {
            memset(szInfo, 0x7f, sizeof(szInfo));
            RTTESTI_CHECK_RC(RTSystemQueryOSInfo((RTSYSOSINFO)i, szInfo, cch), VERR_BUFFER_OVERFLOW);

            /* check the padding. */
            for (size_t off = cch; off < sizeof(szInfo); off++)
                if (szInfo[off] != 0x7f)
                {
                    RTTestIFailed("level=%d, rc=%Rrc, cch=%zu, off=%zu: Wrote too much!\n", i, rc, cch, off);
                    break;
                }

            /* check for zero terminator. */
            if (!memchr(szInfo, '\0', cch))
                RTTestIFailed("level=%d, rc=%Rrc, cch=%zu: Buffer not terminated!\n", i, rc, cch);
        }

        /* Check that the exact length works. */
        rc = RTSystemQueryOSInfo((RTSYSOSINFO)i, szInfo, cchInfo + 1);
        if (rc != VINF_SUCCESS)
            RTTestIFailed("level=%d: rc=%Rrc when specifying exactly right buffer length (%zu)\n", i, rc, cchInfo + 1);
    }

    return RTTestSummaryAndDestroy(hTest);
}

