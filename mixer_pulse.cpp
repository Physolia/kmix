/*
 *              KMix -- KDE's full featured mini mixer
 *
 *
 *              Copyright (C) 2008 Helio Chissini de Castro <helio@kde.org>
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

#include <cstdlib>
#include <QEventLoop>

#include "mixer_pulse.h"
#include "mixer.h"

#include <pulse/glib-mainloop.h>
#include <pulse/ext-stream-restore.h>


#define KMIXPA_PLAYBACK     0
#define KMIXPA_CAPTURE      1
#define KMIXPA_APP_PLAYBACK 2
#define KMIXPA_APP_CAPTURE  3

static unsigned int refcount = 0;
static pa_context *context = NULL;
static pa_glib_mainloop *mainloop = NULL;
static QEventLoop *s_connectionEventloop = NULL;
static enum { UNKNOWN, ACTIVE, INACTIVE } s_pulseActive = UNKNOWN;
static int s_OutstandingRequests = 0;

QMap<int,Mixer_PULSE*> s_Mixers;

typedef QMap<int,devinfo> devmap;
static devmap outputDevices;
static devmap captureDevices;
static QMap<int,QString> clients;
static devmap outputStreams;
static devmap captureStreams;


static void dec_outstanding() {
    if (s_OutstandingRequests <= 0)
        return;

    if (--s_OutstandingRequests <= 0)
    {
        s_pulseActive = ACTIVE;
        if (s_connectionEventloop) {
            s_connectionEventloop->exit(0);
            s_connectionEventloop = NULL;

            // If we have no devices then we consider PA to be 'INACTIVE'
            if (outputDevices.isEmpty() && captureDevices.isEmpty())
                s_pulseActive = INACTIVE;
            else
                s_pulseActive = ACTIVE;
        }
    }
}

static void translateMasksAndMaps(devinfo& dev)
{
    dev.chanMask = Volume::MNONE;
    dev.chanIDs.clear();

    if (dev.channel_map.channels != dev.volume.channels) {
        kError() << "Hiddeous Channel mixup map says " << dev.channel_map.channels << ", volume says: " << dev.volume.channels;
        return;
    }
    if (1 == dev.channel_map.channels && PA_CHANNEL_POSITION_MONO == dev.channel_map.map[0]) {
        // We just use the left channel to represent this.
        dev.chanMask = (Volume::ChannelMask)( dev.chanMask | Volume::MLEFT);
        dev.chanIDs[0] = Volume::LEFT;
    } else {
        for (uint8_t i = 0; i < dev.channel_map.channels; ++i) {
            switch (dev.channel_map.map[i]) {
                case PA_CHANNEL_POSITION_MONO:
                    kWarning(67100) << "Channel Map contains a MONO element but has >1 channel - we can't handle this.";
                    return;

                case PA_CHANNEL_POSITION_FRONT_LEFT:
                    dev.chanMask = (Volume::ChannelMask)( dev.chanMask | Volume::MLEFT);
                    dev.chanIDs[i] = Volume::LEFT;
                    break;
                case PA_CHANNEL_POSITION_FRONT_RIGHT:
                    dev.chanMask = (Volume::ChannelMask)( dev.chanMask | Volume::MRIGHT);
                    dev.chanIDs[i] = Volume::RIGHT;
                    break;
                case PA_CHANNEL_POSITION_FRONT_CENTER:
                    dev.chanMask = (Volume::ChannelMask)( dev.chanMask | Volume::MCENTER);
                    dev.chanIDs[i] = Volume::CENTER;
                    break;
                case PA_CHANNEL_POSITION_REAR_CENTER:
                    dev.chanMask = (Volume::ChannelMask)( dev.chanMask | Volume::MREARCENTER);
                    dev.chanIDs[i] = Volume::REARCENTER;
                    break;
                case PA_CHANNEL_POSITION_REAR_LEFT:
                    dev.chanMask = (Volume::ChannelMask)( dev.chanMask | Volume::MSURROUNDLEFT);
                    dev.chanIDs[i] = Volume::SURROUNDLEFT;
                    break;
                case PA_CHANNEL_POSITION_REAR_RIGHT:
                    dev.chanMask = (Volume::ChannelMask)( dev.chanMask | Volume::MSURROUNDRIGHT);
                    dev.chanIDs[i] = Volume::SURROUNDRIGHT;
                    break;
                case PA_CHANNEL_POSITION_LFE:
                    dev.chanMask = (Volume::ChannelMask)( dev.chanMask | Volume::MWOOFER);
                    dev.chanIDs[i] = Volume::WOOFER;
                    break;
                case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
                    dev.chanMask = (Volume::ChannelMask)( dev.chanMask | Volume::MREARSIDELEFT);
                    dev.chanIDs[i] = Volume::REARSIDELEFT;
                    break;
                case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
                    dev.chanMask = (Volume::ChannelMask)( dev.chanMask | Volume::MREARSIDERIGHT);
                    dev.chanIDs[i] = Volume::REARSIDERIGHT;
                    break;
                default:
                    kWarning(67100) << "Channel Map contains a pa_channel_position we cannot handle " << dev.channel_map.map[i];
                    break;
            }
        }
    }
}

static void sink_cb(pa_context *c, const pa_sink_info *i, int eol, void *) {

    Q_ASSERT(c == context);

    if (eol < 0) {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
            return;

        kWarning(67100) << "Sink callback failure";
        return;
    }

    if (eol > 0) {
        dec_outstanding();
        if (s_Mixers.contains(KMIXPA_PLAYBACK))
            s_Mixers[KMIXPA_PLAYBACK]->triggerUpdate();
        return;
    }

    devinfo s;
    s.index = s.device_index = i->index;
    s.name = QString(i->name).replace(' ', '_');
    s.description = i->description;
    s.volume = i->volume;
    s.channel_map = i->channel_map;
    s.mute = !!i->mute;

    translateMasksAndMaps(s);

    bool is_new = !outputDevices.contains(s.index);
    outputDevices[s.index] = s;
    kDebug(67100) << "Got some info about sink: " << s.description;

    if (is_new && s_Mixers.contains(KMIXPA_PLAYBACK))
        s_Mixers[KMIXPA_PLAYBACK]->newOutputDevice(s.index);
}

static void source_cb(pa_context *c, const pa_source_info *i, int eol, void *) {

    Q_ASSERT(c == context);

    if (eol < 0) {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
            return;

        kWarning(67100) << "Source callback failure";
        return;
    }

    if (eol > 0) {
        dec_outstanding();
        if (s_Mixers.contains(KMIXPA_CAPTURE))
            s_Mixers[KMIXPA_CAPTURE]->triggerUpdate();
        return;
    }

    // Do something....
    if (PA_INVALID_INDEX != i->monitor_of_sink)
    {
        kDebug(67100) << "Ignoring Monitor Source: " << i->description;
        return;
    }

    devinfo s;
    s.index = s.device_index = i->index;
    s.name = QString(i->name).replace(' ', '_');
    s.description = i->description;
    s.volume = i->volume;
    s.channel_map = i->channel_map;
    s.mute = !!i->mute;

    translateMasksAndMaps(s);

    bool is_new = !captureDevices.contains(s.index);
    captureDevices[s.index] = s;
    kDebug(67100) << "Got some info about source: " << s.description;

    if (is_new && s_Mixers.contains(KMIXPA_CAPTURE))
        s_Mixers[KMIXPA_CAPTURE]->newCaptureDevice(s.index);
}

static void client_cb(pa_context *c, const pa_client_info *i, int eol, void *) {

    Q_ASSERT(c == context);

    if (eol < 0) {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
            return;

        kWarning(67100) << "Client callback failure";
        return;
    }

    if (eol > 0) {
        dec_outstanding();
        return;
    }

    clients[i->index] = i->name;
    kDebug(67100) << "Got some info about client: " << i->name;
}

static void sink_input_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *) {

    Q_ASSERT(c == context);

    if (eol < 0) {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
            return;

        kWarning(67100) << "Sink Input callback failure";
        return;
    }

    if (eol > 0) {
        dec_outstanding();
        if (s_Mixers.contains(KMIXPA_APP_PLAYBACK))
            s_Mixers[KMIXPA_APP_PLAYBACK]->triggerUpdate();
        return;
    }

    const char *t;
    if ((t = pa_proplist_gets(i->proplist, "module-stream-restore.id"))) {
        if (strcmp(t, "sink-input-by-media-role:event") == 0) {
            kWarning(67100) << "Ignoring sink-input due to it being designated as an event and thus handled by the Event slider";
            return;
        }
    }

    QString prefix = "Unknown Application: ";
    if (clients.contains(i->client))
        prefix = QString("%1: ").arg(clients[i->client]);

    devinfo s;
    s.index = i->index;
    s.device_index = i->sink;
    s.description = prefix + i->name;
    s.name = QString(s.description).replace(' ', '_');
    s.volume = i->volume;
    s.channel_map = i->channel_map;
    s.mute = !!i->mute;

    translateMasksAndMaps(s);

    bool is_new = !outputStreams.contains(s.index);
    outputStreams[s.index] = s;
    kDebug(67100) << "Got some info about sink input (playback stream): " << s.description;

    if (is_new && s_Mixers.contains(KMIXPA_APP_PLAYBACK))
        s_Mixers[KMIXPA_APP_PLAYBACK]->newOutputStream(s.index);
}

static void source_output_cb(pa_context *c, const pa_source_output_info *i, int eol, void *) {

    Q_ASSERT(c == context);

    if (eol < 0) {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
            return;

        kWarning(67100) << "Source Output callback failure";
        return;
    }

    if (eol > 0) {
        dec_outstanding();
        if (s_Mixers.contains(KMIXPA_APP_CAPTURE))
            s_Mixers[KMIXPA_APP_CAPTURE]->triggerUpdate();
        return;
    }

    /* NB Until Source Outputs support volumes, we just use the volume of the source itself */
    if (!captureDevices.contains(i->source)) {
        kWarning(67100) << "Source Output refers to a Source we don't have any info for :s";
        return;
    }

    QString prefix = "Unknown Application: ";
    if (clients.contains(i->client))
        prefix = QString("%1: ").arg(clients[i->client]);

    devinfo s;
    s.index = i->index;
    s.device_index = i->source;
    s.description = prefix + i->name;
    s.name = QString(s.description).replace(' ', '_');
    //s.volume = i->volume;
    s.volume = captureDevices[i->source].volume;
    s.channel_map = i->channel_map;
    //s.mute = !!i->mute;
    s.mute = captureDevices[i->source].mute;

    translateMasksAndMaps(s);

    bool is_new = !captureStreams.contains(s.index);
    captureStreams[s.index] = s;
    kDebug(67100) << "Got some info about source output (capture stream): " << s.description;

    if (is_new && s_Mixers.contains(KMIXPA_APP_CAPTURE))
        s_Mixers[KMIXPA_APP_CAPTURE]->newOutputStream(s.index);
}

