/*
 * Copyright 2004 Jon Griffiths
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

#ifndef MAPIGUID_H
#define MAPIGUID_H

#define DEFINE_MAPIGUID(n,l,w1,w2) DEFINE_OLEGUID(n,l,w1,w2)

DEFINE_MAPIGUID(IID_IABContainer,0x2030D,0,0);
DEFINE_MAPIGUID(IID_IABLogon,0x20314,0,0);
DEFINE_MAPIGUID(IID_IABProvider,0x20311,0,0);
DEFINE_MAPIGUID(IID_IAddrBook,0x20309,0,0);
DEFINE_MAPIGUID(IID_IAttachment,0x20308,0,0);
DEFINE_MAPIGUID(IID_IDistList,0x2030E,0,0);
DEFINE_MAPIGUID(IID_IEnumMAPIFormProp,0x20323,0,0);
DEFINE_MAPIGUID(IID_IMailUser,0x2030A,0,0);
DEFINE_MAPIGUID(IID_IMAPIAdviseSink,0x20302,0,0);
DEFINE_MAPIGUID(IID_IMAPIContainer,0x2030B,0,0);
DEFINE_MAPIGUID(IID_IMAPIControl,0x2031B,0,0);
DEFINE_MAPIGUID(IID_IMAPIFolder,0x2030C,0,0);
DEFINE_MAPIGUID(IID_IMAPIForm,0x20327,0,0);
DEFINE_MAPIGUID(IID_IMAPIFormAdviseSink,0x2032F,0,0);
DEFINE_MAPIGUID(IID_IMAPIFormContainer,0x2032E,0,0);
DEFINE_MAPIGUID(IID_IMAPIFormFactory,0x20350,0,0);
DEFINE_MAPIGUID(IID_IMAPIFormInfo,0x20324,0,0);
DEFINE_MAPIGUID(IID_IMAPIFormMgr,0x20322,0,0);
DEFINE_MAPIGUID(IID_IMAPIFormProp,0x2032D,0,0);
DEFINE_MAPIGUID(IID_IMAPIMessageSite,0x20370,0,0);
DEFINE_MAPIGUID(IID_IMAPIProgress,0x2031F,0,0);
DEFINE_MAPIGUID(IID_IMAPIProp,0x20303,0,0);
DEFINE_MAPIGUID(IID_IMAPIPropData,0x2031A,0,0);
DEFINE_MAPIGUID(IID_IMAPISession,0x20300,0,0);
DEFINE_MAPIGUID(IID_IMAPISpoolerInit,0x20317,0,0);
DEFINE_MAPIGUID(IID_IMAPISpoolerService,0x2031E,0,0);
DEFINE_MAPIGUID(IID_IMAPISpoolerSession,0x20318,0,0);
DEFINE_MAPIGUID(IID_IMAPIStatus,0x20305,0,0);
DEFINE_MAPIGUID(IID_IMAPISup,0x2030F,0,0);
DEFINE_MAPIGUID(IID_IMAPITable,0x20301,0,0);
DEFINE_MAPIGUID(IID_IMAPITableData,0x20316,0,0);
DEFINE_MAPIGUID(IID_IMAPIViewAdviseSink,0x2032B,0,0);
DEFINE_MAPIGUID(IID_IMAPIViewContext,0x20321,0,0);
DEFINE_MAPIGUID(IID_IMessage,0x20307,0,0);
DEFINE_MAPIGUID(IID_IMsgServiceAdmin,0x2031D,0,0);
DEFINE_MAPIGUID(IID_IMsgStore,0x20306,0,0);
DEFINE_MAPIGUID(IID_IMSLogon,0x20313,0,0);
DEFINE_MAPIGUID(IID_IMSProvider,0x20310,0,0);
DEFINE_MAPIGUID(IID_IPersistMessage,0x2032A,0,0);
DEFINE_MAPIGUID(IID_IProfAdmin,0x2031C,0,0);
DEFINE_MAPIGUID(IID_IProfSect,0x20304,0,0);
DEFINE_MAPIGUID(IID_IProviderAdmin,0x20325,0,0);
DEFINE_MAPIGUID(IID_ISpoolerHook,0x20320,0,0);
DEFINE_MAPIGUID(IID_IStreamDocfile,0x2032C,0,0);
DEFINE_MAPIGUID(IID_IStreamTnef,0x20330,0,0);
DEFINE_MAPIGUID(IID_ITNEF,0x20319,0,0);
DEFINE_MAPIGUID(IID_IXPLogon,0x20315,0,0);
DEFINE_MAPIGUID(IID_IXPProvider,0x20312,0,0);
DEFINE_MAPIGUID(MUID_PROFILE_INSTANCE,0x20385,0,0);
DEFINE_MAPIGUID(PS_MAPI,0x20328,0,0);
DEFINE_MAPIGUID(PS_PUBLIC_STRINGS,0x20329,0,0);
DEFINE_MAPIGUID(PS_ROUTING_ADDRTYPE,0x20381,0,0);
DEFINE_MAPIGUID(PS_ROUTING_DISPLAY_NAME,0x20382,0,0);
DEFINE_MAPIGUID(PS_ROUTING_EMAIL_ADDRESSES,0x20380,0,0);
DEFINE_MAPIGUID(PS_ROUTING_ENTRYID,0x20383,0,0);
DEFINE_MAPIGUID(PS_ROUTING_SEARCH_KEY,0x20384,0,0);

#endif/* MAPIGUID_H */
