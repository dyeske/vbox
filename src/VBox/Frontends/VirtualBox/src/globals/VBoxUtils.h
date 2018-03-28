/* $Id$ */
/** @file
 * VBox Qt GUI - Declarations of utility classes and functions.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxUtils_h___
#define ___VBoxUtils_h___

/* Qt includes: */
#include <QMouseEvent>
#include <QWidget>
#include <QTextBrowser>

/* GUI includes: */
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/* Other VBox includes: */
#include <iprt/types.h>


/** QObject subclass,
  * allowing to apply string-property value for a certain QObject. */
class QObjectPropertySetter : public QObject
{
    Q_OBJECT;

public:

    /** Constructs setter for a property with certain @a strName, passing @a pParent to the base-class. */
    QObjectPropertySetter(QObject *pParent, const QString &strName)
        : QObject(pParent), m_strName(strName)
    {}

public slots:

    /** Assigns string property @a strValue. */
    void sltAssignProperty(const QString &strValue)
    {
        parent()->setProperty(m_strName.toLatin1().constData(), strValue);
    }

private:

    /** Holds the property name. */
    const QString m_strName;
};


/** Simple class,
  * containing functionality to (de)serialize proxy settings. */
class UIProxyManager
{
public:

    /** Proxy states. */
    enum ProxyState
    {
        ProxyState_Disabled,
        ProxyState_Enabled,
        ProxyState_Auto
    };

    /** Constructs proxy-manager which parses @a strProxySettings. */
    UIProxyManager(const QString &strProxySettings = QString())
        : m_enmProxyState(ProxyState_Auto)
        , m_fAuthEnabled(false)
    {
        /* Parse proxy settings: */
        if (strProxySettings.isEmpty())
            return;
        QStringList proxySettings = strProxySettings.split(",");

        /* Parse proxy state, host and port: */
        if (proxySettings.size() > 0)
            m_enmProxyState = proxyStateFromString(proxySettings[0]);
        if (proxySettings.size() > 1)
            m_strProxyHost = proxySettings[1];
        if (proxySettings.size() > 2)
            m_strProxyPort = proxySettings[2];

        /* Parse whether proxy auth enabled and has login/password: */
        if (proxySettings.size() > 3)
            m_fAuthEnabled = proxySettings[3] == "authEnabled";
        if (proxySettings.size() > 4)
            m_strAuthLogin = proxySettings[4];
        if (proxySettings.size() > 5)
            m_strAuthPassword = proxySettings[5];
    }

    /** Serializes proxy settings. */
    QString toString() const
    {
        /* Serialize settings: */
        QString strResult;
        if (m_enmProxyState != ProxyState_Auto || !m_strProxyHost.isEmpty() || !m_strProxyPort.isEmpty() ||
            m_fAuthEnabled || !m_strAuthLogin.isEmpty() || !m_strAuthPassword.isEmpty())
        {
            QStringList proxySettings;
            proxySettings << proxyStateToString(m_enmProxyState);
            proxySettings << m_strProxyHost;
            proxySettings << m_strProxyPort;
            proxySettings << QString(m_fAuthEnabled ? "authEnabled" : "authDisabled");
            proxySettings << m_strAuthLogin;
            proxySettings << m_strAuthPassword;
            strResult = proxySettings.join(",");
        }
        return strResult;
    }

    /** Returns the proxy state. */
    ProxyState proxyState() const { return m_enmProxyState; }
    /** Returns the proxy host. */
    const QString &proxyHost() const { return m_strProxyHost; }
    /** Returns the proxy port. */
    const QString &proxyPort() const { return m_strProxyPort; }

    /** Returns whether the proxy auth is enabled. */
    bool authEnabled() const { return m_fAuthEnabled; }
    /** Returns the proxy auth login. */
    const QString &authLogin() const { return m_strAuthLogin; }
    /** Returns the proxy auth password. */
    const QString &authPassword() const { return m_strAuthPassword; }

    /** Defines the proxy @a enmState. */
    void setProxyState(ProxyState enmState) { m_enmProxyState = enmState; }
    /** Defines the proxy @a strHost. */
    void setProxyHost(const QString &strHost) { m_strProxyHost = strHost; }
    /** Defines the proxy @a strPort. */
    void setProxyPort(const QString &strPort) { m_strProxyPort = strPort; }

    /** Defines whether the proxy auth is @a fEnabled. */
    void setAuthEnabled(bool fEnabled) { m_fAuthEnabled = fEnabled; }
    /** Defines the proxy auth @a strLogin. */
    void setAuthLogin(const QString &strLogin) { m_strAuthLogin = strLogin; }
    /** Defines the proxy auth @a strPassword. */
    void setAuthPassword(const QString &strPassword) { m_strAuthPassword = strPassword; }

private:

    /** Converts passed @a enmState to corresponding #QString. */
    static QString proxyStateToString(ProxyState enmState)
    {
        switch (enmState)
        {
            case ProxyState_Disabled: return QString("ProxyDisabled");
            case ProxyState_Enabled:  return QString("ProxyEnabled");
            case ProxyState_Auto:     break;
        }
        return QString("ProxyAuto");
    }

    /** Converts passed @a strState to corresponding #ProxyState. */
    static ProxyState proxyStateFromString(const QString &strState)
    {
        /* Compose the map of known states: */
        QMap<QString, ProxyState> states;
        states["ProxyDisabled"] = ProxyState_Disabled; // New since VBox 5.0
        states["proxyEnabled"]  = ProxyState_Enabled;  // Old since VBox 4.1
        states["ProxyEnabled"]  = ProxyState_Enabled;  // New since VBox 5.0
        /* Return one of registered or 'Auto' by default: */
        return states.value(strState, ProxyState_Auto);
    }

    /** Holds the proxy state. */
    ProxyState  m_enmProxyState;
    /** Holds the proxy host. */
    QString     m_strProxyHost;
    /** Holds the proxy port. */
    QString     m_strProxyPort;

    /** Holds whether the proxy auth is enabled. */
    bool     m_fAuthEnabled;
    /** Holds the proxy auth login. */
    QString  m_strAuthLogin;
    /** Holds the proxy auth password. */
    QString  m_strAuthPassword;
};


#endif /* !___VBoxUtils_h___ */

