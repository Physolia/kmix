/*
 * KMix -- KDE's full featured mini mixer
 *
 *
 * Copyright (C) 2000 Stefan Schimanski <1Stein@gmx.de>
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
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef KMIXERWIDGET_H
#define KMIXERWIDGET_H

#include <kpanelapplet.h>

#include <qwidget.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qptrlist.h>
#include <qstring.h>

#include "channel.h"
#include "mixer.h"

class Mixer;
class QSlider;
class Channel;
class KActionCollection;
class KConfig;


class KMixerWidget : public QWidget  {
   Q_OBJECT

  public:
   KMixerWidget( int _id, Mixer *mixer, QString mixerName, int mixerNum,
                 bool small, KPanelApplet::Direction, MixDevice::DeviceCategory categoryMask = MixDevice::ALL ,
                 QWidget *parent=0, const char *name=0 );
   ~KMixerWidget();

   void addActionToPopup( KAction *action );

   QString name() const { return m_name; };
   void setName( QString name ) { m_name = name; };

   Mixer *mixer() const { return m_mixer; };
   QString mixerName()  const { return m_mixerName; };
   int mixerNum() const { return m_mixerNum; };

   int id() const { return m_id; };

   struct Colors {
       QColor high, low, back, mutedHigh, mutedLow, mutedBack;
   };

  signals:
   void updateLayout();
   void masterMuted( bool );

  public slots:
   void setTicks( bool on );
   void setLabels( bool on );
   void setIcons( bool on );
   void setColors( const Colors &color );

   void saveConfig( KConfig *config, QString grp );
   void loadConfig( KConfig *config, QString grp );

   void showAll();

  private slots:
   void rightMouseClicked();
   void updateBalance();
   void updateSize();

  private:
   Mixer *m_mixer;
   QSlider *m_balanceSlider;
   QBoxLayout *m_topLayout;
   QBoxLayout *m_devLayout;
   QPtrList<Channel> m_channels;

   QString m_name;
   QString m_mixerName;
   int m_mixerNum;
   int m_id;

   KActionCollection *m_actions;

   bool m_small;
   KPanelApplet::Direction m_direction;
   bool m_iconsEnabled;
   bool m_labelsEnabled;
   bool m_ticksEnabled;

   void mousePressEvent( QMouseEvent *e );
   void createDeviceWidgets( KPanelApplet::Direction, MixDevice::DeviceCategory categoryMask );
   void createMasterVolWidget(KPanelApplet::Direction);
};

#endif
