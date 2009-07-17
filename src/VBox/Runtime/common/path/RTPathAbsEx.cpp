/* $Id$ */
/** @file
 * IPRT - RTPathAbsEx
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
#include "internal/iprt.h"
#include <iprt/path.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include "internal/fs.h"



/**
 * Returns the length of the volume name specifier of the given path.
 * If no such specifier zero is returned.
 */
size_t rtPathVolumeSpecLen(const char *pszPath)
{
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
    if (pszPath && *pszPath)
    {
        /* UTC path. */
        /** @todo r=bird: it's UNC and we have to check that the next char isn't a
         *        slash, then skip both the server and the share name. */
        if (    (pszPath[0] == '\\' || pszPath[0] == '/')
            &&  (pszPath[1] == '\\' || pszPath[1] == '/'))
            return strcspn(pszPath + 2, "\\/") + 2;

        /* Drive letter. */
        if (    pszPath[1] == ':'
            &&  toupper(pszPath[0]) >= 'A' && toupper(pszPath[0]) <= 'Z')
            return 2;
    }
    return 0;

#else
    /* This isn't quite right when looking at the above stuff, but it works assuming that '//' does not mean UNC. */
    /// @todo (dmik) well, it's better to consider there's no volume name
    //  at all on *nix systems
    return 0;
//    return pszPath && pszPath[0] == '/';
#endif
}


/**
 * Get the absolute path (no symlinks, no . or .. components), assuming the
 * given base path as the current directory. The resulting path doesn't have
 * to exist.
 *
 * @returns iprt status code.
 * @param   pszBase         The base path to act like a current directory.
 *                          When NULL, the actual cwd is used (i.e. the call
 *                          is equivalent to RTPathAbs(pszPath, ...).
 * @param   pszPath         The path to resolve.
 * @param   pszAbsPath      Where to store the absolute path.
 * @param   cchAbsPath      Size of the buffer.
 */
RTDECL(int) RTPathAbsEx(const char *pszBase, const char *pszPath, char *pszAbsPath, size_t cchAbsPath)
{
    if (pszBase && pszPath && !rtPathVolumeSpecLen(pszPath))
    {
#if defined(RT_OS_WINDOWS)
        /* The format for very long paths is not supported. */
        if (    (pszBase[0] == '/' || pszBase[0] == '\\')
            &&  (pszBase[1] == '/' || pszBase[1] == '\\')
            &&   pszBase[2] == '?'
            &&  (pszBase[3] == '/' || pszBase[3] == '\\'))
            return VERR_INVALID_NAME;
#endif

        /** @todo there are a couple of things which isn't 100% correct, although the
         * current code will have to work for now - I don't have time to fix it right now.
         *
         * 1) On Windows & OS/2 we confuse '/' with an abspath spec and will
         *    not necessarily resolve it on the right drive.
         * 2) A trailing slash in the base might cause UNC names to be created.
         * 3) The lengths total doesn't have to be less than max length
         *    if the pszPath starts with a slash.
         */
        size_t cchBase = strlen(pszBase);
        size_t cchPath = strlen(pszPath);
        if (cchBase + cchPath >= RTPATH_MAX)
            return VERR_FILENAME_TOO_LONG;

        bool fRootSpec = pszPath[0] == '/'
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            || pszPath[0] == '\\'
#endif
            ;
        size_t cchVolSpec = rtPathVolumeSpecLen(pszBase);
        char szPath[RTPATH_MAX];
        if (fRootSpec)
        {
            /* join the disk name from base and the path */
            memcpy(szPath, pszBase, cchVolSpec);
            strcpy(&szPath[cchVolSpec], pszPath);
        }
        else
        {
            /* join the base path and the path */
            strcpy(szPath, pszBase);
            szPath[cchBase] = RTPATH_DELIMITER;
            strcpy(&szPath[cchBase + 1], pszPath);
        }
        return RTPathAbs(szPath, pszAbsPath, cchAbsPath);
    }

    /* Fallback to the non *Ex version */
    return RTPathAbs(pszPath, pszAbsPath, cchAbsPath);
}

