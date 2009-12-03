/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxNewVMWzd class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

/* VBox includes */
#include "VBoxNewVMWzd.h"
#include "VBoxVMSettingsHD.h"
#include "VBoxUtils.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxNewHDWzd.h"
#include "VBoxMediaManagerDlg.h"

VBoxNewVMWzd::VBoxNewVMWzd (QWidget *aParent)
    : QIWithRetranslateUI<QIAbstractWizard> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxNewVMWzd::setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Name and OS page */
    mLeName->setValidator (new QRegExpValidator (QRegExp (".+"), this));

    mWvalNameAndOS = new QIWidgetValidator (mPageNameAndOS, this);
    connect (mWvalNameAndOS, SIGNAL (validityChanged (const QIWidgetValidator*)),
             this, SLOT (enableNext (const QIWidgetValidator*)));
    connect (mOSTypeSelector, SIGNAL (osTypeChanged()), this, SLOT (onOSTypeChanged()));

    /* Memory page */
    mLeRAM->setFixedWidthByText ("99999");
    mLeRAM->setValidator (new QIntValidator (mSlRAM->minRAM(), mSlRAM->maxRAM(), this));

    mWvalMemory = new QIWidgetValidator (mPageMemory, this);
    connect (mWvalMemory, SIGNAL (validityChanged (const QIWidgetValidator*)),
             this, SLOT (enableNext (const QIWidgetValidator*)));
    connect (mWvalMemory, SIGNAL (isValidRequested (QIWidgetValidator*)),
             this, SLOT (revalidate (QIWidgetValidator*)));
    connect (mSlRAM, SIGNAL (valueChanged (int)),
             this, SLOT (slRAMValueChanged (int)));
    connect (mLeRAM, SIGNAL (textChanged (const QString&)),
             this, SLOT (leRAMTextChanged (const QString&)));

    /* HDD Images page */
    QStyleOptionButton options;
    options.initFrom (mNewVDIRadio);
    QGridLayout *hdLayout = qobject_cast <QGridLayout*> (mGbHDA->layout());
    int wid = mNewVDIRadio->style()->subElementRect (QStyle::SE_RadioButtonIndicator, &options, mNewVDIRadio).width() +
              mNewVDIRadio->style()->pixelMetric (QStyle::PM_RadioButtonLabelSpacing, &options, mNewVDIRadio) -
              hdLayout->spacing() - 1;
    QSpacerItem *spacer = new QSpacerItem (wid, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    hdLayout->addItem (spacer, 2, 0);
    mHDCombo->setType (VBoxDefs::MediumType_HardDisk);
    mHDCombo->repopulate();
    mTbVmm->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png",
                                          ":/select_file_dis_16px.png"));

    mWvalHDD = new QIWidgetValidator (mPageHDD, this);
    connect (mWvalHDD, SIGNAL (validityChanged (const QIWidgetValidator*)),
             this, SLOT (enableNext (const QIWidgetValidator*)));
    connect (mWvalHDD, SIGNAL (isValidRequested (QIWidgetValidator*)),
             this, SLOT (revalidate (QIWidgetValidator*)));
    connect (mGbHDA, SIGNAL (toggled (bool)), mWvalHDD, SLOT (revalidate()));
    connect (mNewVDIRadio, SIGNAL (toggled (bool)), this, SLOT (hdTypeChanged()));
    connect (mExistRadio, SIGNAL (toggled (bool)), this, SLOT (hdTypeChanged()));
    connect (mHDCombo, SIGNAL (currentIndexChanged (int)),
             mWvalHDD, SLOT (revalidate()));
    connect (mTbVmm, SIGNAL (clicked()), this, SLOT (showMediaManager()));

    /* Name and OS page */
    onOSTypeChanged();

    /* Memory page */
    slRAMValueChanged (mSlRAM->value());

    /* HDD Images page */
    hdTypeChanged();

    /* Initial revalidation */
    mWvalNameAndOS->revalidate();
    mWvalMemory->revalidate();
    mWvalHDD->revalidate();

    /* Initialize wizard ftr */
    initializeWizardFtr();

    retranslateUi();
}

