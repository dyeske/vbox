/*
 * Copyright (C) 1991-1998 by LCS/Telegraphics
 * Copyright (C) 2002 Patrik Stridvall
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
 * Oracle LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Oracle elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */

#ifndef __WINE_PKTDEF_H
#define __WINE_PKTDEF_H

/***********************************************************************
 * How to use pktdef.h:
 * 
 * 1. Include wintab.h
 * 2. if using just one packet format:
 *    a. Define PACKETDATA and PACKETMODE as or'ed combinations of WTPKT bits
 *       (use the PK_* identifiers).
 *    b. Include pktdef.h.
 *    c. The generated structure typedef will be called PACKET.  Use PACKETDATA
 *       and PACKETMODE to fill in the LOGCONTEXT structure.
 * 3. If using multiple packet formats, for each one:
 *    a. Define PACKETNAME. Its text value will be a prefix for this packet's
 *       parameters and names.
 *    b. Define <PACKETNAME>PACKETDATA and <PACKETNAME>PACKETMODE similar to
 *       2.a. above.
 *    c. Include pktdef.h.
 *    d. The generated structure typedef will be called
 *       <PACKETNAME>PACKET. Compare with 2.c. above and example #2 below.
 * 4. If using extension packet data, do the following additional steps
 *    for each extension:
 *    a. Before including pktdef.h, define <PACKETNAME>PACKET<EXTENSION>
 *       as either PKEXT_ABSOLUTE or PKEXT_RELATIVE.
 *    b. The generated structure typedef will contain a field for the
 *       extension data.
 *    c. Scan the WTI_EXTENSION categories to find the extension's
 *       packet mask bit.
 *    d. OR the packet mask bit with <PACKETNAME>PACKETDATA and use the
 *       result in the lcPktData field of the LOGCONTEXT structure.
 *    e. If <PACKETNAME>PACKET<EXTENSION> was PKEXT_RELATIVE, OR the
 *       packet mask bit with <PACKETNAME>PACKETMODE and use the result
 *       in the lcPktMode field of the LOGCONTEXT structure.
 *
 *
 * Example #1.  -- single packet format
 *
 * #include <wintab.h>
 * #define PACKETDATA PK_X | PK_Y | PK_BUTTONS  /@ x, y, buttons @/
 * #define PACKETMODE PK_BUTTONS                /@ buttons relative mode @/
 * #include <pktdef.h>
 * ...
 *     lc.lcPktData = PACKETDATA;
 *     lc.lcPktMode = PACKETMODE;
 * 
 * Example #2.  -- multiple formats
 * 
 * #include <wintab.h>
 * #define PACKETNAME     MOE
 * #define MOEPACKETDATA  PK_X | PK_Y | PK_BUTTONS  /@ x, y, buttons @/
 * #define MOEPACKETMODE  PK_BUTTONS                /@ buttons relative mode @/
 * #include <pktdef.h>
 * #define PACKETNAME       LARRY
 * #define LARRYPACKETDATA  PK_Y | PK_Z | PK_BUTTONS  /@ y, z, buttons @/
 * #define LARRYPACKETMODE  PK_BUTTONS                /@ buttons relative mode @/
 * #include <pktdef.h>
 * #define PACKETNAME       CURLY
 * #define CURLYPACKETDATA  PK_X | PK_Z | PK_BUTTONS  /@ x, z, buttons @/
 * #define CURLYPACKETMODE  PK_BUTTONS                /@ buttons relative mode @/
 * #include <pktdef.h>
 * ...
 *     lcMOE.lcPktData = MOEPACKETDATA;
 *     lcMOE.lcPktMode = MOEPACKETMODE;
 * ...
 *     lcLARRY.lcPktData = LARRYPACKETDATA;
 *     lcLARRY.lcPktMode = LARRYPACKETMODE;
 * ...
 *     lcCURLY.lcPktData = CURLYPACKETDATA;
 *     lcCURLY.lcPktMode = CURLYPACKETMODE;
 *
 * Example #3. -- extension packet data "XFOO".
 *
 * #include <wintab.h>
 * #define PACKETDATA  PK_X | PK_Y | PK_BUTTONS  /@ x, y, buttons @/
 * #define PACKETMODE  PK_BUTTONS                /@ buttons relative mode @/
 * #define PACKETXFOO  PKEXT_ABSOLUTE            /@ XFOO absolute mode @/
 * #include <pktdef.h>
 * ...
 * UINT ScanExts(UINT wTag)
 * {
 *     UINT i;
 *     UINT wScanTag;
 * 
 *     /@ scan for wTag's info category. @/
 *     for (i = 0; WTInfo(WTI_EXTENSIONS + i, EXT_TAG, &wScanTag); i++) {
 *          if (wTag == wScanTag) {
 *             /@ return category offset from WTI_EXTENSIONS. @/
 *             return i;
 *         }
 *     }
 *     /@ return error code. @/
 *     return 0xFFFF;
 * }
 * ...
 *     lc.lcPktData = PACKETDATA;
 *     lc.lcPktMode = PACKETMODE;
 * #ifdef PACKETXFOO
 *     categoryXFOO = ScanExts(WTX_XFOO);
 *     WTInfo(WTI_EXTENSIONS + categoryXFOO, EXT_MASK, &maskXFOO);
 *    lc.lcPktData |= maskXFOO;
 * #if PACKETXFOO == PKEXT_RELATIVE
 *     lc.lcPktMode |= maskXFOO;
 * #endif
 * #endif
 *    WTOpen(hWnd, &lc, TRUE);
 */

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

