/* $Id$ */
/** @file
 * Ring-3 Management program for the GCFGM mock-up.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/vmm.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/string.h>


/**
 * Prints the usage and returns 1.
 * @return 1
 */
static int Usage(void)
{
    RTPrintf("usage: tstGlobalConfig <value-name> [new value]\n");
    return 1;
}


int main(int argc, char **argv)
{
    RTR3Init();

    /*
     * Parse args, building the request as we do so.
     */
    if (argc <= 1)
        return Usage();
    if (argc > 3)
    {
        RTPrintf("syntax error: too many arguments\n");
        Usage();
        return 1;
    }

    VMMR0OPERATION enmOp = VMMR0_DO_GCFGM_QUERY_VALUE;
    GCFGMVALUEREQ Req;
    memset(&Req, 0, sizeof(Req));
    Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);

    /* arg[1] = szName */
    size_t cch = strlen(argv[1]);
    if (cch < 2 || argv[1][0] != '/')
    {
        RTPrintf("syntax error: malformed name '%s'\n", argv[1]);
        return 1;
    }
    if (cch >= sizeof(Req.szName))
    {
        RTPrintf("syntax error: the name is too long. (max %zu chars)\n", argv[1], sizeof(Req.szName) - 1);
        return 1;
    }
    memcpy(&Req.szName[0], argv[1], cch + 1);

    /* argv[2] = u64SetValue; optional */
    if (argc == 3)
    {
        char *pszNext = NULL;
        int rc = RTStrToUInt64Ex(argv[2], &pszNext, 0, &Req.u64Value);
        if (RT_FAILURE(rc) || *pszNext)
        {
            RTPrintf("syntax error: '%s' didn't convert successfully to a number. (%Rrc,'%s')\n", argv[2], rc, pszNext);
            return 1;
        }
        enmOp = VMMR0_DO_GCFGM_SET_VALUE;
    }

    /*
     * Open the session, load ring-0 and issue the request.
     */
    PSUPDRVSESSION pSession;
    int rc = SUPR3Init(&pSession);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstGlobalConfig: SUPR3Init -> %Rrc\n", rc);
        return 1;
    }

    rc = SUPLoadVMM("./VMMR0.r0");
    if (RT_SUCCESS(rc))
    {
        Req.pSession = pSession;
        rc = SUPCallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, enmOp, 0, &Req.Hdr);
        if (RT_SUCCESS(rc))
        {
            if (enmOp == VMMR0_DO_GCFGM_QUERY_VALUE)
                RTPrintf("%s = %RU64 (%#RX64)\n", Req.szName, Req.u64Value, Req.u64Value);
            else
                RTPrintf("Successfully set %s = %RU64 (%#RX64)\n", Req.szName, Req.u64Value, Req.u64Value);
        }
        else if (enmOp == VMMR0_DO_GCFGM_QUERY_VALUE)
            RTPrintf("error: Failed to query '%s', rc=%Rrc\n", Req.szName, rc);
        else
            RTPrintf("error: Failed to set '%s' to %RU64, rc=%Rrc\n", Req.szName, Req.u64Value, rc);

    }
    SUPTerm(false /* not forced */);

    return RT_FAILURE(rc) ? 1 : 0;
}
