/*
 * KMix -- KDE's full featured mini mixer
 *
 * Copyright 1996-2000 Christian Esken <esken@kde.org>
 * Copyright 2000-2003 Christian Esken <esken@kde.org>, Stefan Schimanski <1Stein@gmx.de>
 * Copyright 2002-2007 Christian Esken <esken@kde.org>, Helio Chissini de Castro <helio@conectiva.com.br>
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


// include files for QT
#include <QCheckBox>
#include <QLabel>
#include <QLayout>
#include <QMap>
#include <qradiobutton.h>
#include <KTabWidget>
#include <QToolTip>

// include files for KDE
#include <kcombobox.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kmenubar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kaction.h>
#include <kapplication.h>
#include <kstandardaction.h>
#include <kmenu.h>
#include <khelpmenu.h>
#include <kdebug.h>
#include <kxmlguifactory.h>
#include <kglobal.h>
#include <kactioncollection.h>
#include <ktoggleaction.h>

// KMix
#include "mixertoolbox.h"
#include "kmix.h"
#include "kmixdevicemanager.h"
#include "kmixerwidget.h"
#include "kmixprefdlg.h"
#include "kmixdockwidget.h"
#include "kmixtoolbox.h"
#include "viewdockareapopup.h"
//#include "osd.h" // Postponed to KDE4.1


/* KMixWindow
 * Constructs a mixer window (KMix main window)
 */
KMixWindow::KMixWindow()
   : KXmlGuiWindow(0),
   m_showTicks( true ),
   m_showMenubar(true),
//   m_isVisible (false),    // initialize, as we don't trigger a hideEvent()
//   m_visibilityUpdateAllowed( true ),
   m_multiDriverMode (false), // -<- I never-ever want the multi-drivermode to be activated by accident
   m_dockWidget()
   , _dockAreaPopup(0)
{
    setObjectName("KMixWindow");
    // disable delete-on-close because KMix might just sit in the background waiting for cards to be plugged in
    setAttribute(Qt::WA_DeleteOnClose, false);

   initActions(); // init actions first, so we can use them in the loadConfig() already
   loadConfig(); // Load config before initMixer(), e.g. due to "MultiDriver" keyword
   initWidgets();
   initPrefDlg();
   MixerToolBox::instance()->initMixer(m_multiDriverMode, m_hwInfoString);
   KMixDeviceManager *theKMixDeviceManager = KMixDeviceManager::instance();
   recreateGUI();
   theKMixDeviceManager->initHotplug();
   connect(theKMixDeviceManager, SIGNAL( plugged( const char*, const QString&, QString&)), SLOT (plugged( const char*, const QString&, QString&) ) );
   connect(theKMixDeviceManager, SIGNAL( unplugged( const QString&)), SLOT (unplugged( const QString&) ) );
   if ( m_startVisible )
      show(); // Started visible: We don't do "m_isVisible = true;", as the showEvent() already does it

   connect( kapp, SIGNAL( aboutToQuit()), SLOT( saveConfig()) );
}


KMixWindow::~KMixWindow()
{
   clearMixerWidgets();
   MixerToolBox::instance()->deinitMixer();
}


void KMixWindow::initActions()
{
   // file menu
   KStandardAction::quit( this, SLOT(quit()), actionCollection());

   // settings menu
   _actionShowMenubar = KStandardAction::showMenubar( this, SLOT(toggleMenuBar()), actionCollection());
   //actionCollection()->addAction( a->objectName(), a );
   KStandardAction::preferences( this, SLOT(showSettings()), actionCollection());
   KStandardAction::keyBindings( guiFactory(), SLOT(configureShortcuts()), actionCollection());

   QAction *action = actionCollection()->addAction( "hwinfo" );
   action->setText( i18n( "Hardware &Information" ) );
   connect(action, SIGNAL(triggered(bool) ), SLOT( slotHWInfo() ));
   action = actionCollection()->addAction( "hide_kmixwindow" );
   action->setText( i18n( "Hide Mixer Window" ) );
   connect(action, SIGNAL(triggered(bool) ), SLOT(hideOrClose()));
   action->setShortcut(QKeySequence(Qt::Key_Escape));
   action = actionCollection()->addAction("toggle_channels_currentview");
   action->setText(i18n("Configure &Channels"));
   connect(action, SIGNAL(triggered(bool) ), SLOT(slotConfigureCurrentView()));
   createGUI( "kmixui.rc" );
}


