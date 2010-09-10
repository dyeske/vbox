#!/bin/sh
## @file
#
# VirtualBox postinstall script for Solaris.
#

#
# Copyright (C) 2007-2010 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

currentzone=`zonename`
if test "$currentzone" = "global"; then
    ${BASEDIR}/opt/VirtualBox/pkginstall.sh --srv4
    rc=$?
fi

# installf inherits ${PKG_INSTALL_ROOT} from pkgadd, no need to explicitly specify
/usr/sbin/installf -f $PKGINST

# return 20 = requires reboot, 2 = partial failure, 0  = success
exit "$rc"