static void subscribe_cb(pa_context *c, pa_subscription_event_type_t t, uint32_t index, void *) {

    Q_ASSERT(c == context);

    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
        case PA_SUBSCRIPTION_EVENT_SINK:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
                // Todo: Remove our cache of this sink (of 'index')
                outputDevices.remove(index);
            } else {
                pa_operation *o;
                if (!(o = pa_context_get_sink_info_by_index(c, index, sink_cb, NULL))) {
                    kWarning(67100) << "pa_context_get_sink_info_by_index() failed";
                    return;
                }
                pa_operation_unref(o);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
                // Todo: Remove our cache of this source (of 'index')
                captureDevices.remove(index);
            } else {
                pa_operation *o;
                if (!(o = pa_context_get_source_info_by_index(c, index, source_cb, NULL))) {
                    kWarning(67100) << "pa_context_get_source_info_by_index() failed";
                    return;
                }
                pa_operation_unref(o);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                outputStreams.remove(index);
            else {
                pa_operation *o;
                if (!(o = pa_context_get_sink_input_info(c, index, sink_input_cb, NULL))) {
                    kWarning(67100) << "pa_context_get_sink_input_info() failed";
                    return;
                }
                pa_operation_unref(o);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                captureStreams.remove(index);
            else {
                pa_operation *o;
                if (!(o = pa_context_get_source_output_info(c, index, source_output_cb, NULL))) {
                    kWarning(67100) << "pa_context_get_sink_input_info() failed";
                    return;
                }
                pa_operation_unref(o);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_CLIENT:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
                clients.remove(index);
            } else {
                pa_operation *o;
                if (!(o = pa_context_get_client_info(c, index, client_cb, NULL))) {
                    kWarning(67100) << "pa_context_get_client_info() failed";
                    return;
                }
                pa_operation_unref(o);
            }
            break;

    }
}