VBoxNewVMWzd::~VBoxNewVMWzd()
{
    ensureNewHardDiskDeleted();
}

const CMachine& VBoxNewVMWzd::machine() const
{
    return mMachine;
}

void VBoxNewVMWzd::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxNewVMWzd::retranslateUi (this);

    CGuestOSType type = mOSTypeSelector->type();

    mTextRAMBest->setText (
        tr ("The recommended base memory size is <b>%1</b> MB.")
            .arg (type.GetRecommendedRAM()));
    mTextVDIBest->setText (
        tr ("The recommended size of the boot hard disk is <b>%1</b> MB.")
            .arg (type.GetRecommendedHDD()));

    mTxRAMMin->setText (QString ("<qt>%1&nbsp;%2</qt>")
                        .arg (mSlRAM->minRAM()).arg (tr ("MB", "megabytes")));
    mTxRAMMax->setText (QString ("<qt>%1&nbsp;%2</qt>")
                        .arg (mSlRAM->maxRAM()).arg (tr ("MB", "megabytes")));

    QWidget *page = mPageStack->currentWidget();

    if (page == mPageSummary)
    {
        /* Compose summary */
        QString summary = QString (
            "<tr><td><nobr>%1:&nbsp;</nobr></td><td>%2</td></tr>"
            "<tr><td><nobr>%3:&nbsp;</nobr></td><td>%4</td></tr>"
            "<tr><td><nobr>%5:&nbsp;</nobr></td><td>%6&nbsp;%7</td></tr>")
            .arg (tr ("Name", "summary"), mLeName->text())
            .arg (tr ("OS Type", "summary"), type.GetDescription())
            .arg (tr ("Base Memory", "summary")).arg (mSlRAM->value())
            .arg (tr ("MB", "megabytes"));

        if (mGbHDA->isChecked() && !mHDCombo->id().isNull())
            summary += QString (
                "<tr><td><nobr>%8:&nbsp;</nobr></td><td><nobr>%9</nobr></td></tr>")
                .arg (tr ("Boot Hard Disk", "summary"), mHDCombo->currentText());

        mTeSummary->setText ("<table>" + summary + "</table>");
    }
}

void VBoxNewVMWzd::accept()
{
    /* Try to create the machine when the Finish button is pressed.
     * On failure, the wisard will remain open to give it another try. */
    if (constructMachine())
        QDialog::accept();
}

void VBoxNewVMWzd::showMediaManager()
{
    VBoxMediaManagerDlg dlg (this);
    dlg.setup (VBoxDefs::MediumType_HardDisk, true);

    if (dlg.exec() == QDialog::Accepted)
    {
        QString newId = dlg.selectedId();
        if (mHDCombo->id() != newId)
        {
            ensureNewHardDiskDeleted();
            mHDCombo->setCurrentItem (newId);
        }
    }

    mHDCombo->setFocus();
}

void VBoxNewVMWzd::onOSTypeChanged()
{
    slRAMValueChanged (mOSTypeSelector->type().GetRecommendedRAM());
}

void VBoxNewVMWzd::slRAMValueChanged (int aValue)
{
    mLeRAM->setText (QString().setNum (aValue));
}

void VBoxNewVMWzd::leRAMTextChanged (const QString &aText)
{
    mSlRAM->setValue (aText.toInt());
}

void VBoxNewVMWzd::hdTypeChanged()
{
    mHDCombo->setEnabled (mExistRadio->isChecked());
    mTbVmm->setEnabled (mExistRadio->isChecked());
    if (mExistRadio->isChecked())
        mHDCombo->setFocus();

    mWvalHDD->revalidate();
}

