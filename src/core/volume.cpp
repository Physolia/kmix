/*
 * KMix -- KDE's full featured mini mixer
 *
 *
 * Copyright (C) 1996-2004 Christian Esken <esken@kde.org>
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

#include "core/volume.h"

// for operator<<()
#include <iostream>

#include <kdebug.h>


int Volume::_channelMaskEnum[9] =
{
    MLEFT, MRIGHT, MCENTER,
    MWOOFER,
    MSURROUNDLEFT, MSURROUNDRIGHT,
    MREARSIDELEFT, MREARSIDERIGHT,
    MREARCENTER
};

QString Volume::ChannelNameReadable[9] =
{
    "Left", "Right",
    "Center", "Subwoofer",
    "Surround Left", "Surround Right",
    "Side Left", "Side Right",
    "Rear Center"
};

char Volume::ChannelNameForPersistence[9][30] =
{
    "volumeL", "volumeR",
    "volumeCenter", "volumeWoofer",
    "volumeSurroundL", "volumeSurroundR",
    "volumeSideL", "volumeSideR",
    "volumeRearCenter"
};

// Forbidden/private. Only here because if there is no CaptureVolume we need the values initialized
// And also QMap requires it.
Volume::Volume()
{
    _minVolume = 0;
    _maxVolume = 0;
    _hasSwitch = false;
}

VolumeChannel::VolumeChannel()
{

}

Volume::Volume(long maxVolume, long minVolume, bool hasSwitch, bool isCapture)
{
    init((ChannelMask) 0, maxVolume, minVolume, hasSwitch, isCapture);
}

/**
 * @Deprecated
 */
void Volume::addVolumeChannels(ChannelMask chmask)
{
    for (Volume::ChannelID chid = Volume::CHIDMIN; chid <= Volume::CHIDMAX;) {
        if (chmask & Volume::_channelMaskEnum[chid]) {
            addVolumeChannel(VolumeChannel(chid));
        }
        chid = (Volume::ChannelID)( 1 + (int)chid); // ugly
    } // for all channels
}

void Volume::addVolumeChannel(VolumeChannel ch)
{
    _volumesL.insert(ch.chid, ch);
}

void Volume::init(ChannelMask chmask, long maxVolume, long minVolume, bool hasSwitch, bool isCapture)
{
    _chmask          = chmask;
    _maxVolume       = maxVolume;
    _minVolume       = minVolume;
    _hasSwitch       = hasSwitch;
    _isCapture       = isCapture;
    //_muted           = false;
    _switchActivated = false;
}

QMap<Volume::ChannelID, VolumeChannel> Volume::getVolumes() const
{
    return _volumesL;
}

// @ compatibility
void Volume::setAllVolumes(long vol)
{
    long int finalVol = volrange(vol);
    QMap<Volume::ChannelID, VolumeChannel>::iterator it = _volumesL.begin();
    while (it != _volumesL.end()) {
        it.value().volume = finalVol;
        ++it;
    }
}

void Volume::changeAllVolumes(long step)
{
    QMap<Volume::ChannelID, VolumeChannel>::iterator it = _volumesL.begin();
    while (it != _volumesL.end()) {
        it.value().volume = volrange(it.value().volume + step);
        ++it;
    }
}


// @ compatibility
void Volume::setVolume( ChannelID chid, long vol)
{
    QMap<Volume::ChannelID, VolumeChannel>::iterator it = _volumesL.find(chid);
    if (it != _volumesL.end()) {
        it.value().volume = vol;
    }
}

/**
 * Copy the volume elements contained in v to this Volume object.
 */
void Volume::setVolume(const Volume &v)
{
    foreach (VolumeChannel vc, _volumesL) {
        ChannelID chid = vc.chid;
        v.getVolumes()[chid].volume = vc.volume;
    }
}

long Volume::maxVolume()
{
    return _maxVolume;
}

long Volume::minVolume()
{
    return _minVolume;
}

long Volume::volumeSpan()
{
    return _maxVolume - _minVolume + 1;
}

long Volume::getVolume(ChannelID chid)
{
    return _volumesL.value(chid).volume;
}

qreal Volume::getAvgVolume(ChannelMask chmask)
{
    int avgVolumeCounter = 0;
    long long sumOfActiveVolumes = 0;
    foreach (VolumeChannel vc, _volumesL) {
        if (Volume::_channelMaskEnum[vc.chid] & chmask) {
            sumOfActiveVolumes += vc.volume;
            ++ avgVolumeCounter;
        }
    }
    if (avgVolumeCounter != 0) {
        qreal sumOfActiveVolumesQreal = sumOfActiveVolumes;
        sumOfActiveVolumesQreal /= avgVolumeCounter;
        return sumOfActiveVolumesQreal;
    }
    else
        return 0;
}


int Volume::getAvgVolumePercent(ChannelMask chmask)
{
    qreal volume = getAvgVolume(chmask);
    // min=-100, max=200 => volSpan = 301
    // volume = -50 =>  volShiftedToZero = -50+min = 50
    qreal volSpan = volumeSpan();
    qreal volShiftedToZero = volume - _minVolume;
    qreal percentReal = volSpan == 0 ? 0 : (100 * volShiftedToZero) / (volSpan - 1);
    int percent = qRound(percentReal);

    return percent;
}

int Volume::count()
{
    return getVolumes().count();
}

/**
 * returns a "sane" volume level. This means, it is a volume level inside the
 * valid bounds
 */
long Volume::volrange(long vol)
{
    if (vol < _minVolume) {
        return _minVolume;
    }
    else if (vol < _maxVolume) {
        return vol;
    }
    else {
        return _maxVolume;
    }
}


std::ostream& operator<<(std::ostream& os, const Volume& vol) {
    os << "(";

    bool first = true;
    foreach (const VolumeChannel vc, vol.getVolumes()) {
        if ( !first )  os << ",";
        else first = false;
        os << vc.volume;
    } // all channels
    os << ")";

    os << " [" << vol._minVolume << "-" << vol._maxVolume;
    if (vol._switchActivated) {
        os << " : switch active ]";
    } else {
        os << " : switch inactive ]";
    }

    return os;
}

QDebug operator<<(QDebug os, const Volume& vol) {
    os << "(";
    bool first = true;
    foreach (VolumeChannel vc, vol.getVolumes()) {
        if ( !first )  os << ",";
        else first = false;
        os << vc.volume;
    } // all channels
    os << ")";

    os << " [" << vol._minVolume << "-" << vol._maxVolume;
    if ( vol._switchActivated ) {
        os << " : switch active ]";
    } else {
        os << " : switch inactive ]";
    }

    return os;
}
