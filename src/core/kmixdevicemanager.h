/*
 * KMix -- KDE's full featured mini mixer
 *
 * Copyright 2006-2007 Christian Esken <esken@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef kmixdevicemanager_h
#define kmixdevicemanager_h

#include <QObject>
#include <QtCore/QStringList>

class KMixDeviceManager : public QObject
{
    Q_OBJECT

public:
    static KMixDeviceManager* instance();
    void initHotplug();
    void setHotpluggingBackends(const QStringList& backends) { _hotpluggingBackends = backends; }
    QString getUDI_ALSA(int num);
    QString getUDI_OSS(const QString& devname);

signals:
    void plugged(const char* driverName, const QString& udi, QString& dev);
    void unplugged(const QString& udi);

private:
    KMixDeviceManager();
    virtual ~KMixDeviceManager();
    QStringList _hotpluggingBackends;

private slots:
    void pluggedSlot(const QString&);
    void unpluggedSlot(const QString&);

private:
    static KMixDeviceManager* s_KMixDeviceManager;
};

#endif

