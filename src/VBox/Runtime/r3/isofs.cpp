/* $Id$ */
/** @file
 * IPRT - ISO 9660 file system handling.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/isofs.h>
#include <iprt/path.h>


/**
 * Destroys the patch cache.
 *
 * @param   pFile
 */
RTR3DECL(void) rtIsoFsDestroyPathCache(PRTISOFSFILE pFile)
{
    PRTISOFSPATHTABLEENTRY pNode = RTListNodeGetFirst(&pFile->listPaths, RTISOFSPATHTABLEENTRY, Node);
    while (pNode)
    {
        PRTISOFSPATHTABLEENTRY pNext = RTListNodeGetNext(&pNode->Node, RTISOFSPATHTABLEENTRY, Node);
        bool fLast = RTListNodeIsLast(&pFile->listPaths, &pNode->Node);

        if (pNode->path)
            RTStrFree(pNode->path);
        if (pNode->path_full)
            RTStrFree(pNode->path_full);
        RTListNodeRemove(&pNode->Node);
        RTMemFree(pNode);

        if (fLast)
            break;

        pNode = pNext;
    }
}


/**
 *
 *
 * @return  int
 *
 * @param   pList
 * @param   pszPath
 * @param   pHeader
 */
RTR3DECL(int) rtIsoFsAddToPathCache(PRTLISTNODE pList, const char *pszPath,
                                    RTISOFSPATHTABLEHEADER *pHeader)
{
    AssertPtrReturn(pList, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPath, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pHeader, VERR_INVALID_PARAMETER);

    PRTISOFSPATHTABLEENTRY pNode = (PRTISOFSPATHTABLEENTRY)RTMemAlloc(sizeof(RTISOFSPATHTABLEENTRY));
    if (pNode == NULL)
        return VERR_NO_MEMORY;

    pNode->path = NULL;
    if (RT_SUCCESS(RTStrAAppend(&pNode->path, pszPath)))
    {
        memcpy((RTISOFSPATHTABLEHEADER*)&pNode->header,
               (RTISOFSPATHTABLEHEADER*)pHeader, sizeof(pNode->header));

        pNode->path_full = NULL;
        pNode->Node.pPrev = NULL;
        pNode->Node.pNext = NULL;
        RTListAppend(pList, &pNode->Node);
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 *
 *
 * @return  int
 *
 * @param   pList
 * @param   pNode
 * @param   pszPathNode
 * @param   ppszPath
 */
RTR3DECL(int) rtIsoFsGetParentPathSub(PRTLISTNODE pList, PRTISOFSPATHTABLEENTRY pNode,
                                      char *pszPathNode, char **ppszPath)
{
    int rc = VINF_SUCCESS;
    if (pNode->header.parent_index > 1)
    {
        uint16_t idx = 1;
        PRTISOFSPATHTABLEENTRY pNodeParent = RTListNodeGetFirst(pList, RTISOFSPATHTABLEENTRY, Node);
        while (idx++ < pNode->header.parent_index)
            pNodeParent =  RTListNodeGetNext(&pNodeParent->Node, RTISOFSPATHTABLEENTRY, Node);
        char *pszPath;
        if (RTStrAPrintf(&pszPath, "%s/%s", pNodeParent->path, pszPathNode))
        {
            rc = rtIsoFsGetParentPathSub(pList, pNodeParent, pszPath, ppszPath);
            RTStrFree(pszPath);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
    {
        char *pszPath = RTStrDup(pszPathNode);
        *ppszPath = pszPath;
    }
    return rc;
}


/**
 *
 *
 * @return  int
 *
 * @param   pFile
 */
RTR3DECL(int) rtIsoFsUpdatePathCache(PRTISOFSFILE pFile)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    rtIsoFsDestroyPathCache(pFile);

    RTListInit(&pFile->listPaths);

    /* Seek to path tables. */
    int rc = VINF_SUCCESS;
    Assert(pFile->pvd.path_table_start_first > 16);
    uint64_t uTableStart = (pFile->pvd.path_table_start_first * RTISOFS_SECTOR_SIZE);
    Assert(uTableStart % RTISOFS_SECTOR_SIZE == 0); /* Make sure it's aligned. */
    if (RTFileTell(pFile->file) != uTableStart)
        rc = RTFileSeek(pFile->file, uTableStart, RTFILE_SEEK_BEGIN, &uTableStart);

    /*
     * Since this is a sequential format, for performance it's best to read the
     * complete path table (every entry can have its own level (directory depth) first
     * and the actual directories of the path table afterwards.
     */

    /* Read in the path table ... */
    size_t cbLeft = pFile->pvd.path_table_size;
    RTISOFSPATHTABLEHEADER header;
    while ((cbLeft > 0) && RT_SUCCESS(rc))
    {
        size_t cbRead;
        rc = RTFileRead(pFile->file, (RTISOFSPATHTABLEHEADER*)&header, sizeof(RTISOFSPATHTABLEHEADER), &cbRead);
        if (RT_FAILURE(rc))
            break;
        cbLeft -= cbRead;
        if (header.length)
        {
            Assert(cbLeft >= header.length);
            Assert(header.length <= 31);
            /* Allocate and read in the actual path name. */
            char *pszName = RTStrAlloc(header.length + 1);
            rc = RTFileRead(pFile->file, (char*)pszName, header.length, &cbRead);
            if (RT_SUCCESS(rc))
            {
                cbLeft -= cbRead;
                pszName[cbRead] = '\0'; /* Terminate string. */
                /* Add entry to cache ... */
                rc = rtIsoFsAddToPathCache(&pFile->listPaths, pszName, &header);
            }
            RTStrFree(pszName);
            /* Read padding if required ... */
            if ((header.length % 2) != 0) /* If we have an odd length, read/skip the padding byte. */
            {
                rc = RTFileSeek(pFile->file, 1, RTFILE_SEEK_CURRENT, NULL);
                cbLeft--;
            }
        }
    }

    /* Transform path names into full paths. This is a bit ugly right now. */
    PRTISOFSPATHTABLEENTRY pNode = RTListNodeGetLast(&pFile->listPaths, RTISOFSPATHTABLEENTRY, Node);
    while (   pNode
           && !RTListNodeIsFirst(&pFile->listPaths, &pNode->Node)
           && RT_SUCCESS(rc))
    {
        rc = rtIsoFsGetParentPathSub(&pFile->listPaths, pNode,
                                       pNode->path, &pNode->path_full);
        if (RT_SUCCESS(rc))
            pNode = RTListNodeGetPrev(&pNode->Node, RTISOFSPATHTABLEENTRY, Node);
    }

    return rc;
}


/**
 *
 *
 * @return  int
 *
 * @param   pszFileName
 * @param   pFile
 */
RTR3DECL(int) RTIsoFsOpen(PRTISOFSFILE pFile, const char *pszFileName)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszFileName, VERR_INVALID_PARAMETER);

    RTListInit(&pFile->listPaths);
#if 0
    Assert(sizeof(RTISOFSDATESHORT) == 7);
    Assert(sizeof(RTISOFSDATELONG) == 17);
    int l = sizeof(RTISOFSDIRRECORD);
    RTPrintf("RTISOFSDIRRECORD=%ld\n", l);
    Assert(l == 33);
    /* Each volume descriptor exactly occupies one sector. */
    l = sizeof(RTISOFSPRIVOLDESC);
    RTPrintf("RTISOFSPRIVOLDESC=%ld\n", l);
    Assert(l == RTISOFS_SECTOR_SIZE);
#endif
    int rc = RTFileOpen(&pFile->file, pszFileName, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(rc))
    {
        uint64_t cbSize;
        rc = RTFileGetSize(pFile->file, &cbSize);
        if (   RT_SUCCESS(rc)
            && cbSize > 16 * RTISOFS_SECTOR_SIZE)
        {
            uint64_t cbOffset = 16 * RTISOFS_SECTOR_SIZE; /* Start reading at 32k. */
            size_t cbRead;
            RTISOFSPRIVOLDESC pvd;
            bool fFoundPrimary = false;
            bool fIsValid = false;
            while (cbOffset < _1M)
            {
                /* Get primary descriptor. */
                rc = RTFileRead(pFile->file, (PRTISOFSPRIVOLDESC)&pvd, sizeof(RTISOFSPRIVOLDESC), &cbRead);
                if (RT_FAILURE(rc) || cbRead < sizeof(RTISOFSPRIVOLDESC))
                    break;
                if (   RTStrStr((char*)pvd.name_id, RTISOFS_STANDARD_ID)
                    && pvd.type == 0x1 /* Primary Volume Descriptor */)
                {
                    memcpy((PRTISOFSPRIVOLDESC)&pFile->pvd,
                           (PRTISOFSPRIVOLDESC)&pvd, sizeof(RTISOFSPRIVOLDESC));
                    fFoundPrimary = true;
                }
                else if(pvd.type == 0xff /* Termination Volume Descriptor */)
                {
                    if (fFoundPrimary)
                        fIsValid = true;
                    break;
                }
                cbOffset += sizeof(RTISOFSPRIVOLDESC);
            }

            if (fIsValid)
                rc = rtIsoFsUpdatePathCache(pFile);
            else
                rc = VERR_INVALID_PARAMETER;
        }
        if (RT_FAILURE(rc))
            RTIsoFsClose(pFile);
    }
    return rc;
}


/**
 *
 *
 *
 * @param   pFile
 */
RTR3DECL(void) RTIsoFsClose(PRTISOFSFILE pFile)
{
    if (pFile)
    {
        rtIsoFsDestroyPathCache(pFile);
        RTFileClose(pFile->file);
    }
}


/**
 * Parses an extent given at the specified sector + size and
 * returns a directory record.
 *
 * @return  int
 *
 * @param   pFile
 * @param   pszFileName
 * @param   uExtentSector
 * @param   cbExtent
 * @param   ppRec
 */
RTR3DECL(int) rtIsoFsFindEntry(PRTISOFSFILE pFile, const char *pszFileName,
                               uint32_t uExtentSector, uint32_t cbExtent /* Bytes */,
                               PRTISOFSDIRRECORD *ppRec)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    Assert(uExtentSector > 16);

    int rc = RTFileSeek(pFile->file, uExtentSector * RTISOFS_SECTOR_SIZE,
                        RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = VERR_FILE_NOT_FOUND;

        uint8_t uBuffer[RTISOFS_SECTOR_SIZE];
        size_t cbLeft = cbExtent;
        while (!RT_SUCCESS(rc) && cbLeft > 0)
        {
            size_t cbRead;
            int rc2 = RTFileRead(pFile->file, (void*)&uBuffer, sizeof(uBuffer), &cbRead);
            Assert(RT_SUCCESS(rc2) && cbRead == RTISOFS_SECTOR_SIZE);
            cbLeft -= cbRead;

            uint32_t idx = 0;
            while (idx < cbRead)
            {
                PRTISOFSDIRRECORD pCurRecord = (PRTISOFSDIRRECORD)&uBuffer[idx];
                if (pCurRecord->record_length == 0)
                    break;

                Assert(pCurRecord->name_len > 0);
                char *pszName = RTStrAlloc(pCurRecord->name_len + 1);
                AssertPtr(pszName);
                Assert(idx + sizeof(RTISOFSDIRRECORD) < cbRead);
                memcpy(pszName, &uBuffer[idx + sizeof(RTISOFSDIRRECORD)], pCurRecord->name_len);

                if (   pCurRecord->name_len == 1
                    && pszName[0] == 0x0)
                {
                    /* This is a "." directory (self). */
                }
                else if (   pCurRecord->name_len == 1
                         && pszName[0] == 0x1)
                {
                    /* This is a ".." directory (parent). */
                }
                else /* Regular directory or file */
                {
                    if (pCurRecord->flags & RT_BIT(1)) /* Directory */
                    {
                        /* We don't recursively go into directories
                         * because we already have the cached path table. */
                        pszName[pCurRecord->name_len] = 0;
                        /*rc = rtIsoFsParseDir(pFile, pszFileName,
                                                 pDirHdr->extent_location, pDirHdr->extent_data_length);*/
                    }
                    else /* File */
                    {
                        /* Get last occurence of ";" and cut it off. */
                        char *pTerm = strrchr(pszName, ';');
                        if (pTerm)
                            pszName[pTerm - pszName] = 0;

                        /* Don't use case sensitive comparison here, in IS0 9660 all
                         * file / directory names are UPPERCASE. */
                        if (!RTStrICmp(pszName, pszFileName))
                        {
                            PRTISOFSDIRRECORD pRec = (PRTISOFSDIRRECORD)RTMemAlloc(sizeof(RTISOFSDIRRECORD));
                            if (pRec)
                            {
                                memcpy(pRec, pCurRecord, sizeof(RTISOFSDIRRECORD));
                                *ppRec = pRec;
                                rc = VINF_SUCCESS;
                                break;
                            }
                            else
                                rc = VERR_NO_MEMORY;
                        }
                    }
                }
                idx += pCurRecord->record_length;
            }
        }
    }
    return rc;
}


