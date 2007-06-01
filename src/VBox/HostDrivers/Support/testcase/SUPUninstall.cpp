/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Testcase for driver uninstall
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/sup.h>
#include <VBox/err.h>
#include <iprt/runtime.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    RTR3Init(false);
    int rc = SUPUninstall();
    if (VBOX_SUCCESS(rc))
    {
        printf("uninstalled successfully\n");
        return 0;
    }
    printf("uninstallation failed. rc=%d\n", rc);

    return 1;
}
