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
#include "kmix_debug.h"

// for operator<<()
#include <iostream>

#include <klocalizedstring.h>

float Volume::VOLUME_STEP_DIVISOR = 20;
float Volume::VOLUME_PAGESTEP_DIVISOR = 10;


static Volume::ChannelMask channelMask[] =
{
        Volume::MLEFT, Volume::MRIGHT, Volume::MCENTER,
        Volume::MWOOFER,
        Volume::MSURROUNDLEFT, Volume::MSURROUNDRIGHT,
        Volume::MREARSIDELEFT, Volume::MREARSIDERIGHT,
        Volume::MREARCENTER
};

QString Volume::channelNameReadable(ChannelID id)
{
    switch (id)
    {
case LEFT:              return (i18nc("Channel name", "Left"));
case RIGHT:             return (i18nc("Channel name", "Right"));
case CENTER:            return (i18nc("Channel name", "Center"));
case WOOFER:            return (i18nc("Channel name", "Subwoofer"));
case SURROUNDLEFT:      return (i18nc("Channel name", "Surround Left"));
case SURROUNDRIGHT:     return (i18nc("Channel name", "Surround Right"));
case REARSIDELEFT:      return (i18nc("Channel name", "Side Left"));
case REARSIDERIGHT:     return (i18nc("Channel name", "Side Right"));
case REARCENTER:        return (i18nc("Channel name", "Rear Center"));
default:                qCWarning(KMIX_LOG) << "called for unknown ID" << id;
                        return (i18nc("Channel name", "Unknown"));
    }
}


QString Volume::channelNameForPersistence(ChannelID id)
{
    switch (id)
    {
case LEFT:              return ("volumeL");
case RIGHT:             return ("volumeR");
case CENTER:            return ("volumeCenter");
case WOOFER:            return ("volumeWoofer");
case SURROUNDLEFT:      return ("volumeSurroundL");
case SURROUNDRIGHT:     return ("volumeSurroundR");
case REARSIDELEFT:      return ("volumeSideL");
case REARSIDERIGHT:     return ("volumeSideR");
case REARCENTER:        return ("volumeRearCenter");
default:                qCWarning(KMIX_LOG) << "called for unknown ID" << id;
                        return ("unknown");
    }
}


// Forbidden/private. Only here because if there is no CaptureVolume we need the values initialized
// And also QMap requires it.
Volume::Volume()
: _chmask(MNONE)
, _minVolume(0)
, _maxVolume(0)
, _hasSwitch(false)
, _switchActivated(false)
, _switchType(None)
, _isCapture(false)
{
}

/**
 * Do not use. Only implicitely required for QMap.
 *
 * @deprecated Do not use
 */
VolumeChannel::VolumeChannel()
: volume(0)
, chid(Volume::NOCHANNEL)
{
}

VolumeChannel::VolumeChannel(Volume::ChannelID c)
: volume(0)
, chid(c)
{
}

Volume::Volume(long maxVolume, long minVolume, bool hasSwitch, bool isCapture )
{
    init(static_cast<ChannelMask>(0), maxVolume, minVolume, hasSwitch, isCapture );
}

/**
 * @deprecated
 */
void Volume::addVolumeChannels(ChannelMask chmask)
{
	for ( Volume::ChannelID chid=Volume::CHIDMIN; chid<= Volume::CHIDMAX;  )
	{
		if ( chmask & channelMask[chid] )
		{
			addVolumeChannel(VolumeChannel(chid));
		}
		chid = static_cast<Volume::ChannelID>(1+static_cast<int>(chid));
	} // for all channels
}

void Volume::addVolumeChannel(VolumeChannel vc)
{
	_volumesL.insert(vc.chid, vc);
	// Add the corresponding "muted version" of the channel.
//	VolumeChannel* zeroChannel = new VolumeChannel(vc.chid);
//	zeroChannel->volume = 0;
//	_volumesMuted.insert(zeroChannel->chid, *zeroChannel); // TODO remove _volumesMuted
}



void Volume::init( ChannelMask chmask, long maxVolume, long minVolume, bool hasSwitch, bool isCapture)
{
	_chmask          = chmask;
	_maxVolume       = maxVolume;
	_minVolume       = minVolume;
	_hasSwitch       = hasSwitch;
	_isCapture       = isCapture;
	//_muted           = false;
	// Presume that the switch is active. This will always work:
	// a) Physical switches will be updated after start from the hardware.
	// b) Emulated virtual/switches will not receive updates from the hardware, so they shouldn't disable the channels.
	_switchActivated = true;
}

QMap<Volume::ChannelID, VolumeChannel> Volume::getVolumesWhenActive() const
{
	return _volumesL;
}

QMap<Volume::ChannelID, VolumeChannel> Volume::getVolumes() const
{
	return _volumesL;
}

/**
 * Returns the absolute change to do one "step" for this volume. This is similar to a page step in a slider,
 * namely a fixed percentage of the range.
 * One step is the percentage given by 100/VOLUME_STEP_DIVISOR. The
 * default VOLUME_STEP_DIVISOR is 20, so default change is 5% of the volumeSpan().
 *
 * This method guarantees a minimum absolute change of 1, zero is never returned.
 *
 * It is NOT verified, that such a volume change would actually be possible. You might hit the upper or lower bounds
 * of the volume range.
 *
 *
 * @param decrease true, if you want a volume step that decreases the volume by one page step
 * @return The volume step. It will be negative if you have used decrease==true
 *
 */