/**
 *
 *
 * @return  int
 *
 * @param   pFile
 * @param   pszPath
 * @param   puSector
 */
RTR3DECL(int) rtIsoFsResolvePath(PRTISOFSFILE pFile, const char *pszPath, uint32_t *puSector)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPath, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puSector, VERR_INVALID_PARAMETER);

    int rc = VERR_FILE_NOT_FOUND;
    char *pszTemp = RTStrDup(pszPath);
    if (pszTemp)
    {
        RTPathStripFilename(pszTemp);

        bool bFound = false;
        PRTISOFSPATHTABLEENTRY pNode;
        if (!RTStrCmp(pszTemp, ".")) /* Root directory? Use first node! */
        {
            pNode = RTListNodeGetFirst(&pFile->listPaths, RTISOFSPATHTABLEENTRY, Node);
            bFound = true;
        }
        else
        {
            RTListForEach(&pFile->listPaths, pNode, RTISOFSPATHTABLEENTRY, Node)
            {
                if (   pNode->path_full != NULL /* Root does not have a path! */
                    && !RTStrICmp(pNode->path_full, pszTemp))
                {
                    bFound = true;
                    break;
                }
            }
        }
        if (bFound)
        {
            *puSector = pNode->header.sector_dir_table;
            rc = VINF_SUCCESS;
        }
        RTStrFree(pszTemp);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 *
 *
 * @return  int
 *
 * @param   pFile
 * @param   pszPath
 * @param   ppRecord
 */
RTR3DECL(int) rtIsoFsGetDirectoryRecord(PRTISOFSFILE pFile, const char *pszPath,
                                        PRTISOFSDIRRECORD *ppRecord)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPath, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppRecord, VERR_INVALID_PARAMETER);

    uint32_t uSector;
    int rc = rtIsoFsResolvePath(pFile, pszPath, &uSector);
    if (RT_SUCCESS(rc))
    {
        /* Seek and read the directory record of given file. */
        rc = RTFileSeek(pFile->file, uSector * RTISOFS_SECTOR_SIZE,
                        RTFILE_SEEK_BEGIN, NULL);
        if (RT_SUCCESS(rc))
        {
            size_t cbRead;
            PRTISOFSDIRRECORD pRecord = (PRTISOFSDIRRECORD)RTMemAlloc(sizeof(RTISOFSDIRRECORD));
            if (pRecord)
            {
                rc = RTFileRead(pFile->file, (PRTISOFSDIRRECORD)pRecord, sizeof(RTISOFSDIRRECORD), &cbRead);
                if (RT_SUCCESS(rc))
                {
                    Assert(cbRead == sizeof(RTISOFSDIRRECORD));
                    *ppRecord = pRecord;
                }
                if (RT_FAILURE(rc))
                    RTMemFree(pRecord);
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }
    return rc;
}


RTR3DECL(int) RTIsoFsGetFileInfo(PRTISOFSFILE pFile, const char *pszPath,
                                 uint32_t *pcbOffset, size_t *pcbLength)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPath, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbOffset, VERR_INVALID_PARAMETER);

    PRTISOFSDIRRECORD pDirRecord;
    int rc = rtIsoFsGetDirectoryRecord(pFile, pszPath, &pDirRecord);
    if (RT_SUCCESS(rc))
    {
        /* Get actual file record. */
        PRTISOFSDIRRECORD pFileRecord;
        rc = rtIsoFsFindEntry(pFile,
                                RTPathFilename(pszPath),
                                pDirRecord->extent_location,
                                pDirRecord->extent_data_length,
                                &pFileRecord);
        if (RT_SUCCESS(rc))
        {
            *pcbOffset = pFileRecord->extent_location * RTISOFS_SECTOR_SIZE;
            *pcbLength = pFileRecord->extent_data_length;
            RTMemFree(pFileRecord);
        }
        RTMemFree(pDirRecord);
    }
    return rc;
}