void VBoxNewVMWzd::revalidate (QIWidgetValidator *aWval)
{
    /* Get common result of validators */
    bool valid = aWval->isOtherValid();

    /* Do individual validations for pages */
    if (aWval->widget() == mPageMemory)
    {
        valid = true;
        if (mSlRAM->value() > (int)mSlRAM->maxRAMAlw())
            valid = false;
    }
    else if (aWval->widget() == mPageHDD)
    {
        valid = true;
        if (    (mGbHDA->isChecked())
            &&  (vboxGlobal().findMedium (mHDCombo->id()).isNull())
            &&  (mExistRadio->isChecked()))
        {
            valid = false;
        }
    }

    aWval->setOtherValid (valid);
}

void VBoxNewVMWzd::enableNext (const QIWidgetValidator *aWval)
{
    nextButton (aWval->widget())->setEnabled (aWval->isValid());
}

void VBoxNewVMWzd::onPageShow()
{
    /* Make sure all is properly translated & composed */
    retranslateUi();

    QWidget *page = mPageStack->currentWidget();

    if (page == mPageWelcome)
        nextButton (page)->setFocus();
    else if (page == mPageNameAndOS)
        mLeName->setFocus();
    else if (page == mPageMemory)
        mSlRAM->setFocus();
    else if (page == mPageHDD)
        mHDCombo->setFocus();
    else if (page == mPageSummary)
        mTeSummary->setFocus();

    if (page == mPageSummary)
        finishButton()->setDefault (true);
    else
        nextButton (page)->setDefault (true);
}

void VBoxNewVMWzd::showBackPage()
{
    /* Delete temporary HD if present */
    if (sender() == mBtnBack5)
        ensureNewHardDiskDeleted();

    /* Switch to the back page */
    QIAbstractWizard::showBackPage();
}

void VBoxNewVMWzd::showNextPage()
{
    /* Ask user about disk-less machine */
    if (sender() == mBtnNext4 && !mGbHDA->isChecked() &&
        !vboxProblem().confirmHardDisklessMachine (this))
        return;

    /* Show the New Hard Disk wizard */
    if (sender() == mBtnNext4 && mGbHDA->isChecked() &&
        mNewVDIRadio->isChecked() && !showNewHDWizard())
        return;

    /* Switch to the next page */
    QIAbstractWizard::showNextPage();
}


bool VBoxNewVMWzd::showNewHDWizard()
{
    VBoxNewHDWzd dlg (this);

    dlg.setRecommendedFileName (mLeName->text());
    dlg.setRecommendedSize (mOSTypeSelector->type().GetRecommendedHDD());

    if (dlg.exec() == QDialog::Accepted)
    {
        ensureNewHardDiskDeleted();
        mHardDisk = dlg.hardDisk();
        mHDCombo->setCurrentItem (mHardDisk.GetId());
        return true;
    }

    return false;
}

