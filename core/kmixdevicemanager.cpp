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

#include "core/kmixdevicemanager.h"

#include <QRegExp>
#include <QRegularExpression>
#include <QString>
#include <QTimer>

#include "kmix_debug.h"

#include <solid/device.h>
#include <solid/devicenotifier.h>


static const int HOTPLUG_DELAY = 500;			// settling delay, milliseconds

KMixDeviceManager* KMixDeviceManager::s_KMixDeviceManager = 0;


KMixDeviceManager* KMixDeviceManager::instance()
{
    if ( s_KMixDeviceManager == 0 ) {
        s_KMixDeviceManager = new KMixDeviceManager();
    }
    return s_KMixDeviceManager;
}

void KMixDeviceManager::initHotplug()
{
    qCDebug(KMIX_LOG);
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceAdded,
            this, &KMixDeviceManager::pluggedSlot);
    connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved,
            this, &KMixDeviceManager::unpluggedSlot);
}


QString KMixDeviceManager::getUDI_ALSA(int num)
{
	return (QString("hw:%1").arg(num));
}


QString KMixDeviceManager::getUDI_OSS(const QString &devname)
{
	return (devname);
}


static bool isSoundDevice(const QString &udi)
{
    QRegularExpression rx("/sound/");				// any UDI mentioning sound
    return (udi.contains(rx));
}


static int matchDevice(const QString &udi)
{
    QRegExp rx("/sound/card(\\d+)$");			// match sound card and ID number
    if (!udi.contains(rx)) return (-1);			// UDI not recognised
    return (rx.cap(1).toInt());				// assume conversion succeeds
}


void KMixDeviceManager::pluggedSlot(const QString &udi)
{
    if (!isSoundDevice(udi)) return;			// ignore non-sound devices

    Solid::Device device(udi);
    if (!device.isValid())
    {
        qCWarning(KMIX_LOG) << "Invalid device for UDI" << udi;
        return;
    }

    // Solid device UDIs for a plugged sound card are of the form:
    //
    //   /org/kde/solid/udev/sys/devices/pci0000:00/0000:00:13.2/usb2/2-4/2-4.1/2-4.1:1.0/sound/card3
    //   /org/kde/solid/udev/sys/devices/pci0000:00/0000:00:13.2/usb2/2-4/2-4.1/2-4.1:1.0/sound/card3/pcmC3D0p
    //   /org/kde/solid/udev/sys/devices/pci0000:00/0000:00:13.2/usb2/2-4/2-4.1/2-4.1:1.0/sound/card3/...
    //
    // The first of these is considered to be the canonical form and triggers
    // device hotplug, after a settling delay.
    //
    // Note that the Solid device UDI is not used anwhere else within KMix,
    // what is referred to as a "UDI" elsewhere is as described in the
    // comment for Mixer::udi() in core/mixer.h and generated by getUDI_ALSA()
    // above.  Solid only ever supported ALSA anyway, even in KDE4, so we can
    // assume the driver is ALSA here without having to ask.  See
    // https://community.kde.org/Frameworks/Porting_Notes#Solid_Changes

    const int dev = matchDevice(udi);
    if (dev!=-1)
    {
        qCDebug(KMIX_LOG) << "udi" << udi;
        const QString alsaUDI = getUDI_ALSA(dev);
        qCDebug(KMIX_LOG) << "ALSA udi" << alsaUDI << "device" << dev;
        QTimer::singleShot(HOTPLUG_DELAY, [=](){ emit plugged("ALSA", alsaUDI, dev); });
    }							// allow hotplug to settle
    else qCDebug(KMIX_LOG) << "Ignored unrecognised UDI" << udi;
}


void KMixDeviceManager::unpluggedSlot(const QString &udi)
{
    if (!isSoundDevice(udi)) return;			// ignore non-sound devices

    // At this point the device has already been unplugged by the user.
    // Solid doesn't know anything about the device except the UDI, but
    // that can be matched using the same logic as for plugging.

    const int dev = matchDevice(udi);
    if (dev!=-1)
    {
        qCDebug(KMIX_LOG) << "udi" << udi;
        const QString alsaUDI = getUDI_ALSA(dev);
        qCDebug(KMIX_LOG) << "ALSA udi" << alsaUDI << "device" << dev;
        QTimer::singleShot(HOTPLUG_DELAY, [=](){ emit unplugged(alsaUDI); });
    }							// allow hotplug to settle
    else qCDebug(KMIX_LOG) << "Ignored unrecognised UDI" << udi;
}


void KMixDeviceManager::setHotpluggingBackends(const QString &backendName)
{
    qCDebug(KMIX_LOG) << "using" << backendName;
    // Note: this setting is ignored, it is assumed above that
    // Solid only delivers sound card hotplug events for ALSA.
    _hotpluggingBackend = backendName;
}