#ifndef PACKETNAME
/* if no packet name prefix */
# define __PFX(x)    x
# define __IFX(x,y)  x ## y
#else
/* add prefixes and infixes to packet format names */
# define __PFX(x)       __PFX2(PACKETNAME,x)
# define __PFX2(p,x)    __PFX3(p,x)
# define __PFX3(p,x)    p ## x
# define __IFX(x,y)     __IFX2(x,PACKETNAME,y)
# define __IFX2(x,i,y)  __IFX3(x,i,y)
# define __IFX3(x,i,y)  x ## i ## y
#endif

#define __SFX2(x,s)  __SFX3(x,s)
#define __SFX3(x,s)  x ## s

#define __TAG  __IFX(tag,PACKET)
#define __TYPES \
  __PFX(PACKET), * __IFX(P,PACKET), \
  * __IFX(NP,PACKET), * __IFX(LP,PACKET)

#define __DATA    (__PFX(PACKETDATA))
#define __MODE    (__PFX(PACKETMODE))
#define __EXT(x)  __SFX2(__PFX(PACKET),x)

typedef struct __TAG {
#if (__DATA & PK_CONTEXT)
    HCTX pkContext;
#endif
#if (__DATA & PK_STATUS)
    UINT pkStatus;
#endif
#if (__DATA & PK_TIME)
    DWORD pkTime;
#endif
#if (__DATA & PK_CHANGED)
    WTPKT pkChanged;
#endif
#if (__DATA & PK_SERIAL_NUMBER)
    UINT pkSerialNumber;
#endif
#if (__DATA & PK_CURSOR)
    UINT pkCursor;
#endif
#if (__DATA & PK_BUTTONS)
    DWORD pkButtons;
#endif
#if (__DATA & PK_X)
    LONG pkX;
#endif
#if (__DATA & PK_Y)
    LONG pkY;
#endif
#if (__DATA & PK_Z)
    LONG pkZ;
#endif
#if (__DATA & PK_NORMAL_PRESSURE)
# if (__MODE & PK_NORMAL_PRESSURE)
    /* relative */
    int pkNormalPressure;
# else
    /* absolute */
    UINT pkNormalPressure;
# endif
#endif
#if (__DATA & PK_TANGENT_PRESSURE)
# if (__MODE & PK_TANGENT_PRESSURE)
    /* relative */
    int pkTangentPressure;
# else
    /* absolute */
    UINT pkTangentPressure;
# endif
#endif
#if (__DATA & PK_ORIENTATION)
    ORIENTATION pkOrientation;
#endif
#if (__DATA & PK_ROTATION)
    ROTATION pkRotation;  /* 1.1 */
#endif

#ifndef NOWTEXTENSIONS
    /* extensions begin here. */

#if (__EXT(FKEYS) == PKEXT_RELATIVE) || (__EXT(FKEYS) == PKEXT_ABSOLUTE)
    UINT pkFKeys;
#endif
#if (__EXT(TILT) == PKEXT_RELATIVE) || (__EXT(TILT) == PKEXT_ABSOLUTE)
    TILT pkTilt;
#endif

#endif

} __TYPES;

#undef PACKETNAME
#undef __TAG
#undef __TAG2
#undef __TYPES
#undef __TYPES2
#undef __DATA
#undef __MODE
#undef __PFX
#undef __PFX2
#undef __PFX3
#undef __IFX
#undef __IFX2
#undef __IFX3
#undef __SFX2
#undef __SFX3

#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* defined(__WINE_PKTDEF_H */
