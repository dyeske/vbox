/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxCloseVMDlg class implementation
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
#include "VBoxCloseVMDlg.h"
#include "VBoxProblemReporter.h"
#ifdef Q_WS_MAC
# include "VBoxConsoleWnd.h"
#endif /* Q_WS_MAC */

/* Qt includes */
#include <QPushButton>
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#ifdef VBOX_WITH_NEW_RUNTIME_CORE
# include "UIMachineWindowNormal.h"
#endif /* !VBOX_WITH_NEW_RUNTIME_CORE */

VBoxCloseVMDlg::VBoxCloseVMDlg (QWidget *aParent)
    : QIWithRetranslateUI<QIDialog> (aParent)
{
#ifdef Q_WS_MAC
# ifdef VBOX_WITH_NEW_RUNTIME_CORE
    /* No sheets in another mode than normal for now. Firstly it looks ugly and
     * secondly in some cases it is broken. */
    UIMachineWindowNormal *pWnd = qobject_cast<UIMachineWindowNormal*>(aParent);
    if (pWnd)
        setWindowFlags (Qt::Sheet);
# endif /* !VBOX_WITH_NEW_RUNTIME_CORE */
# ifndef VBOX_FORCE_NEW_RUNTIME_CORE_ALWAYS
    /* Sheets are broken if the window is in fullscreen mode. So make it a
     * normal window in that case. */
    VBoxConsoleWnd *cwnd = qobject_cast<VBoxConsoleWnd*> (aParent);
    if (cwnd == NULL ||
        (!cwnd->isTrueFullscreen() &&
         !cwnd->isTrueSeamless()))
        setWindowFlags (Qt::Sheet);
# endif /* VBOX_FORCE_NEW_RUNTIME_CORE_ALWAYS */
#endif /* Q_WS_MAC */

    /* Apply UI decorations */
    Ui::VBoxCloseVMDlg::setupUi (this);

#ifdef Q_WS_MAC
    /* Make some more space around the content */
    hboxLayout->setContentsMargins (40, 0, 40, 0);
    vboxLayout2->insertSpacing (1, 20);
    /* and more space between the radio buttons */
    gridLayout->setSpacing (15);
#endif /* Q_WS_MAC */
    /* Set fixed size */
    setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect (mButtonBox, SIGNAL (helpRequested()),
             &vboxProblem(), SLOT (showHelpHelpDialog()));
}

void VBoxCloseVMDlg::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxCloseVMDlg::retranslateUi (this);
}