static void context_state_callback(pa_context *c, void *)
{
    Q_ASSERT(c == context);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            // Attempt to load things up
            pa_operation *o;

            pa_context_set_subscribe_callback(c, subscribe_cb, NULL);

            if (!(o = pa_context_subscribe(c, (pa_subscription_mask_t)
                                           (PA_SUBSCRIPTION_MASK_SINK|
                                            PA_SUBSCRIPTION_MASK_SOURCE|
                                            PA_SUBSCRIPTION_MASK_CLIENT|
                                            PA_SUBSCRIPTION_MASK_SINK_INPUT|
                                            PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT), NULL, NULL))) {
                kWarning(67100) << "pa_context_subscribe() failed";
                return;
            }
            pa_operation_unref(o);

            if (!(o = pa_context_get_sink_info_list(c, sink_cb, NULL))) {
                kWarning(67100) << "pa_context_get_sink_info_list() failed";
                return;
            }
            pa_operation_unref(o);
            s_OutstandingRequests++;

            if (!(o = pa_context_get_source_info_list(c, source_cb, NULL))) {
                kWarning(67100) << "pa_context_get_source_info_list() failed";
                return;
            }
            pa_operation_unref(o);
            s_OutstandingRequests++;


            if (!(o = pa_context_get_client_info_list(c, client_cb, NULL))) {
                kWarning(67100) << "pa_context_client_info_list() failed";
                return;
            }
            pa_operation_unref(o);
            s_OutstandingRequests++;

            if (!(o = pa_context_get_sink_input_info_list(c, sink_input_cb, NULL))) {
                kWarning(67100) << "pa_context_get_sink_input_info_list() failed";
                return;
            }
            pa_operation_unref(o);
            s_OutstandingRequests++;

            if (!(o = pa_context_get_source_output_info_list(c, source_output_cb, NULL))) {
                kWarning(67100) << "pa_context_get_source_output_info_list() failed";
                return;
            }
            pa_operation_unref(o);
            s_OutstandingRequests++;

            /* These calls are not always supported */
            /*if ((o = pa_ext_stream_restore_read(c, ext_stream_restore_read_cb, w))) {
                pa_operation_unref(o);
                s_OutstandingRequests++;

                pa_ext_stream_restore_set_subscribe_cb(c, ext_stream_restore_subscribe_cb, w);

                if ((o = pa_ext_stream_restore_subscribe(c, 1, NULL, NULL)))
                    pa_operation_unref(o);
            } else
                kWarning(67100) << "Failed to initialize stream_restore extension: " << pa_strerror(pa_context_errno(context));*/

            break;

        case PA_CONTEXT_FAILED:
            s_pulseActive = INACTIVE;
            if (s_connectionEventloop) {
                s_connectionEventloop->exit(0);
                s_connectionEventloop = NULL;
            }
            break;

        case PA_CONTEXT_TERMINATED:
        default:
            s_pulseActive = INACTIVE;
            /// @todo Deal with reconnection...
            break;
    }
}

