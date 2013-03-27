/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIVMCloseDialog class implementation
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
/* Qt includes: */
#include <QPushButton>

/* GUI includes: */
#include "UIVMCloseDialog.h"
#include "UIMessageCenter.h"
#include "UIMachineWindowNormal.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIVMCloseDialog::UIVMCloseDialog(QWidget *pParent)
    : QIWithRetranslateUI<QIDialog>(pParent)
{
    /* Apply UI decorations: */
    Ui::UIVMCloseDialog::setupUi(this);

#ifdef Q_WS_MAC
    /* Add more space around the content: */
    hboxLayout->setContentsMargins(40, 0, 40, 0);
    vboxLayout2->insertSpacing(1, 20);
    /* And more space between the radio buttons: */
    gridLayout->setSpacing(15);
#endif /* Q_WS_MAC */

    /* Configure default button connections: */
    connect(mButtonBox, SIGNAL(helpRequested()),
            &msgCenter(), SLOT(sltShowHelpHelpDialog()));
}

void UIVMCloseDialog::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIVMCloseDialog::retranslateUi(this);
}

void UIVMCloseDialog::polishEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QIDialog::polishEvent(pEvent);

    /* Make the dialog-size fixed: */
    setFixedSize(size());
}