bool VBoxNewVMWzd::constructMachine()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* OS type */
    CGuestOSType type = mOSTypeSelector->type();
    AssertMsg (!type.isNull(), ("vmGuestOSType() must return non-null type"));
    QString typeId = type.GetId();

    /* Create a machine with the default settings file location */
    if (mMachine.isNull())
    {
        mMachine = vbox.CreateMachine (mLeName->text(), typeId, QString::null, QString::null);
        if (!vbox.isOk())
        {
            vboxProblem().cannotCreateMachine (vbox, this);
            return false;
        }

        /* The FirstRun wizard is to be shown only when we don't attach any hard
         * disk or attach a new (empty) one. Selecting an existing hard disk
         * will cancel the wizard. */
        if (!mGbHDA->isChecked() || !mHardDisk.isNull())
            mMachine.SetExtraData (VBoxDefs::GUI_FirstRun, "yes");
    }

    /* RAM size */
    mMachine.SetMemorySize (mSlRAM->value());

    /* VRAM size - select maximum between recommended and minimum for fullscreen */
    mMachine.SetVRAMSize (qMax (type.GetRecommendedVRAM(),
                                (ULONG) (VBoxGlobal::requiredVideoMemory(&mMachine) / _1M)));

    /* Enabling audio by default */
    mMachine.GetAudioAdapter().SetEnabled (true);

    /* Enable the OHCI and EHCI controller by default for new VMs. (new in 2.2) */
    CUSBController usbController = mMachine.GetUSBController();
    if (!usbController.isNull())
    {
        usbController.SetEnabled (true);
        usbController.SetEnabledEhci (true);
    }

    /* Create default storage controllers */
    QString ideCtrName = VBoxVMSettingsHD::tr ("IDE Controller");
    QString floppyCtrName = VBoxVMSettingsHD::tr ("Floppy Controller");
    KStorageBus ideBus = KStorageBus_IDE;
    KStorageBus floppyBus = KStorageBus_Floppy;
    mMachine.AddStorageController (ideCtrName, ideBus);
    mMachine.AddStorageController (floppyCtrName, floppyBus);

    /* Register the VM prior to attaching hard disks */
    vbox.RegisterMachine (mMachine);
    if (!vbox.isOk())
    {
        vboxProblem().cannotCreateMachine (vbox, mMachine, this);
        return false;
    }

    /* Attach default devices */
    {
        bool success = false;
        QString machineId = mMachine.GetId();
        CSession session = vboxGlobal().openSession (machineId);
        if (!session.isNull())
        {
            CMachine m = session.GetMachine();

            /* Boot hard disk (IDE Primary Master) */
            if (mGbHDA->isChecked())
            {
                m.AttachDevice (ideCtrName, 0, 0, KDeviceType_HardDisk, mHDCombo->id());
                if (!m.isOk())
                    vboxProblem().cannotAttachDevice (this, m, VBoxDefs::MediumType_HardDisk, mHDCombo->location(), ideBus, 0, 0);
            }

            /* Attach empty CD/DVD ROM Device */
            m.AttachDevice (ideCtrName, 1, 0, KDeviceType_DVD, QString(""));
            if (!m.isOk())
                vboxProblem().cannotAttachDevice (this, m, VBoxDefs::MediumType_DVD, QString(), ideBus, 1, 0);

            /* Attach empty Floppy Device */
            m.AttachDevice (floppyCtrName, 0, 0, KDeviceType_Floppy, QString(""));
            if (!m.isOk())
                vboxProblem().cannotAttachDevice (this, m, VBoxDefs::MediumType_Floppy, QString(), floppyBus, 0, 0);

            if (m.isOk())
            {
                m.SaveSettings();
                if (m.isOk())
                    success = true;
                else
                    vboxProblem().cannotSaveMachineSettings (m, this);
            }

            session.Close();
        }
        if (!success)
        {
            /* Unregister on failure */
            vbox.UnregisterMachine (machineId);
            if (vbox.isOk())
                mMachine.DeleteSettings();
            return false;
        }
    }

    /* Ensure we don't try to delete a newly created hard disk on success */
    mHardDisk.detach();

    return true;
}

void VBoxNewVMWzd::ensureNewHardDiskDeleted()
{
    if (!mHardDisk.isNull())
    {
        /* Remember ID as it may be lost after the deletion */
        QString id = mHardDisk.GetId();

        bool success = false;

        CProgress progress = mHardDisk.DeleteStorage();
        if (mHardDisk.isOk())
        {
            vboxProblem().showModalProgressDialog (progress, windowTitle(),
                                                   parentWidget());
            if (progress.isOk() && progress.GetResultCode() == S_OK)
                success = true;
        }

        if (success)
            vboxGlobal().removeMedium (VBoxDefs::MediumType_HardDisk, id);
        else
            vboxProblem().cannotDeleteHardDiskStorage (this, mHardDisk,
                                                       progress);
        mHardDisk.detach();
    }
}