static void setVolumeFromPulse(Volume& volume, const devinfo& dev)
{
    chanIDMap::const_iterator iter;
    for (iter = dev.chanIDs.begin(); iter != dev.chanIDs.end(); ++iter)
    {
        //kDebug(67100) <<  "Setting volume for channel " << i << " to " << cvolume.values[i] << " (" << ((100*cvolume.values[i]) / PA_VOLUME_NORM) << "%)";
        volume.setVolume(iter.value(), (long)dev.volume.values[iter.key()]);
    }
}

static pa_cvolume genVolumeForPulse(const devinfo& dev, Volume& volume)
{
    pa_cvolume cvol = dev.volume;

    chanIDMap::const_iterator iter;
    for (iter = dev.chanIDs.begin(); iter != dev.chanIDs.end(); ++iter)
    {
        cvol.values[iter.key()] = (uint32_t)volume.getVolume(iter.value());
        //kDebug(67100) <<  "Setting volume for channel " << i << " to " << cvolume.values[i] << " (" << ((100*cvolume.values[i]) / PA_VOLUME_NORM) << "%)";
    }
    return cvol;
}

void Mixer_PULSE::newOutputDevice(int index)
{
    Q_UNUSED(index);
}

void Mixer_PULSE::newCaptureDevice(int index)
{
    Q_UNUSED(index);
}

void Mixer_PULSE::newOutputStream(int index)
{
    Q_UNUSED(index);
}

void Mixer_PULSE::newCaptureStream(int index)
{
    Q_UNUSED(index);
}

void Mixer_PULSE::addDevice(devinfo& dev, bool capture)
{
    if (dev.chanMask != Volume::MNONE) {
        Volume v(dev.chanMask, PA_VOLUME_NORM, PA_VOLUME_MUTED, false, capture);
        setVolumeFromPulse(v, dev);
        MixDevice* md = new MixDevice( _mixer, dev.name, dev.description);
        md->addPlaybackVolume(v);
        md->setMuted(dev.mute);
        m_mixDevices.append(md);
    }
}

