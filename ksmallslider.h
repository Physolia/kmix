//-*-C++-*-
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
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef KSMALLSLIDER_H
#define KSMALLSLIDER_H

#include <qwidget.h>
#include <qpixmap.h>
#include <qrangecontrol.h>

class KSmallSlider : public QWidget, public QRangeControl
{
      Q_OBJECT

   public:
      KSmallSlider( QWidget *parent, const char *name=0 );
      KSmallSlider( Orientation, QWidget *parent, const char *name=0 );
      KSmallSlider( int minValue, int maxValue, int pageStep, int value, Orientation,
		    QWidget *parent, const char *name=0 );

      virtual void setOrientation( Orientation );
      Orientation orientation() const;
      virtual void setTracking( bool enable );
      bool tracking() const;    
      QSize sizeHint() const;
      QSizePolicy sizePolicy() const;
      QSize minimumSizeHint() const;

      int minValue() const;
      int maxValue() const;
      void setMinValue( int );
      void setMaxValue( int );
      int lineStep() const;
      int pageStep() const;
      void setLineStep( int );
      void setPageStep( int );
      int  value() const;
      
      bool gray() const;

public slots:
      virtual void setValue( int );
      void addStep();
      void subtractStep();
      void setGray( bool value );

      signals:
      void valueChanged( int value );
      void sliderPressed();
      void sliderMoved( int value );
      void sliderReleased();

   protected:
      void resizeEvent( QResizeEvent * );
      void paintEvent( QPaintEvent * );

      void mousePressEvent( QMouseEvent * );
      void mouseReleaseEvent( QMouseEvent * );
      void mouseMoveEvent( QMouseEvent * );
      void wheelEvent( QWheelEvent * );    

      void valueChange();
      void rangeChange();

   private:
      enum State { Idle, Dragging };

      void init();
      int positionFromValue( int ) const;
      int valueFromPosition( int ) const;
      void moveSlider( int );
      void reallyMoveSlider( int );
      void resetState();
      int slideLength() const;
      int available() const;
      int goodPart( const QPoint& ) const;
      void initTicks();

      QCOORD sliderPos;
      int sliderVal;
      State state;
      bool track; 
      bool grayed;
      Orientation orient;
};


inline bool KSmallSlider::tracking() const
{
    return track;
}

inline KSmallSlider::Orientation KSmallSlider::orientation() const
{
    return orient;
}

#endif
