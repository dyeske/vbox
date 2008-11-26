/** @file
 *
 * VirtualBox COM class implementation
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

#include "GuestOSTypeImpl.h"
#include "Logging.h"
#include <iprt/cpputils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

GuestOSType::GuestOSType()
    : mOSType (VBOXOSTYPE_Unknown)
    , mIs64Bit (false)
    , mRecommendedIOAPIC (false)
    , mRecommendedVirtEx (false)
    , mRAMSize (0), mVRAMSize (0)
    , mHDDSize (0), mMonitorCount (0)
    , mNetworkAdapterType (NetworkAdapterType_Am79C973)
{
}

GuestOSType::~GuestOSType()
{
}

HRESULT GuestOSType::FinalConstruct()
{
    return S_OK;
}

void GuestOSType::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the guest OS type object.
 *
 * @returns COM result indicator
 * @param aFamilyId          os family short name string
 * @param aFamilyDescription os family name string
 * @param aId                os short name string
 * @param aDescription       os name string
 * @param aOSType            global OS type ID
 * @param aIs64Bit           returns true if the given OS is 64-bit
 * @param aRAMSize           recommended RAM size in megabytes
 * @param aVRAMSize          recommended video memory size in megabytes
 * @param aHDDSize           recommended HDD size in megabytes
 */
HRESULT GuestOSType::init (const char *aFamilyId, const char *aFamilyDescription,
                           const char *aId, const char *aDescription,
                           VBOXOSTYPE aOSType, bool aIs64Bit,
                           bool aRecommendedIOAPIC, bool aRecommendedVirtEx,
                           uint32_t aRAMSize, uint32_t aVRAMSize, uint32_t aHDDSize,
                           NetworkAdapterType_T aNetworkAdapterType)
{
    LogFlowThisFunc (("aFamilyId='%s', aFamilyDescription='%s', "
                      "aId='%s', aDescription='%s', "
                      "aType=%d, aIs64Bit=%d, "
                      "aRecommendedIOAPIC=%d, aRecommendedVirtEx=%d, "
                      "aRAMSize=%d, aVRAMSize=%d, aHDDSize=%d, "
                      "aNetworkAdapterType=%d\n",
                      aFamilyId, aFamilyDescription,
                      aId, aDescription,
                      aOSType, aIs64Bit,
                      aRecommendedIOAPIC, aRecommendedVirtEx,
                      aRAMSize, aVRAMSize, aHDDSize,
                      aNetworkAdapterType));

    ComAssertRet (aFamilyId && aFamilyDescription && aId && aDescription, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mFamilyID) = aFamilyId;
    unconst (mFamilyDescription) = aFamilyDescription;
    unconst (mID) = aId;
    unconst (mDescription) = aDescription;
    unconst (mOSType) = aOSType;
    unconst (mIs64Bit) = aIs64Bit;
    unconst (mRecommendedIOAPIC) = aRecommendedIOAPIC;
    unconst (mRecommendedVirtEx) = aRecommendedVirtEx;
    unconst (mRAMSize) = aRAMSize;
    unconst (mVRAMSize) = aVRAMSize;
    unconst (mHDDSize) = aHDDSize;
    unconst (mNetworkAdapterType) = aNetworkAdapterType;

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void GuestOSType::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;
}

// IGuestOSType properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP GuestOSType::COMGETTER(FamilyId) (BSTR *aFamilyId)
{
    if (!aFamilyId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mFamilyID is constant during life time, no need to lock */
    mFamilyID.cloneTo (aFamilyId);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(FamilyDescription) (BSTR *aFamilyDescription)
{
    if (!aFamilyDescription)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mFamilyDescription is constant during life time, no need to lock */
    mFamilyDescription.cloneTo (aFamilyDescription);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(Id) (BSTR *aId)
{
    if (!aId)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mID is constant during life time, no need to lock */
    mID.cloneTo (aId);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(Description) (BSTR *aDescription)
{
    if (!aDescription)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mDescription is constant during life time, no need to lock */
    mDescription.cloneTo (aDescription);

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(Is64Bit) (BOOL *aIs64Bit)
{
    if (!aIs64Bit)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mIs64Bit is constant during life time, no need to lock */
    *aIs64Bit = mIs64Bit;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedIOAPIC) (BOOL *aRecommendedIOAPIC)
{
    if (!aRecommendedIOAPIC)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mRecommendedIOAPIC is constant during life time, no need to lock */
    *aRecommendedIOAPIC = mRecommendedIOAPIC;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedVirtEx) (BOOL *aRecommendedVirtEx)
{
    if (!aRecommendedVirtEx)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mRecommendedVirtEx is constant during life time, no need to lock */
    *aRecommendedVirtEx = mRecommendedVirtEx;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedRAM) (ULONG *aRAMSize)
{
    if (!aRAMSize)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mRAMSize is constant during life time, no need to lock */
    *aRAMSize = mRAMSize;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedVRAM) (ULONG *aVRAMSize)
{
    if (!aVRAMSize)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mVRAMSize is constant during life time, no need to lock */
    *aVRAMSize = mVRAMSize;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(RecommendedHDD) (ULONG *aHDDSize)
{
    if (!aHDDSize)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mHDDSize is constant during life time, no need to lock */
    *aHDDSize = mHDDSize;

    return S_OK;
}

STDMETHODIMP GuestOSType::COMGETTER(AdapterType) (NetworkAdapterType_T *aNetworkAdapterType)
{
    if (!aNetworkAdapterType)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* mNetworkAdapterType is constant during life time, no need to lock */
    *aNetworkAdapterType = mNetworkAdapterType;

    return S_OK;
}
