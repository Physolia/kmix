/*
 * KDockWidget
 *
 *              Copyright (C) 1999 Christian Esken
 *                       esken@kde.org
 * 
 * This class is based on a contribution by Harri Porten <porten@tu-harburg.de>
 * It has been reworked by Christian Esken to be a generic base class for
 * docking.
 *
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


#ifndef _DOCKING_H_
#define _DOCKING_H_

#include <ktmainwindow.h>
#include <qpixmap.h>
#include <qpoint.h>

/**
 This class creates a widget that allows applications to display a widget
 on the docking area of the panel. The following services are provided:
 A popup menu with some standard entries is created. You can
 place further entries in there. The menu is being shown by a right-button
 click on the widget.
 Hide and show a main window  
 */
class KDockWidget : public QWidget {

  Q_OBJECT

public:
  /// Creates a docking widget and allows passing of the name of
  /// the docking icon. After the construcor has run
  KDockWidget(const char *name=0, const char *dockIconName=0);
  /// Overloader constructor. Only differs from the previous constructor
  /// in that you can pass the icon as a QPixmap
  KDockWidget(const char *name=0, QPixmap* dockPixmap=0);
  /// Deletes the docking widget. Please note that the widget undocks from
  /// the panel automatically.
  ~KDockWidget();
  void setMainWindow(KTMainWindow *ktmw);
  QPixmap* dockPixmap() const;
  /// Checks, if application is in "docked" state. Returns true, if yes.
  bool isDocked() const;
  void savePosition();
  void setPixmap(const char* dockPixmapName);
  void setPixmap(QPixmap* dockPixmap);

public slots:
  /// Dock this widget - this means to show this widget on the docking area
  virtual void dock();
  /// Undock this widget - this means to remove this widget from the docking area
  virtual void undock();

protected:
  void paintEvent(QPaintEvent *e);
  void paintIcon();
  void baseInit();

private slots:
  void timeclick();
  void toggle_window_state();
  void mousePressEvent(QMouseEvent *e);


signals:
  void quit_clicked();

protected slots:
  void emit_quit();

protected:
  bool		docked;
  int		toggleID;
  int		pos_x;
  int		pos_y;
  KTMainWindow	*ktmw;
  QPopupMenu	*popup_m;
  QPixmap	*pm_dockPixmap;
  bool		dockingInProgress;
 };

#endif

