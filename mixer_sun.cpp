/*
 *              KMix -- KDE's full featured mini mixer
 *
 *
 *              Copyright (C) 1996-2000 Christian Esken
 *                        esken@kde.org
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
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/audioio.h>


Mixer* Mixer::getMixer(int devnum, int SetNum)
{
  Mixer *l_mixer;
  l_mixer = new Mixer_SUN( devnum, SetNum);
  l_mixer->init(devnum, SetNum);
  return l_mixer;
}


Mixer_SUN::Mixer_SUN() : Mixer() { };
Mixer_SUN::Mixer_SUN(int devnum, int SetNum) : Mixer(devnum, SetNum);
void Mixer_SUN::setDevNumName_I(int devnum)
{
  devname = "/dev/audioctl";
}

QString Mixer_SUN::errorText(int mixer_error)
{
  QString l_s_errmsg;
  switch (mixer_error)
    {
    case ERR_PERM:
      l_s_errmsg = i18n(  "kmix: You have no permission to access the mixer device.\n" \
			  "Ask your system administrator to fix /dev/sndctl to allow the access.");
      break;
    default:
      l_s_errmsg = Mixer::errorText(mixer_error);
    }
  return l_s_errmsg;
}


void Mixer_SUN::readVolumeFromHW( int /*devnum*/, int *VolLeft, int *VolRight )
{
  audio_info_t audioinfo;
  int Volume;

  if (ioctl(fd, AUDIO_GETINFO, &audioinfo) < 0)
    errormsg(Mixer::ERR_READ);
  Volume = audioinfo.play.gain;
  *VolLeft  = *VolRight = (Volume & 0x7f);
}
