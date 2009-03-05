/** @file
 * Main - Network Interfaces.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef ___netif_h
#define ___netif_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/net.h>

//#include "VBox/com/ptr.h"
//#include <list>

#if 1
/**
 * Encapsulation type.
 */
typedef enum NETIFTYPE
{
    NETIF_T_UNKNOWN,
    NETIF_T_ETHERNET,
    NETIF_T_PPP,
    NETIF_T_SLIP
} NETIFTYPE;

/**
 * Current state of the interface.
 */
typedef enum NETIFSTATUS
{
    NETIF_S_UNKNOWN,
    NETIF_S_UP,
    NETIF_S_DOWN
} NETIFSTATUS;

/**
 * Host Network Interface Information.
 */
typedef struct NETIFINFO
{
    NETIFINFO     *pNext;
    RTNETADDRIPV4  IPAddress;
    RTNETADDRIPV4  IPNetMask;
    RTNETADDRIPV4  IPDefaultGateway;
    RTNETADDRIPV6  IPv6Address;
    RTNETADDRIPV6  IPv6NetMask;
    RTNETADDRIPV6  IPV6DefaultGateway;
    RTMAC          MACAddress;
    NETIFTYPE      enmMediumType;
    NETIFSTATUS    enmStatus;
    RTUUID         Uuid;
    char           szShortName[50];
    char           szName[1];
} NETIFINFO;

/** Pointer to a network interface info. */
typedef NETIFINFO *PNETIFINFO;
/** Pointer to a const network interface info. */
typedef NETIFINFO const *PCNETIFINFO;
#endif

int NetIfList(std::list <ComObjPtr <HostNetworkInterface> > &list);
int NetIfEnableStaticIpConfig(HostNetworkInterface * pIf, ULONG ip, ULONG mask, ULONG gw);
int NetIfEnableStaticIpConfigV6(HostNetworkInterface * pIf, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength, IN_BSTR aIPV6DefaultGateway);
int NetIfEnableDynamicIpConfig(HostNetworkInterface * pIf);

DECLINLINE(Bstr) composeIPv6Address(PRTNETADDRIPV6 aAddrPtr)
{
    char szTmp[8*5];

    RTStrPrintf(szTmp, sizeof(szTmp),
                "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                aAddrPtr->au8[0], aAddrPtr->au8[1],
                aAddrPtr->au8[2], aAddrPtr->au8[3],
                aAddrPtr->au8[4], aAddrPtr->au8[5],
                aAddrPtr->au8[6], aAddrPtr->au8[7],
                aAddrPtr->au8[8], aAddrPtr->au8[9],
                aAddrPtr->au8[10], aAddrPtr->au8[11],
                aAddrPtr->au8[12], aAddrPtr->au8[13],
                aAddrPtr->au8[14], aAddrPtr->au8[15]);
    return Bstr(szTmp);
}

DECLINLINE(int) prefixLength2IPv6Address(ULONG cPrefix, PRTNETADDRIPV6 aAddrPtr)
{
    if(cPrefix > 128)
        return VERR_INVALID_PARAMETER;
    if(!aAddrPtr)
        return VERR_INVALID_PARAMETER;

    ULONG index    = cPrefix >> 3;
    ULONG subindex = cPrefix & 0x7;
    ULONG i;

    for(i = 0; i < index; i++)
        aAddrPtr->au8[i] = ~0;

    for(i = index + 1; i < 16; i++)
        aAddrPtr->au8[i] = 0;

    if(subindex)
        aAddrPtr->au32[index] = (~0) >> (8 - subindex);

    return VINF_SUCCESS;
}

DECLINLINE(Bstr) composeHardwareAddress(PRTMAC aMacPtr)
{
    char szTmp[6*3];

    RTStrPrintf(szTmp, sizeof(szTmp),
                "%02x:%02x:%02x:%02x:%02x:%02x",
                aMacPtr->au8[0], aMacPtr->au8[1],
                aMacPtr->au8[2], aMacPtr->au8[3],
                aMacPtr->au8[4], aMacPtr->au8[5]);
    return Bstr(szTmp);
}

#endif