void KMixWindow::initPrefDlg()
{
   m_prefDlg = new KMixPrefDlg( this );
   connect( m_prefDlg, SIGNAL(signalApplied(KMixPrefDlg *)), SLOT(applyPrefs(KMixPrefDlg *)) );
}




void KMixWindow::initWidgets()
{
   // Main widget and layout
   setCentralWidget( new QWidget( this ) );

   // Widgets layout
   m_widgetsLayout = new QVBoxLayout(   centralWidget()   );
   m_widgetsLayout->setObjectName(   "m_widgetsLayout"   );
   m_widgetsLayout->setSpacing(   0   );
   m_widgetsLayout->setMargin (   0   );


   m_wsMixers = new KTabWidget( centralWidget() );
   connect( m_wsMixers, SIGNAL( currentChanged ( int ) ), SLOT( newMixerShown(int)) );

   m_widgetsLayout->addWidget(m_wsMixers);

   // show menubar if the actions says so (or if the action does not exist)
   menuBar()->setVisible( (_actionShowMenubar==0) || _actionShowMenubar->isChecked());

   m_widgetsLayout->activate();
}


/**
 * Updates the docking icon by recreating it.
 * @returns Whether the docking succeeded. Failure usually means that there
 *    was no suitable mixer control selected.
 */
bool KMixWindow::updateDocking()
{
   if (m_showDockWidget == false) {
        return false; // docking is disabled
    }

   // delete old dock widget
   if (m_dockWidget)
   {
      delete m_dockWidget;
      m_dockWidget = 0L;
   }
   if ( _dockAreaPopup ) {
      delete _dockAreaPopup;
      _dockAreaPopup = 0L;
   }

   if ( Mixer::mixers().count() == 0 ) {
      return false;
   }

    // create dock widget and the corresponding popup
    /* A GUIProfile does not make sense for the DockAreaPopup => Using (GUIProfile*)0 */
   QWidget* referenceWidgetForSystray = this;
     if ( m_volumeWidget ) {
      _dockAreaPopup = new ViewDockAreaPopup(0, "dockArea", Mixer::getGlobalMasterMixer(), 0, (GUIProfile*)0, this);
      _dockAreaPopup->createDeviceWidgets();
      referenceWidgetForSystray = _dockAreaPopup;
    }
    m_dockWidget = new KMixDockWidget( referenceWidgetForSystray, "mainDockWidget", _dockAreaPopup );
    m_dockWidget->show();
    return true;
}

void KMixWindow::saveConfig()
{
   saveBaseConfig();
   saveViewConfig();
   saveVolumes();
#ifdef __GNUC_
#warn We must Sync here, or we will lose configuration data. The reson for that is unknown.
#endif
   KGlobal::config()->sync();
}

void KMixWindow::saveBaseConfig()
{
   KConfigGroup config(KGlobal::config(), "Global");

   config.writeEntry( "Size", size() );
   config.writeEntry( "Position", pos() );
   // Cannot use isVisible() here, as in the "aboutToQuit()" case this widget is already hidden.
   // (Please note that the problem was only there when quitting via Systray - esken).
   // Using it again, as internal behaviour has changed with KDE4
   config.writeEntry( "Visible", isVisible() );
   config.writeEntry( "Menubar", _actionShowMenubar->isChecked() );
   config.writeEntry( "AllowDocking", m_showDockWidget );
   config.writeEntry( "TrayVolumeControl", m_volumeWidget );
   config.writeEntry( "Tickmarks", m_showTicks );
   config.writeEntry( "Labels", m_showLabels );
   config.writeEntry( "startkdeRestore", m_onLogin );
   Mixer* mixerMasterCard = Mixer::getGlobalMasterMixer();
   if ( mixerMasterCard != 0 ) {
      config.writeEntry( "MasterMixer", mixerMasterCard->id() );
   }
   MixDevice* mdMaster = Mixer::getGlobalMasterMD();
   if ( mdMaster != 0 ) {
      config.writeEntry( "MasterMixerDevice", mdMaster->id() );
   }
   QString mixerIgnoreExpression = MixerToolBox::instance()->mixerIgnoreExpression();
   config.writeEntry( "MixerIgnoreExpression", mixerIgnoreExpression );
   
   // @todo basically this should be moved in the views later (e.g. KDE4.2 ?)
   if ( m_toplevelOrientation  == Qt::Horizontal )
      config.writeEntry( "Orientation","Horizontal" );
   else
      config.writeEntry( "Orientation","Vertical" );
}

