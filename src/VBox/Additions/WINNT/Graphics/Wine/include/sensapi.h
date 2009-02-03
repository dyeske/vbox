/*
 * Copyright (C) 2005 Steven Edwards
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
 * Sun LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Sun elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */

#ifndef __SENSAPI_H__
#define __SENSAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_ALIVE_LAN 1
#define NETWORK_ALIVE_WAN 2
#define NETWORK_ALIVE_AOL 4

typedef struct tagQOCINFO
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwInSpeed;
    DWORD dwOutSpeed;
} QOCINFO, *LPQOCINFO;

BOOL WINAPI IsDestinationReachableA(LPCSTR lpszDestination, LPQOCINFO lpQOCInfo);
BOOL WINAPI IsDestinationReachableW(LPCWSTR lpszDestination, LPQOCINFO lpQOCInfo);
#define     IsDestinationReachable WINELIB_NAME_AW(IsDestinationReachable)
BOOL WINAPI IsNetworkAlive(LPDWORD lpdwFlags);

#ifdef __cplusplus
}
#endif

#endif /* __SENSAPI_H__ */
