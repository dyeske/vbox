/* $Id$ */
/** @file
 * IPRT - Crypto - PKCS \#7, Sanity Checkers.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/crypto/pkcs7.h>

#include <iprt/err.h>
#include <iprt/string.h>

//#include <iprt/stream.h>

#include "pkcs7-internal.h"


static int rtCrPkcs7SignedData_CheckSanityExtra(PCRTCRPKCS7SIGNEDDATA pSignedData, uint32_t fFlags,
                                                PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    bool const fAuthenticode = RT_BOOL(fFlags & RTCRPKCS7SIGNEDDATA_SANITY_F_AUTHENTICODE);

    //RTAsn1Dump(&pSignedData->SeqCore.Asn1Core, 0, 0, RTAsn1DumpStrmPrintfV, g_pStdOut);

    if (RTAsn1Integer_UnsignedCompareWithU32(&pSignedData->Version, RTCRPKCS7SIGNEDDATA_V1) != 0)
        return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_SIGNED_DATA_VERSION, "SignedData version is %llu, expected %u",
                             pSignedData->Version.uValue.u, RTCRPKCS7SIGNEDDATA_V1);

    /*
     * DigestAlgorithms.
     */
    if (pSignedData->DigestAlgorithms.cItems == 0) /** @todo this might be too strict */
        return RTErrInfoSet(pErrInfo, VERR_CR_PKCS7_SIGNED_DATA_NO_DIGEST_ALGOS, "SignedData.DigestAlgorithms is empty");
    if (pSignedData->DigestAlgorithms.cItems != 1 && fAuthenticode)
        return RTErrInfoSetF(pErrInfo, VERR_CR_SPC_NOT_EXACTLY_ONE_DIGEST_ALGO,
                             "SignedData.DigestAlgorithms has more than one algorithm (%u)",
                             pSignedData->DigestAlgorithms.cItems);

    if (fFlags & RTCRPKCS7SIGNEDDATA_SANITY_F_ONLY_KNOWN_HASH)
        for (uint32_t i = 0; i < pSignedData->DigestAlgorithms.cItems; i++)
        {
            if (RTCrX509AlgorithmIdentifier_QueryDigestType(&pSignedData->DigestAlgorithms.paItems[i]) == RTDIGESTTYPE_INVALID)
                return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_UNKNOWN_DIGEST_ALGORITHM,
                                     "SignedData.DigestAlgorithms[%i] is not known: %s",
                                     i, pSignedData->DigestAlgorithms.paItems[i].Algorithm.szObjId);
            if (pSignedData->DigestAlgorithms.paItems[i].Parameters.enmType != RTASN1TYPE_NULL)
                return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_DIGEST_PARAMS_NOT_IMPL,
                                     "SignedData.DigestAlgorithms[%i] has parameters: tag=%u",
                                     i, pSignedData->DigestAlgorithms.paItems[i].Parameters.u.Core.uTag);
        }

    /*
     * Certificates.
     */
    if (   (fFlags & RTCRPKCS7SIGNEDDATA_SANITY_F_SIGNING_CERT_PRESENT)
        && pSignedData->Certificates.cItems == 0)
        return RTErrInfoSet(pErrInfo, VERR_CR_PKCS7_NO_CERTIFICATES,
                            "SignedData.Certifcates is empty, expected at least one certificate");

    if (pSignedData->Certificates.cItems > 0)
    {
        int rc = RTCrX509Certificates_CheckSanity(&pSignedData->Certificates, 0, pErrInfo, "SignedData.T0.Certificates");
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Crls.
     */
    if (fAuthenticode && RTAsn1Core_IsPresent(&pSignedData->Crls))
        return RTErrInfoSet(pErrInfo, VERR_CR_PKCS7_EXPECTED_NO_CRLS,
                            "SignedData.Crls is not empty as expected for authenticode.");
    /** @todo check Crls when they become important. */

    /*
     * SignerInfos.
     */
    if (pSignedData->SignerInfos.cItems == 0)
        return RTErrInfoSet(pErrInfo, VERR_CR_PKCS7_NO_SIGNER_INFOS, "SignedData.SignerInfos is empty?");
    if (fAuthenticode && pSignedData->SignerInfos.cItems != 1)
        return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_EXPECTED_ONE_SIGNER_INFO,
                             "SignedData.SignerInfos should have one entry for authenticode: %u",
                             pSignedData->SignerInfos.cItems);

    for (uint32_t i = 0; i < pSignedData->SignerInfos.cItems; i++)
    {
        PCRTCRPKCS7SIGNERINFO pSignerInfo = &pSignedData->SignerInfos.paItems[i];

        if (RTAsn1Integer_UnsignedCompareWithU32(&pSignerInfo->Version, RTCRPKCS7SIGNERINFO_V1) != 0)
            return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_SIGNER_INFO_VERSION,
                                 "SignedData.SignerInfos[%u] version is %llu, expected %u",
                                 pSignerInfo->Version.uValue.u, RTCRPKCS7SIGNERINFO_V1);

        /* IssuerAndSerialNumber. */
        int rc = RTCrX509Name_CheckSanity(&pSignerInfo->IssuerAndSerialNumber.Name, 0, pErrInfo,
                                          "SignedData.SignerInfos[#].IssuerAndSerialNumber.Name");
        if (RT_FAILURE(rc))
            return rc;

        if (pSignerInfo->IssuerAndSerialNumber.SerialNumber.Asn1Core.cb == 0)
            return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_SIGNER_INFO_NO_ISSUER_SERIAL_NO,
                                 "SignedData.SignerInfos[%u].IssuerAndSerialNumber.SerialNumber is missing (zero length)", i);

        PCRTCRX509CERTIFICATE pCert;
        pCert = RTCrX509Certificates_FindByIssuerAndSerialNumber(&pSignedData->Certificates,
                                                                 &pSignerInfo->IssuerAndSerialNumber.Name,
                                                                 &pSignerInfo->IssuerAndSerialNumber.SerialNumber);
        if (!pCert && (fFlags & RTCRPKCS7SIGNEDDATA_SANITY_F_SIGNING_CERT_PRESENT))
            return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_SIGNER_CERT_NOT_SHIPPED,
                                 "SignedData.SignerInfos[%u].IssuerAndSerialNumber not found in T0.Certificates", i);

        /* DigestAlgorithm */
        uint32_t j = 0;
        while (   j < pSignedData->DigestAlgorithms.cItems
               && RTCrX509AlgorithmIdentifier_Compare(&pSignedData->DigestAlgorithms.paItems[j],
                                                      &pSignerInfo->DigestAlgorithm) != 0)
            j++;
        if (j >= pSignedData->DigestAlgorithms.cItems)
            return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_DIGEST_ALGO_NOT_FOUND_IN_LIST,
                                 "SignedData.SignerInfos[%u].DigestAlgorithm (%s) not found in SignedData.DigestAlgorithms",
                                 i, pSignerInfo->DigestAlgorithm.Algorithm.szObjId);

        /* Digest encryption algorithm. */
        if (   pCert
            && RTCrX509AlgorithmIdentifier_Compare(&pSignerInfo->DigestEncryptionAlgorithm,
                                                   &pCert->TbsCertificate.SubjectPublicKeyInfo.Algorithm) != 0)
            return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_SIGNER_INFO_DIGEST_ENCRYPT_MISMATCH,
                                 "SignedData.SignerInfos[%u].DigestEncryptionAlgorithm (%s) mismatch with certificate (%s)",
                                 i, pSignerInfo->DigestEncryptionAlgorithm.Algorithm.szObjId,
                                 pCert->TbsCertificate.SubjectPublicKeyInfo.Algorithm.Algorithm.szObjId);

        /* Authenticated attributes we know. */
        if (RTCrPkcs7Attributes_IsPresent(&pSignerInfo->AuthenticatedAttributes))
        {
            bool fFoundContentInfo   = false;
            bool fFoundMessageDigest = false;
            for (j = 0; j < pSignerInfo->AuthenticatedAttributes.cItems; j++)
            {
                PCRTCRPKCS7ATTRIBUTE pAttrib = &pSignerInfo->AuthenticatedAttributes.paItems[j];
                if (RTAsn1ObjId_CompareWithString(&pAttrib->Type, RTCR_PKCS9_ID_CONTENT_TYPE_OID) == 0)
                {
                    if (fFoundContentInfo)
                        return RTErrInfoSet(pErrInfo, VERR_CR_PKCS7_MISSING_CONTENT_TYPE_ATTRIB,
                                            "Multiple authenticated content-type attributes.");
                    fFoundContentInfo = true;
                    AssertReturn(pAttrib->enmType == RTCRPKCS7ATTRIBUTETYPE_OBJ_IDS, VERR_INTERNAL_ERROR_3);
                    if (pAttrib->uValues.pObjIds->cItems != 1)
                        return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_BAD_CONTENT_TYPE_ATTRIB,
                                             "Expected exactly one value for content-type attrib, found: %u",
                                             pAttrib->uValues.pObjIds->cItems);
                }
                else if (RTAsn1ObjId_CompareWithString(&pAttrib->Type, RTCR_PKCS9_ID_MESSAGE_DIGEST_OID) == 0)
                {
                    if (fFoundMessageDigest)
                        return RTErrInfoSet(pErrInfo, VERR_CR_PKCS7_MISSING_MESSAGE_DIGEST_ATTRIB,
                                            "Multiple authenticated message-digest attributes.");
                    fFoundMessageDigest = true;
                    AssertReturn(pAttrib->enmType == RTCRPKCS7ATTRIBUTETYPE_OCTET_STRINGS, VERR_INTERNAL_ERROR_3);
                    if (pAttrib->uValues.pOctetStrings->cItems != 1)
                        return RTErrInfoSetF(pErrInfo, VERR_CR_PKCS7_BAD_CONTENT_TYPE_ATTRIB,
                                             "Expected exactly one value for message-digest attrib, found: %u",
                                             pAttrib->uValues.pOctetStrings->cItems);
                }
                else
                    AssertReturn(pAttrib->enmType == RTCRPKCS7ATTRIBUTETYPE_UNKNOWN, VERR_INTERNAL_ERROR_3);
            }

            if (!fFoundContentInfo)
                return RTErrInfoSet(pErrInfo, VERR_CR_PKCS7_MISSING_CONTENT_TYPE_ATTRIB,
                                    "Missing authenticated content-type attribute.");
            if (!fFoundMessageDigest)
                return RTErrInfoSet(pErrInfo, VERR_CR_PKCS7_MISSING_MESSAGE_DIGEST_ATTRIB,
                                    "Missing authenticated message-digest attribute.");
        }
    }

    return VINF_SUCCESS;
}


/*
 * Generate the code.
 */
#include <iprt/asn1-generator-sanity.h>