void KMixWindow::saveViewConfig()
{
    // Save Views
    for ( int i=0; i<m_wsMixers->count() ; ++i )
    {
        QWidget *w = m_wsMixers->widget(i);
        if ( w->inherits("KMixerWidget") ) {
            KMixerWidget* mw = (KMixerWidget*)w;
            // Here also Views are saved. even for Mixers that are closed. This is neccesary when unplugging cards.
            // Otherwise the user will be confused afer re-plugging the card (as the config was not saved).
            mw->saveConfig( KGlobal::config().data() );
        }
    }
}


/**
 * Stores the volumes of all mixers  Can be restored via loadVolumes() or
 * the kmixctrl application.
 */
void KMixWindow::saveVolumes()
{
   KConfig *cfg = new KConfig( "kmixctrlrc" );
   for ( int i=0; i<Mixer::mixers().count(); ++i)
   {
      Mixer *mixer = (Mixer::mixers())[i];
      if ( mixer->isOpen() ) { // protect from unplugged devices (better do *not* save them)
          mixer->volumeSave( cfg );
      }
   }
   delete cfg;
}



void KMixWindow::loadConfig()
{
   loadBaseConfig();
   //loadViewConfig(); // mw->loadConfig() explicitly called always after creating mw.
   //loadVolumes(); // not in use
}

void KMixWindow::loadBaseConfig()
{
    KConfigGroup config(KGlobal::config(), "Global");

   m_showDockWidget = config.readEntry("AllowDocking", true);
   m_volumeWidget = config.readEntry("TrayVolumeControl", true);
   m_showTicks = config.readEntry("Tickmarks", true);
   m_showLabels = config.readEntry("Labels", true);
   m_onLogin = config.readEntry("startkdeRestore", true );
   m_startVisible = config.readEntry("Visible", true);
   kDebug(67100) << "MultiDriver a = " << m_multiDriverMode;
   m_multiDriverMode = config.readEntry("MultiDriver", false);
   kDebug(67100) << "MultiDriver b = " << m_multiDriverMode;
   const QString& orientationString = config.readEntry("Orientation", "Vertical");
   QString mixerMasterCard = config.readEntry( "MasterMixer", "" );
   QString masterDev = config.readEntry( "MasterMixerDevice", "" );
   Mixer::setGlobalMaster(mixerMasterCard, masterDev);
   QString mixerIgnoreExpression = config.readEntry( "MixerIgnoreExpression", "Modem" );
   MixerToolBox::instance()->setMixerIgnoreExpression(mixerIgnoreExpression);

   if ( orientationString == "Horizontal" )
       m_toplevelOrientation  = Qt::Horizontal;
   else
       m_toplevelOrientation = Qt::Vertical;

   // show/hide menu bar
   bool showMenubar = config.readEntry("Menubar", true);

   if (_actionShowMenubar) _actionShowMenubar->setChecked( showMenubar );

   // restore window size and position
   if ( !kapp->isSessionRestored() ) // done by the session manager otherwise
   {
      QSize defSize( minimumWidth(), height() );
      QSize size = config.readEntry("Size", defSize );
      if(!size.isEmpty()) resize(size);

      QPoint defPos = pos();
      QPoint pos = config.readEntry("Position", defPos);
      move(pos);
   }
}