RTR3DECL(int) RTIsoFsExtractFile(PRTISOFSFILE pFile, const char *pszSource,
                                 const char *pszDest)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszSource, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDest, VERR_INVALID_PARAMETER);

    uint32_t cbOffset;
    size_t cbLength;
    int rc = RTIsoFsGetFileInfo(pFile, pszSource, &cbOffset, &cbLength);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileSeek(pFile->file, cbOffset, RTFILE_SEEK_BEGIN, NULL);
        if (RT_SUCCESS(rc))
        {
            RTFILE fileDest;
            rc = RTFileOpen(&fileDest, pszDest, RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
            if (RT_SUCCESS(rc))
            {
                size_t cbToRead, cbRead, cbWritten;
                uint8_t byBuffer[_64K];
                while (   cbLength > 0
                       && RT_SUCCESS(rc))
                {
                    cbToRead = RT_MIN(cbLength, _64K);
                    rc = RTFileRead(pFile->file, (uint8_t*)byBuffer, cbToRead, &cbRead);
                    if (RT_FAILURE(rc))
                        break;
                    rc = RTFileWrite(fileDest, (uint8_t*)byBuffer, cbRead, &cbWritten);
                    if (RT_FAILURE(rc))
                        break;
                    cbLength -= cbRead;
                }
                RTFileClose(fileDest);
            }
        }
    }
    return rc;
}