Mixer_Backend* PULSE_getMixer( Mixer *mixer, int devnum )
{
   Mixer_Backend *l_mixer;
   l_mixer = new Mixer_PULSE( mixer, devnum );
   return l_mixer;
}


Mixer_PULSE::Mixer_PULSE(Mixer *mixer, int devnum) : Mixer_Backend(mixer, devnum)
{
   if ( devnum == -1 )
      m_devnum = 0;

   ++refcount;
   if (INACTIVE != s_pulseActive && 1 == refcount)
   {
       mainloop = pa_glib_mainloop_new(g_main_context_default());
       g_assert(mainloop);

       pa_mainloop_api *api = pa_glib_mainloop_get_api(mainloop);
       g_assert(api);

       context = pa_context_new(api, "KMix KDE 4");
       g_assert(context);

       // We create a simple event loop to allow the glib loop
       // to iterate until we've connected or not to the server.
       s_connectionEventloop = new QEventLoop;

       // (cg) Convert to PA_CONTEXT_NOFLAGS when PulseAudio 0.9.19 is required
       if (pa_context_connect(context, NULL, static_cast<pa_context_flags_t>(0), 0) >= 0) {
           pa_context_set_state_callback(context, &context_state_callback, s_connectionEventloop);
           // Now we block until we connect or otherwise...
           s_connectionEventloop->exec();
       }
   }

   s_Mixers[m_devnum] = this;
}

Mixer_PULSE::~Mixer_PULSE()
{
    s_Mixers.remove(m_devnum);

    if (refcount > 0)
    {
        --refcount;
        if (0 == refcount)
        {
            pa_context_unref(context);
            pa_glib_mainloop_free(mainloop);
        }
    }
}

int Mixer_PULSE::open()
{
    //kDebug(67100) <<  "Trying Pulse sink";

    if (ACTIVE == s_pulseActive && m_devnum <= KMIXPA_APP_CAPTURE)
    {
        devmap::iterator iter;
        if (KMIXPA_PLAYBACK == m_devnum)
        {
            m_mixerName = "Playback Devices";
            for (iter = outputDevices.begin(); iter != outputDevices.end(); ++iter)
                addDevice(*iter, false);
        }
        else if (KMIXPA_CAPTURE == m_devnum)
        {
            m_mixerName = "Capture Devices";
            for (iter = captureDevices.begin(); iter != captureDevices.end(); ++iter)
                addDevice(*iter, true);
        }
        else if (KMIXPA_APP_PLAYBACK == m_devnum)
        {
            m_mixerName = "Playback Streams";
            /// @todo Add support for "events sounds" fixed slider
            for (iter = outputStreams.begin(); iter != outputStreams.end(); ++iter)
                addDevice(*iter, true);
        }
        else if (KMIXPA_APP_CAPTURE == m_devnum)
        {
            m_mixerName = "Capture Streams";
            for (iter = captureStreams.begin(); iter != captureStreams.end(); ++iter)
                addDevice(*iter, true);
        }

        kDebug(67100) <<  "Using PulseAudio for mixer: " << m_mixerName;
        m_isOpen = true;
    }
 
    return 0;
}

int Mixer_PULSE::close()
{
    return 1;
}

int Mixer_PULSE::readVolumeFromHW( const QString& id, MixDevice *md )
{
    devmap *map = NULL;
    Volume *vol = NULL;

    if (KMIXPA_PLAYBACK == m_devnum) {
        map = &outputDevices;
        vol = &md->playbackVolume();
    } else if (KMIXPA_CAPTURE == m_devnum) {
        map = &captureDevices;
        vol = &md->captureVolume();
    } else if (KMIXPA_APP_PLAYBACK == m_devnum) {
        map = &outputStreams;
        vol = &md->playbackVolume();
    } else if (KMIXPA_APP_CAPTURE == m_devnum) {
        map = &captureStreams;
        vol = &md->captureVolume();
    }

    Q_ASSERT(map);
    Q_ASSERT(vol);

    devmap::iterator iter;
    for (iter = map->begin(); iter != map->end(); ++iter)
    {
        if (iter->name == id)
        {
            setVolumeFromPulse(*vol, *iter);
            md->setMuted(iter->mute);
            break;
        }
    }

    return 0;
}