/**
 * Loads the volumes of all mixers from kmixctrlrc.
 * In other words:
 * Restores the default voumes as stored via saveVolumes() or the
 * execution of "kmixctrl --save"
 */
/* Currently this is not in use
void
KMixWindow::loadVolumes()
{
    KConfig *cfg = new KConfig( "kmixctrlrc", true );
    for ( int i=0; i<Mixer::mixers().count(); ++i)
    {
        Mixer *mixer = (Mixer::mixers())[i];
        mixer->volumeLoad( cfg );
    }
    delete cfg;
}
*/




/**
 * Create or recreate the Mixer GUI elements
 */
void KMixWindow::recreateGUI()
{
   saveViewConfig();  // save the state before recreating
   clearMixerWidgets();
   if ( Mixer::mixers().count() > 0 ) {
      for (int i=0; i<Mixer::mixers().count(); ++i) {
         Mixer *mixer = (Mixer::mixers())[i];
         addMixerWidget(mixer->id());
      }
      bool dockingSucceded = updateDocking();
      if( !dockingSucceded && Mixer::mixers().count() > 0 )
         show(); // avoid invisible and unaccessible main window
   }
   else {
      // No soundcard found. Do not complain, but sit in the background, and wait for newly plugged soundcards.
       updateDocking();  // -<- removes the DockIcon
       hide();
   }
}

void KMixWindow::plugged( const char* driverName, const QString& udi, QString& dev)
{
    kDebug(67100) << "Plugged: dev=" << dev << "(" << driverName << ") udi=" << udi << "\n";
    QString driverNameString;
    driverNameString = driverName;
    int devNum = dev.toInt();
    Mixer *mixer = new Mixer( driverNameString, devNum );
    if ( mixer != 0 ) {
        kDebug(67100) << "Plugged: dev=" << dev << "\n";
        MixerToolBox::instance()->possiblyAddMixer(mixer);
        recreateGUI();
    }

// Test code for OSD. But OSD is postponed to KDE4.1
//    OSDWidget* osd = new OSDWidget(0);
//    osd->volChanged(70, true);

}

void KMixWindow::unplugged( const QString& udi)
{
    kDebug(67100) << "Unplugged: udi=" <<udi << "\n";
    for (int i=0; i<Mixer::mixers().count(); ++i) {
        Mixer *mixer = (Mixer::mixers())[i];
        kDebug(67100) << "Try Match with:" << mixer->udi() << "\n";
        if (mixer->udi() == udi ) {
            kDebug(67100) << "Unplugged Match: Removing udi=" <<udi << "\n";
            //KMixToolBox::notification("MasterFallback", "aaa");
            bool globalMasterMixerDestroyed = ( mixer == Mixer::getGlobalMasterMixer() );
            // Part 1) Remove Tab
            for ( int i=0; i<m_wsMixers->count() ; ++i )
            {
                QWidget *w = m_wsMixers->widget(i);
                KMixerWidget* kmw = ::qobject_cast<KMixerWidget*>(w);
                if ( kmw && kmw->mixer() ==  mixer ) {
                    kmw->saveConfig( KGlobal::config().data() );
                    m_wsMixers->removeTab(i);
                    delete kmw;
                    i= -1; // Restart loop from scratch (indices are most likeliy invalidated at removeTab() )
                }
            }
            MixerToolBox::instance()->removeMixer(mixer);
            // Check whether the Global Master disappeared, and select a new one if neccesary
            MixDevice* md = Mixer::getGlobalMasterMD();
            if ( globalMasterMixerDestroyed || md == 0 ) {
                // We don't know what the global master should be now.
                // So lets play stupid, and just select the recommendended master of the first device
                if ( Mixer::mixers().count() > 0 ) {
                    QString localMaster = ((Mixer::mixers())[0])->getLocalMasterMD()->id();
                    Mixer::setGlobalMaster( ((Mixer::mixers())[0])->id(), localMaster);
                    
                    QString text;
                    text = i18n("The soundcard containing the master device was unplugged. Changing to control %1 on card %2", 
                            ((Mixer::mixers())[0])->getLocalMasterMD()->readableName(),
                            ((Mixer::mixers())[0])->readableName()
                                );
                    KMixToolBox::notification("MasterFallback", text);
                }
            }
            if ( Mixer::mixers().count() == 0 ) {
                QString text;
                text = i18n("The last soundcard was unplugged.");
                KMixToolBox::notification("MasterFallback", text);
            }
            recreateGUI();
            break;
        }
    }

}


