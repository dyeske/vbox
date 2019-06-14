/* $Id$ */
/** @file
 * IPRT - RTDirSetMode, generic implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_DIR
#ifdef RT_OS_WINDOWS /* dir.h has host specific stuff */
# include <iprt/win/windows.h>
#else
# include <dirent.h>
#endif

#include <iprt/dir.h>
#include "internal/iprt.h"

#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include "internal/dir.h"



RTR3DECL(int) RTDirSetMode(RTDIR hDir, RTFMODE fMode)
{
    /*
     * Validate and digest input.
     */
    if (!rtDirValidHandle(hDir))
        return VERR_INVALID_PARAMETER;

    /* Make sure we flag it as a directory so that it will be converted correctly: */
    if (!(fMode & RTFS_UNIX_MASK))
        fMode |= RTFS_DOS_DIRECTORY;
    else if (!(fMode & RTFS_TYPE_MASK))
        fMode |= RTFS_TYPE_DIRECTORY;

    return RTPathSetMode(hDir->pszPath, fMode);
}