int Mixer_PULSE::writeVolumeToHW( const QString& id, MixDevice *md )
{
    devmap::iterator iter;
    if (KMIXPA_PLAYBACK == m_devnum)
    {
        for (iter = outputDevices.begin(); iter != outputDevices.end(); ++iter)
        {
            if (iter->name == id)
            {
                pa_operation *o;

                pa_cvolume volume = genVolumeForPulse(*iter, md->playbackVolume());
                if (!(o = pa_context_set_sink_volume_by_index(context, iter->index, &volume, NULL, NULL))) {
                    kWarning(67100) <<  "pa_context_set_sink_volume_by_index() failed";
                    return Mixer::ERR_READ;
                }
                pa_operation_unref(o);

                if (!(o = pa_context_set_sink_mute_by_index(context, iter->index, (md->isMuted() ? 1 : 0), NULL, NULL))) {
                    kWarning(67100) <<  "pa_context_set_sink_mute_by_index() failed";
                    return Mixer::ERR_READ;
                }
                pa_operation_unref(o);

                return 0;
            }
        }
    }
    else if (KMIXPA_CAPTURE == m_devnum)
    {
        for (iter = captureDevices.begin(); iter != captureDevices.end(); ++iter)
        {
            if (iter->name == id)
            {
                pa_operation *o;

                pa_cvolume volume = genVolumeForPulse(*iter, md->captureVolume());
                if (!(o = pa_context_set_source_volume_by_index(context, iter->index, &volume, NULL, NULL))) {
                    kWarning(67100) <<  "pa_context_set_source_volume_by_index() failed";
                    return Mixer::ERR_READ;
                }
                pa_operation_unref(o);

                if (!(o = pa_context_set_source_mute_by_index(context, iter->index, (md->isMuted() ? 1 : 0), NULL, NULL))) {
                    kWarning(67100) <<  "pa_context_set_source_mute_by_index() failed";
                    return Mixer::ERR_READ;
                }
                pa_operation_unref(o);

                return 0;
            }
        }
    }
    else if (KMIXPA_APP_PLAYBACK == m_devnum)
    {
        for (iter = outputStreams.begin(); iter != outputStreams.end(); ++iter)
        {
            if (iter->name == id)
            {
                pa_operation *o;

                pa_cvolume volume = genVolumeForPulse(*iter, md->playbackVolume());
                if (!(o = pa_context_set_sink_input_volume(context, iter->index, &volume, NULL, NULL))) {
                    kWarning(67100) <<  "pa_context_set_sink_input_volume() failed";
                    return Mixer::ERR_READ;
                }
                pa_operation_unref(o);

                if (!(o = pa_context_set_sink_input_mute(context, iter->index, (md->isMuted() ? 1 : 0), NULL, NULL))) {
                    kWarning(67100) <<  "pa_context_set_sink_input_mute() failed";
                    return Mixer::ERR_READ;
                }
                pa_operation_unref(o);

                return 0;
            }
        }
    }
    else if (KMIXPA_APP_CAPTURE == m_devnum)
    {
        for (iter = captureStreams.begin(); iter != captureStreams.end(); ++iter)
        {
            if (iter->name == id)
            {
                pa_operation *o;

                // NB Note that this is different from APP_PLAYBACK in that we set the volume on the source itself.
                pa_cvolume volume = genVolumeForPulse(*iter, md->captureVolume());
                if (!(o = pa_context_set_source_volume_by_index(context, iter->device_index, &volume, NULL, NULL))) {
                    kWarning(67100) <<  "pa_context_set_source_volume_by_index() failed";
                    return Mixer::ERR_READ;
                }
                pa_operation_unref(o);

                if (!(o = pa_context_set_source_mute_by_index(context, iter->device_index, (md->isMuted() ? 1 : 0), NULL, NULL))) {
                    kWarning(67100) <<  "pa_context_set_source_mute_by_index() failed";
                    return Mixer::ERR_READ;
                }
                pa_operation_unref(o);

                return 0;
            }
        }
    }

    return 0;
}

void Mixer_PULSE::triggerUpdate()
{
    readSetFromHWforceUpdate();
    readSetFromHW();
}

void Mixer_PULSE::setRecsrcHW( const QString& /*id*/, bool /* on */ )
{
   return;
}

bool Mixer_PULSE::isRecsrcHW( const QString& /*id*/ )
{
   return false;
}


QString PULSE_getDriverName() {
        return "PulseAudio";
}

QString Mixer_PULSE::getDriverName()
{
        return "PulseAudio";
}