/**
 * Create a widget with an error message
 * This widget shows an error message like "no mixers detected.
void KMixWindow::setErrorMixerWidget()
{
   QString s = i18n("Please plug in your soundcard.No soundcard found. Probably you have not set it up or are missing soundcard drivers. Please check your operating system manual for installing your soundcard."); // !! better text
   m_errorLabel = new QLabel( s,this  );
   m_errorLabel->setAlignment( Qt::AlignCenter );
   m_errorLabel->setWordWrap(true);
   m_errorLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
   m_wsMixers->addTab( m_errorLabel, i18n("No soundcard found") );
}
 */

void KMixWindow::clearMixerWidgets()
{
   while ( m_wsMixers->count() != 0 )
   {
      QWidget *mw = m_wsMixers->widget(0);
      m_wsMixers->removeTab(0);
      delete mw;
   }
}



void KMixWindow::addMixerWidget(const QString& mixer_ID)
{
   kDebug(67100) << "KMixWindow::addMixerWidget() " << mixer_ID;
   Mixer *mixer = MixerToolBox::instance()->find(mixer_ID);
   if ( mixer != 0 )
   {
      kDebug(67100) << "KMixWindow::addMixerWidget() " << mixer_ID << " is being added";
      ViewBase::ViewFlags vflags = ViewBase::HasMenuBar;
      if ( m_showMenubar ) {
            vflags |= ViewBase::MenuBarVisible;
      }
      if ( m_toplevelOrientation == Qt::Vertical ) {
            vflags |= ViewBase::Horizontal;
      }
      else {
            vflags |= ViewBase::Vertical;
      }


      KMixerWidget *kmw = new KMixerWidget( mixer, this, "KMixerWidget", vflags, actionCollection() );

      /* A newly added mixer will automatically added at the top
      * and thus the window title is also set appropriately */
      bool isFirstTab = m_wsMixers->count() == 0;
      m_wsMixers->addTab( kmw, kmw->mixer()->readableName() );
      if (isFirstTab) {
         m_wsMixers->setCurrentWidget(kmw);
         //setWindowTitle( kmw->mixer()->readableName() );
      }

      kmw->loadConfig( KGlobal::config().data() );

      kmw->setTicks( m_showTicks );
      kmw->setLabels( m_showLabels );
      kmw->mixer()->readSetFromHWforceUpdate();
   } // given mixer exist really
}



bool KMixWindow::queryClose ( )
{
    kDebug(67100) << "queryClose ";
    if ( m_showDockWidget && !kapp->sessionSaving() )
    {
        kDebug(67100) << "don't close";
        // Hide (don't close and destroy), if docking is enabled. Except when session saving (shutdown) is in process.
        hide();
        return false;
    }
    else {
        // Accept the close in all situations, when the user has disabled docking
        kDebug(67100) << "close";
        return true;
    }
}

void KMixWindow::hideOrClose ( )
{
    if ( m_showDockWidget  && m_dockWidget != 0) {
        // we can hide if there is a dock widget
        hide();
    }
    else {
        //  if there is no dock widget, we will quit
        quit();
    }
}

void KMixWindow::quit()
{
    kDebug(67100) << "quit";
    kapp->quit();
}


