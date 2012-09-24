/*
 * KMix -- KDE's full featured mini mixer
 *
 * Copyright (C) Trever Fischer <tdfischer@fedoraproject.org>
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

#include "PulseAudio.h"
#include "PulseControl.h"
#include <QtCore/QDebug>

namespace Backends {

PulseAudio::PulseAudio(QObject *parent)
    : Backend(parent)
{
    m_loop = pa_glib_mainloop_new(NULL);
    m_loopAPI = pa_glib_mainloop_get_api(m_loop);
    m_context = pa_context_new(m_loopAPI, "KMix");
}

PulseAudio::~PulseAudio()
{
    if (m_context) {
        pa_context_unref(m_context);
        m_context = NULL;
    }
    if (m_loop) {
        pa_glib_mainloop_free(m_loop);
        m_loop = NULL;
    }
}

void PulseAudio::sink_cb(pa_context *cxt, const pa_sink_info *info, int eol, gpointer user_data) {
    PulseAudio *that = static_cast<PulseAudio*>(user_data);
    qDebug() << eol;
    if (eol < 0) {
        qDebug() << pa_context_errno(cxt);
        if (pa_context_errno(cxt) == PA_ERR_NOENTITY)
            return;
    }
    if (eol > 0) {
        return;
    }
    PulseControl *control;
    qDebug() << "Event for" << info->index;
    if (!that->m_sinks.contains(info->index)) {
        control = new PulseControl(cxt, info, that);
        QObject::connect(control, SIGNAL(scheduleRefresh(int)), that, SLOT(refreshSink(int)));
        that->m_sinks[info->index] = control;
        that->registerControl(control);
    } else {
        control = that->m_sinks[info->index];
        control->update(info);
    }
}

void PulseAudio::refreshSink(int idx)
{
    qDebug() << "update queued for" << idx;
    pa_context_get_sink_info_by_index(m_context, idx, sink_cb, this);
}

void PulseAudio::subscribe_cb(pa_context *cxt, pa_subscription_event_type t, uint32_t index, gpointer user_data)
{
    qDebug() << "subscription" << t;
    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
        case PA_SUBSCRIPTION_EVENT_SINK:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
                //FIXME
            } else {
                pa_operation *op;
                if (!(op = pa_context_get_sink_info_by_index(cxt, index, sink_cb, user_data))) {
                    qWarning() << "pa_context_get_sink_info_by_index failed";
                    return;
                }
                pa_operation_unref(op);
            }
            break;
    }
}

void PulseAudio::context_state_callback(pa_context *cxt, gpointer user_data)
{
    PulseAudio *that = static_cast<PulseAudio*>(user_data);
    pa_context_state_t state = pa_context_get_state(cxt);
    if (state == PA_CONTEXT_READY) {
        pa_operation *op;
        pa_context_set_subscribe_callback(cxt, subscribe_cb, that);
        if (!(op = pa_context_subscribe(cxt, (pa_subscription_mask_t) (
                PA_SUBSCRIPTION_MASK_SINK |
                PA_SUBSCRIPTION_MASK_SOURCE | 
                PA_SUBSCRIPTION_MASK_CLIENT | 
                PA_SUBSCRIPTION_MASK_SINK_INPUT |
                PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT),
                NULL, NULL))) {
            qWarning() << "pa_context_subscribe failed";
            return;
        }
        pa_operation_unref(op);
        if (!(op = pa_context_get_sink_info_list(cxt, sink_cb, that))) {
            return;
        }
        pa_operation_unref(op);
    }
}

bool PulseAudio::open()
{
    if (pa_context_connect(m_context, NULL, PA_CONTEXT_NOFAIL, 0) < 0) {
        pa_context_unref(m_context);
        m_context = NULL;
        return false;
    }
    pa_context_set_state_callback(m_context, &context_state_callback, this);
    return true;
}

} //namespace Backends