long Volume::volumeStep(bool decrease) const
{
  	long inc = volumeSpan() / Volume::VOLUME_STEP_DIVISOR;
	if ( inc == 0 )	inc = 1;
	if ( decrease ) inc *= -1;
	return inc;
}


// @ compatibility
void Volume::setAllVolumes(long vol)
{
	long int finalVol = volrange(vol);
	QMap<Volume::ChannelID, VolumeChannel>::iterator it = _volumesL.begin();
	while (it != _volumesL.end())
	{
		it.value().volume = finalVol;
		//it.value().unmutedVolume= finalVol;
		++it;
	}
}

void Volume::changeAllVolumes( long step )
{
	QMap<Volume::ChannelID, VolumeChannel>::iterator it = _volumesL.begin();
	while (it != _volumesL.end())
	{
		long int finalVol = volrange(it.value().volume + step);
		it.value().volume = finalVol;
// 		it.value().unmutedVolume= finalVol;
		++it;
	}
}


/**
 * Sets the volume for the given Channel
 * @ compatibility
 */
void Volume::setVolume( ChannelID chid, long vol)
{
	QMap<Volume::ChannelID, VolumeChannel>::iterator it = _volumesL.find(chid);
	if ( it != _volumesL.end())
	{
		it.value().volume = vol;
		//it.value().unmutedVolume = vol;
	}
}

/**
 * Copy the volume elements contained in v to this Volume object.
 */
// void Volume::setVolume(const Volume &v)
// {
// 	foreach (VolumeChannel vc, _volumesL )
// 	{
// 		ChannelID chid = vc.chid;
// 		v.getVolumes()[chid].volume = vc.volume;
// 		//v.getVolumes()[chid].unmutedVolume = vc.volume;
// 	}
// }

   void Volume::setSwitch( bool active )
   {
     _switchActivated = active;

//      if ( isCapture() )
//        return;
//      
     // for playback volumes we will not only do the switch, but also set the volume to 0
// 	QMap<Volume::ChannelID, VolumeChannel>::iterator it = _volumesL.begin();
//      if ( active )
//      {
// 	while (it != _volumesL.end())
// 	{
// 	  VolumeChannel& vc = it.value();
// 	  vc.volume = vc.unmutedVolume;
// 	  ++it;
// 	}	
//      }
//      else
//      {
// 	while (it != _volumesL.end())
// 	{
// 	  VolumeChannel& vc = it.value();
// 	  vc.unmutedVolume = vc.volume;
// 	  vc.volume = 0;
// 	  ++it;
//        }       
//      }
  }


/**
 * Returns the volume of the given channel.
 */
long Volume::getVolume(ChannelID chid) const
{
	return _volumesL.value(chid).volume;
}

/**
 * Returns the volume of the given channel. If this Volume is inactive (switched off), 0 is returned.
 */
long Volume::getVolumeForGUI(ChannelID chid) const
{
	if (! isSwitchActivated() )
		return 0;

	return _volumesL.value(chid).volume;
}

qreal Volume::getAvgVolume(ChannelMask chmask) const
{
	int avgVolumeCounter = 0;
	long long sumOfActiveVolumes = 0;
	foreach (VolumeChannel vc, _volumesL )
	{
		if (channelMask[vc.chid] & chmask )
		{
			sumOfActiveVolumes += vc.volume;
			++avgVolumeCounter;
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


int Volume::getAvgVolumePercent(ChannelMask chmask) const
{
	qreal volume = getAvgVolume(chmask);
	// min=-100, max=200 => volSpan = 301
	// volume = -50 =>  volShiftedToZero = -50+min = 50
	qreal volSpan = volumeSpan();
	qreal volShiftedToZero = volume - _minVolume;
	qreal percentReal = ( volSpan == 0 ) ? 0 : ( 100 * volShiftedToZero ) / ( volSpan - 1);
	int percent = qRound(percentReal);
	//qCDebug(KMIX_LOG) << "volSpan=" << volSpan << ", volume=" << volume << ", volShiftedToPositive=" << volShiftedToZero << ", percent=" << percent;

	return percent;
}

/**
 * returns a "sane" volume level. This means, it is a volume level inside the
 * valid bounds
 */
long Volume::volrange( long vol )
{
	if ( vol < _minVolume ) {
		return _minVolume;
	}
	else if ( vol < _maxVolume ) {
		return vol;
	}
	else {
		return _maxVolume;
	}
}


std::ostream& operator<<(std::ostream& os, const Volume& vol) {
	os << "(";

	bool first = true;
	foreach ( const VolumeChannel vc, vol.getVolumes() )
	{
		if ( !first )  os << ",";
		else first = false;
		os << vc.volume;
	} // all channels
	os << ")";

	os << " [" << vol._minVolume << "-" << vol._maxVolume;
	if ( vol._switchActivated ) { os << " : switch active ]"; } else { os << " : switch inactive ]"; }

	return os;
}

QDebug operator<<(QDebug os, const Volume& vol) {
	os << "(";
	bool first = true;
	foreach ( VolumeChannel vc, vol.getVolumes() )
	{
		if ( !first )  os << ",";
		else first = false;
		os << vc.volume;
	} // all channels
	os << ")";

	os << " [" << vol._minVolume << "-" << vol._maxVolume;
	if ( vol._switchActivated ) { os << " : switch active ]"; } else { os << " : switch inactive ]"; }

	return os;
}