void KMixWindow::showSettings()
{
   if (!m_prefDlg->isVisible())
   {
      // copy actual values to dialog
      m_prefDlg->m_dockingChk->setChecked( m_showDockWidget );
      m_prefDlg->m_volumeChk->setChecked(m_volumeWidget);
      m_prefDlg->m_volumeChk->setEnabled( m_showDockWidget );
      m_prefDlg->m_onLogin->setChecked( m_onLogin );

      m_prefDlg->m_showTicks->setChecked( m_showTicks );
      m_prefDlg->m_showLabels->setChecked( m_showLabels );
      m_prefDlg->_rbVertical  ->setChecked( m_toplevelOrientation == Qt::Vertical );
      m_prefDlg->_rbHorizontal->setChecked( m_toplevelOrientation == Qt::Horizontal );

      // show dialog
      m_prefDlg->show();
   }
}


void KMixWindow::showHelp()
{
   actionCollection()->action( "help_contents" )->trigger();
}


void
KMixWindow::showAbout()
{
   actionCollection()->action( "help_about_app" )->trigger();
}



void KMixWindow::applyPrefs( KMixPrefDlg *prefDlg )
{
   bool labelsHasChanged = m_showLabels ^ prefDlg->m_showLabels->isChecked();
   bool ticksHasChanged = m_showTicks ^ prefDlg->m_showTicks->isChecked();
   bool dockwidgetHasChanged = m_showDockWidget ^ prefDlg->m_dockingChk->isChecked();
   bool systrayPopupHasChanged = m_volumeWidget ^ prefDlg->m_volumeChk->isChecked();
   bool toplevelOrientationHasChanged =
        ( prefDlg->_rbVertical->isChecked()   && m_toplevelOrientation == Qt::Horizontal )
     || ( prefDlg->_rbHorizontal->isChecked() && m_toplevelOrientation == Qt::Vertical   );

   m_showLabels = prefDlg->m_showLabels->isChecked();
   m_showTicks = prefDlg->m_showTicks->isChecked();
   m_showDockWidget = prefDlg->m_dockingChk->isChecked();
   m_volumeWidget = prefDlg->m_volumeChk->isChecked();
   m_onLogin = prefDlg->m_onLogin->isChecked();
   if ( prefDlg->_rbVertical->isChecked() ) {
      m_toplevelOrientation = Qt::Vertical;
   }
   else if ( prefDlg->_rbHorizontal->isChecked() ) {
      m_toplevelOrientation = Qt::Horizontal;
   }

   if ( labelsHasChanged || ticksHasChanged || dockwidgetHasChanged || toplevelOrientationHasChanged || systrayPopupHasChanged) {
      recreateGUI();
   }

   this->repaint(); // make KMix look fast (saveConfig() often uses several seconds)
   kapp->processEvents();
   saveConfig();
}


void KMixWindow::toggleMenuBar()
{
   menuBar()->setVisible(_actionShowMenubar->isChecked());
}

/*
void KMixWindow::showEvent( QShowEvent * )
{
   if ( m_visibilityUpdateAllowed )
      m_isVisible = isVisible();
}

void KMixWindow::hideEvent( QHideEvent * )
{
   if ( m_visibilityUpdateAllowed )
   {
      m_isVisible = isVisible();
   }
}

void KMixWindow::stopVisibilityUpdates()
{
   m_visibilityUpdateAllowed = false;
}
*/

void KMixWindow::slotHWInfo()
{
   KMessageBox::information( 0, m_hwInfoString, i18n("Mixer Hardware Information") );
}

void KMixWindow::slotConfigureCurrentView()
{
    KMixerWidget* mw = (KMixerWidget*)m_wsMixers->currentWidget();
    ViewBase* view = 0;
    if (mw) view = mw->currentView();
    if (view) view->configureView();
}

void KMixWindow::newMixerShown(int /*tabIndex*/ ) {
   KMixerWidget* mw = (KMixerWidget*)m_wsMixers->currentWidget();
   Mixer* mixer = mw->mixer();
   setWindowTitle( mixer->readableName() );
   // As switching the tab does NOT mean switching the mixer, we do not need to update dock icon here.
   // It would lead to unnecesary flickering of the (complete) dock area.
}



#include "kmix.moc"
