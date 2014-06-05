/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowScale class implementation
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QDesktopWidget>
#include <QMenu>
#include <QTimer>
#include <QSpacerItem>
#include <QResizeEvent>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIExtraDataManager.h"
#include "UISession.h"
#include "UIMachineLogic.h"
#include "UIMachineWindowScale.h"
#ifdef Q_WS_WIN
# include "UIMachineView.h"
#endif /* Q_WS_WIN */
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include "UIImageTools.h"
#endif /* Q_WS_MAC */

UIMachineWindowScale::UIMachineWindowScale(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
    , m_pMainMenu(0)
{
}

void UIMachineWindowScale::sltPopupMainMenu()
{
    /* Popup main-menu if present: */
    if (m_pMainMenu && !m_pMainMenu->isEmpty())
    {
        m_pMainMenu->popup(geometry().center());
        QTimer::singleShot(0, m_pMainMenu, SLOT(sltHighlightFirstAction()));
    }
}

void UIMachineWindowScale::prepareMainLayout()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMainLayout();

    /* Strict spacers to hide them, they are not necessary for scale-mode: */
    m_pTopSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_pBottomSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_pLeftSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_pRightSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void UIMachineWindowScale::prepareMenu()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMenu();

    /* Prepare menu: */
    RuntimeMenuType restrictedMenus = gEDataManager->restrictedRuntimeMenuTypes(vboxGlobal().managedVMUuid());
    RuntimeMenuType allowedMenus = static_cast<RuntimeMenuType>(RuntimeMenuType_All ^ restrictedMenus);
    m_pMainMenu = uisession()->newMenu(allowedMenus);
}

#ifdef Q_WS_MAC
void UIMachineWindowScale::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

    /* Install the resize delegate for keeping the aspect ratio. */
    ::darwinInstallResizeDelegate(this);
    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }
}
#endif /* Q_WS_MAC */

void UIMachineWindowScale::loadSettings()
{
    /* Call to base-class: */
    UIMachineWindow::loadSettings();

    /* Load scale window settings: */
    CMachine m = machine();

    /* Load extra-data settings: */
    {
        /* Load extra-data: */
        QRect geo = gEDataManager->machineWindowGeometry(machineLogic()->visualStateType(),
                                                         m_uScreenId, vboxGlobal().managedVMUuid());

        /* If we do have proper geometry: */
        if (!geo.isNull())
        {
            /* Restore window geometry: */
            m_normalGeometry = geo;
            setGeometry(m_normalGeometry);

            /* Maximize (if necessary): */
            if (gEDataManager->isMachineWindowShouldBeMaximized(machineLogic()->visualStateType(),
                                                                m_uScreenId, vboxGlobal().managedVMUuid()))
                setWindowState(windowState() | Qt::WindowMaximized);
        }
        /* If we do NOT have proper geometry: */
        else
        {
            /* Get available geometry, for screen with (x,y) coords if possible: */
            QRect availableGeo = !geo.isNull() ? QApplication::desktop()->availableGeometry(QPoint(geo.x(), geo.y())) :
                                                 QApplication::desktop()->availableGeometry(this);

            /* Resize to default size: */
            resize(640, 480);
            /* Move newly created window to the screen-center: */
            m_normalGeometry = geometry();
            m_normalGeometry.moveCenter(availableGeo.center());
            setGeometry(m_normalGeometry);
        }
    }
}

void UIMachineWindowScale::saveSettings()
{
    /* Get machine: */
    CMachine m = machine();

    /* Save window geometry: */
    {
        gEDataManager->setMachineWindowGeometry(machineLogic()->visualStateType(),
                                                m_uScreenId, m_normalGeometry,
                                                isMaximizedChecked(), vboxGlobal().managedVMUuid());
    }

    /* Call to base-class: */
    UIMachineWindow::saveSettings();
}

#ifdef Q_WS_MAC
void UIMachineWindowScale::cleanupVisualState()
{
    /* Uninstall the resize delegate for keeping the aspect ratio. */
    ::darwinUninstallResizeDelegate(this);

    /* Call to base-class: */
    UIMachineWindow::cleanupVisualState();
}
#endif /* Q_WS_MAC */

void UIMachineWindowScale::cleanupMenu()
{
    /* Cleanup menu: */
    delete m_pMainMenu;
    m_pMainMenu = 0;

    /* Call to base-class: */
    UIMachineWindow::cleanupMenu();
}

void UIMachineWindowScale::showInNecessaryMode()
{
    /* Make sure this window should be shown at all: */
    if (!uisession()->isScreenVisible(m_uScreenId))
        return hide();

    /* Make sure this window is not minimized: */
    if (isMinimized())
        return;

    /* Show in normal mode: */
    show();
}

bool UIMachineWindowScale::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::Resize:
        {
            QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
            if (!isMaximizedChecked())
            {
                m_normalGeometry.setSize(pResizeEvent->size());
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position: */
                updateDbgWindows();
#endif /* VBOX_WITH_DEBUGGER_GUI */
            }
            break;
        }
        case QEvent::Move:
        {
            if (!isMaximizedChecked())
            {
                m_normalGeometry.moveTo(geometry().x(), geometry().y());
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position: */
                updateDbgWindows();
#endif /* VBOX_WITH_DEBUGGER_GUI */
            }
            break;
        }
        default:
            break;
    }
    return UIMachineWindow::event(pEvent);
}

#ifdef Q_WS_WIN
bool UIMachineWindowScale::winEvent(MSG *pMessage, long *pResult)
{
    /* Try to keep aspect ratio during window resize if:
     * 1. machine view exists and 2. event-type is WM_SIZING and 3. shift key is NOT pressed: */
    if (machineView() && pMessage->message == WM_SIZING && !(QApplication::keyboardModifiers() & Qt::ShiftModifier))
    {
        double dAspectRatio = machineView()->aspectRatio();
        if (dAspectRatio)
        {
            RECT *pRect = reinterpret_cast<RECT*>(pMessage->lParam);
            switch (pMessage->wParam)
            {
                case WMSZ_LEFT:
                case WMSZ_RIGHT:
                {
                    pRect->bottom = pRect->top + (double)(pRect->right - pRect->left) / dAspectRatio;
                    break;
                }
                case WMSZ_TOP:
                case WMSZ_BOTTOM:
                {
                    pRect->right = pRect->left + (double)(pRect->bottom - pRect->top) * dAspectRatio;
                    break;
                }
                case WMSZ_BOTTOMLEFT:
                case WMSZ_BOTTOMRIGHT:
                {
                    pRect->bottom = pRect->top + (double)(pRect->right - pRect->left) / dAspectRatio;
                    break;
                }
                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                {
                    pRect->top = pRect->bottom - (double)(pRect->right - pRect->left) / dAspectRatio;
                    break;
                }
                default:
                    break;
            }
        }
    }
    /* Call to base-class: */
    return UIMachineWindow::winEvent(pMessage, pResult);
}
#endif /* Q_WS_WIN */

bool UIMachineWindowScale::isMaximizedChecked()
{
#ifdef Q_WS_MAC
    /* On the Mac the WindowStateChange signal doesn't seems to be delivered
     * when the user get out of the maximized state. So check this ourself. */
    return ::darwinIsWindowMaximized(this);
#else /* Q_WS_MAC */
    return isMaximized();
#endif /* !Q_WS_MAC */
}

