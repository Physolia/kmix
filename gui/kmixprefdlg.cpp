/*
 * KMix -- KDE's full featured mini mixer
 *
 *
 * Copyright (C) 2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2001 Preston Brown <pbrown@kde.org>
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

#include "kmixprefdlg.h"

#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qlabel.h>
#include <qradiobutton.h>
#include <qgroupbox.h>
#include <qguiapplication.h>
#include <qlistwidget.h>

#include <kconfig.h>
#include <klocalizedstring.h>
#include <kmessagewidget.h>

#include "dialogbase.h"
#include "dialogstatesaver.h"
#include "gui/kmixerwidget.h"
#include "core/mixertoolbox.h"
#include "settings.h"


/* static */ KMixPrefDlg *KMixPrefDlg::instance(QWidget *parent)
{
	KMixPrefDlg *sInstance = new KMixPrefDlg(parent);
	return (sInstance);
}


KMixPrefDlg::KMixPrefDlg(QWidget *parent)
	: KConfigDialog(parent, i18n("Configure"), Settings::self())
{
	setFaceType(KPageDialog::List);

	m_startupTab = m_generalTab = m_controlsTab = nullptr;

	new DialogStateSaver(this);			// save dialogue size when closed
}


void KMixPrefDlg::showAtPage(KMixPrefDlg::PrefPage page)
{
	// First create the pages and their widgets if not done already.
	if (m_startupTab==nullptr)
	{
		qCDebug(KMIX_LOG) << "creating pages";

		m_startupTab = new QFrame(this);
		createStartupTab();
		m_generalTab = new QFrame(this);
		createGeneralTab();
		m_controlsTab = new QFrame(this);
		createControlsTab();

		// I thought KConfigDialog would call this, but I saw
		// during a gdb session that it does not do so.
		// However, there is no need to track changes or emit
		// the change signal during this initialisation.
		// See settingChanged().
		QSignalBlocker block(this);
		updateWidgets();

		m_generalPage = addPage(m_generalTab, i18n("General"), "configure");
		m_startupPage = addPage(m_startupTab, i18n("Startup"), "preferences-system-login");
		m_soundmenuPage = addPage(m_controlsTab, i18n("Volume Control"), "audio-volume-high");
	}

	// Then the requested page can be selected.
	switch (page)
	{
	case PageGeneral:
		setCurrentPage(m_generalPage);
		break;
	case PageStartup:
		setCurrentPage(m_startupPage);
		break;
	case PageVolumeControl:
		setCurrentPage(m_soundmenuPage);
		break;
	default:
		qCWarning(KMIX_LOG) << "Unknown preferences page" << page;
		break;
	}

	show();						// show with the selected page
}


// --- TABS --------------------------------------------------------------------------------------------------
void KMixPrefDlg::createStartupTab()
{
	layoutStartupTab = new QVBoxLayout(m_startupTab);
	layoutStartupTab->setContentsMargins(0, 0, 0, 0);
	layoutStartupTab->setSpacing(DialogBase::verticalSpacing());

	allowAutostart = new QCheckBox(i18n("Start KMix on desktop startup"), m_startupTab);
	allowAutostart->setChecked(Settings::autoStart());
	connect(allowAutostart, &QAbstractButton::toggled, this, [this]() { settingChanged(); });
	addWidgetToLayout(allowAutostart, layoutStartupTab, 10,
			  i18n("Start KMix automatically when the desktop starts"));

	allowAutostartWarning = new KMessageWidget(
		i18n("Autostart will not work, because the autostart file kmix_autostart.desktop is missing. Check that KMix is installed correctly."), m_startupTab);
	allowAutostartWarning->setIcon(QIcon::fromTheme("dialog-error"));
	allowAutostartWarning->setMessageType(KMessageWidget::Error);
	allowAutostartWarning->setCloseButtonVisible(false);
	allowAutostartWarning->setWordWrap(true);
	allowAutostartWarning->setVisible(false);
	addWidgetToLayout(allowAutostartWarning, layoutStartupTab, 2, "");

	layoutStartupTab->addItem(new QSpacerItem(1, 2*DialogBase::verticalSpacing()));

	m_onLogin = new QCheckBox(i18n("Restore previous volume settings on desktop startup"), m_startupTab);
	m_onLogin->setChecked(Settings::startRestore());
	connect(m_onLogin, &QAbstractButton::toggled, this, [this]() { settingChanged(); });
	addWidgetToLayout(m_onLogin, layoutStartupTab, 10,
			  i18n("Restore all mixer volume levels and switches to their last used settings when the desktop starts"));

	dynamicControlsRestoreWarning = new KMessageWidget(
		i18n("The volume of PulseAudio or MPRIS2 dynamic controls will not be restored."), m_startupTab);
	dynamicControlsRestoreWarning->setIcon(QIcon::fromTheme("dialog-warning"));
	dynamicControlsRestoreWarning->setMessageType(KMessageWidget::Warning);
	dynamicControlsRestoreWarning->setCloseButtonVisible(false);
	dynamicControlsRestoreWarning->setWordWrap(true);
	dynamicControlsRestoreWarning->setVisible(false);
	addWidgetToLayout(dynamicControlsRestoreWarning, layoutStartupTab, 2, "");

	layoutStartupTab->addStretch();
}


void KMixPrefDlg::createOrientationGroup(const QString &labelSliderOrientation, QGridLayout *orientationLayout, int row, KMixPrefDlgPrefOrientationType prefType)
{
	QButtonGroup* orientationGroup = new QButtonGroup(m_generalTab);
	orientationGroup->setExclusive(true);
	QLabel* qlb = new QLabel(labelSliderOrientation, m_generalTab);

	QRadioButton* qrbHor = new QRadioButton(i18n("&Horizontal"), m_generalTab);
	QRadioButton* qrbVert = new QRadioButton(i18n("&Vertical"), m_generalTab);

	if (prefType == TrayOrientation)
	{
		_rbTraypopupHorizontal = qrbHor;
		_rbTraypopupVertical = qrbVert;
	}
	else
	{
		_rbHorizontal = qrbHor;
		_rbVertical = qrbVert;
	}

	// Add both buttons to button group
	orientationGroup->addButton(qrbHor);
	orientationGroup->addButton(qrbVert);
	// Add both buttons and label to layout
	orientationLayout->addWidget(qlb, row, 0, Qt::AlignLeft);
	orientationLayout->addWidget(qrbHor, row, 1, Qt::AlignLeft);
	orientationLayout->addWidget(qrbVert, row, 2, Qt::AlignLeft);
	orientationLayout->setColumnStretch(2, 1);

	connect(qrbHor, &QAbstractButton::toggled, this, [this]() { settingChanged(KMixPrefDlg::ChangedControls); });
	connect(qrbVert, &QAbstractButton::toggled, this, [this]() { settingChanged(KMixPrefDlg::ChangedControls); });
}


void KMixPrefDlg::setOrientationTooltip(QGridLayout *orientationLayout, int row, const QString &tooltip)
{
	for (int c = 0; c<=2; ++c)
	{
		QLayoutItem *item = orientationLayout->itemAtPosition(row, c);
		if (item==nullptr) continue;
		QWidget *widget = item->widget();
		if (widget==nullptr) continue;
		widget->setToolTip(tooltip);
	}
}


void KMixPrefDlg::createGeneralTab()
{
	QBoxLayout* layout = new QVBoxLayout(m_generalTab);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(DialogBase::verticalSpacing());

	// --- Behavior ---------------------------------------------------------
	QGroupBox *grp = new QGroupBox(i18n("Behavior"), m_generalTab);
	grp->setFlat(true);
	layout->addWidget(grp);

	auto *behaviorLayout = new QVBoxLayout;
	behaviorLayout->setContentsMargins(0, 0, 0, 0);
	grp->setLayout(behaviorLayout);

	// Only PulseAudio supports volume overdrive.  Ignore that
	// configuration option for other backends, and show a warning
	// message.
	const bool pulseAudioAvailable = MixerToolBox::pulseaudioPresent();

	// Volume Overdrive
	m_volumeOverdrive = new QCheckBox(i18n("Volume overdrive"), grp);
	m_volumeOverdrive->setChecked(pulseAudioAvailable && Settings::volumeOverdrive());
	m_volumeOverdrive->setEnabled(pulseAudioAvailable);
	connect(m_volumeOverdrive, &QAbstractButton::toggled, this, [this]() { settingChanged(); });
	addWidgetToLayout(m_volumeOverdrive, behaviorLayout, 10,
			  i18nc("@info:tooltip", "Allow the volume to be raised above the recommended maximum (typically to 150%)"));

	// Volume Overdrive warning
	m_pulseOnlyWarning = new KMessageWidget(i18n("Volume overdrive is only available for PulseAudio."), grp);
	m_pulseOnlyWarning->setIcon(QIcon::fromTheme("dialog-warning"));
	m_pulseOnlyWarning->setMessageType(KMessageWidget::Warning);
	m_pulseOnlyWarning->setCloseButtonVisible(false);
	m_pulseOnlyWarning->setWordWrap(true);
	m_pulseOnlyWarning->setVisible(!pulseAudioAvailable);
	addWidgetToLayout(m_pulseOnlyWarning, behaviorLayout, 2, "");

	// Restart warning
	m_restartWarning = new KMessageWidget(
		i18n("%1 must be restarted for the volume overdrive setting to take effect.",
		     QGuiApplication::applicationDisplayName()), grp);
	m_restartWarning->setIcon(QIcon::fromTheme("dialog-information"));
	m_restartWarning->setMessageType(KMessageWidget::Information);
	m_restartWarning->setCloseButtonVisible(false);
	m_restartWarning->setWordWrap(true);
	m_restartWarning->setVisible(pulseAudioAvailable);
	addWidgetToLayout(m_restartWarning, behaviorLayout, 2, "");

	// Volume Feedback
	m_beepOnVolumeChange = new QCheckBox(i18n("Volume feedback"), grp);
	m_beepOnVolumeChange->setChecked(Settings::beepOnVolumeChange());
	connect(m_beepOnVolumeChange, &QAbstractButton::toggled, this, [this]() { settingChanged(); });
	addWidgetToLayout(m_beepOnVolumeChange, behaviorLayout, 10,
			  i18nc("@info:tooltip", "Play a sample sound when the volume changes"));

#ifndef HAVE_CANBERRA
	// Volume Feedback warning
	KMessageWidget *canberraWarning = new KMessageWidget(i18n("Volume feedback is only available when configured with Canberra."), grp);
	canberraWarning->setIcon(QIcon::fromTheme("dialog-warning"));
	canberraWarning->setMessageType(KMessageWidget::Warning);
	canberraWarning->setCloseButtonVisible(false);
	canberraWarning->setWordWrap(true);
	addWidgetToLayout(canberraWarning, behaviorLayout, 2, "");
	// Volume Feedback check box
	m_beepOnVolumeChange->setChecked(false);
	m_beepOnVolumeChange->setEnabled(false);
#endif
	// Volume Step layout
	QHBoxLayout *horizontalGrid = new QHBoxLayout();
	horizontalGrid->setMargin(0);

	// Volume Step spin box
	m_volumeStep = new QSpinBox(grp);
	m_volumeStep->setSuffix(" %");
	m_volumeStep->setRange(1, 50);
	m_volumeStep->setValue(Settings::volumePercentageStep());
	connect(m_volumeStep, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { settingChanged(); });

	m_volumeStep->setToolTip(xi18nc("@info:tooltip",
					"<para>Set the volume step as a percentage of the volume range.</para>"
					"<para>This affects changing the volume via hot keys, "
					"with the mouse wheel over the system tray icon, "
					"or moving sliders by a page step.</para>"
					"<para>%1 must be restarted for this change to take effect.</para>",
					QGuiApplication::applicationDisplayName()));

	horizontalGrid->addWidget(new QLabel(i18n("Volume step:"), m_generalTab));
	horizontalGrid->addWidget(m_volumeStep);
	horizontalGrid->addItem(new QSpacerItem(1 ,1 , QSizePolicy::Expanding));
	behaviorLayout->addItem(DialogBase::verticalSpacerItem());
	behaviorLayout->addLayout(horizontalGrid);

	// --- Visual ---------------------------------------------------------
	layout->addItem(DialogBase::verticalSpacerItem());
	grp = new QGroupBox(i18n("Visual"), m_generalTab);
	grp->setFlat(true);
	layout->addWidget(grp);

	auto *visualLayout = new QVBoxLayout;
	visualLayout->setContentsMargins(0, 0, 0, 0);
	grp->setLayout(visualLayout);

	// [CONFIG]
	m_showTicks = new QCheckBox(i18n("Show slider &tickmarks"), grp);
	m_showTicks->setChecked(Settings::showTicks());
	connect(m_showTicks, &QAbstractButton::toggled, this, [this]() { settingChanged(KMixPrefDlg::ChangedGui); });
	addWidgetToLayout(m_showTicks, visualLayout, 10, i18n("Show tick mark scales on sliders"));

	m_showLabels = new QCheckBox(i18n("Show control &labels"), grp);
	m_showLabels->setChecked(Settings::showLabels());
	connect(m_showLabels, &QAbstractButton::toggled, this, [this]() { settingChanged(KMixPrefDlg::ChangedGui); });
	addWidgetToLayout(m_showLabels, visualLayout, 10, i18n("Show description labels for sliders"));

	// [CONFIG]
	m_showOSD = new QCheckBox(i18n("Show On Screen Display (&OSD)"), grp);
	m_showOSD->setChecked(Settings::showOSD());
	connect(m_showOSD, &QAbstractButton::toggled, this, [this]() { settingChanged(); });
	addWidgetToLayout(m_showOSD, visualLayout, 10, i18n("Show an on-screen indicator when changing the volume via hotkeys"));

	// [CONFIG] Slider orientation (main window)
	visualLayout->addItem(DialogBase::verticalSpacerItem());
	QGridLayout* orientationGrid = new QGridLayout();
	orientationGrid->setHorizontalSpacing(DialogBase::horizontalSpacing());
	visualLayout->addLayout(orientationGrid);

	// Slider orientation (main window, and tray popup separately).
	createOrientationGroup(i18n("Slider orientation (main window): "), orientationGrid, 0, KMixPrefDlg::MainOrientation);
	createOrientationGroup(i18n("Slider orientation (system tray popup):"), orientationGrid, 1, KMixPrefDlg::TrayOrientation);
	setOrientationTooltip(orientationGrid, 0, i18n("Set the orientation of the main window control sliders"));
	setOrientationTooltip(orientationGrid, 1, i18n("Set the orientation of the system tray volume control sliders"));

	// Push everything above to the top
	layout->addStretch();
}

void KMixPrefDlg::createControlsTab()
{
	layoutControlsTab = new QVBoxLayout(m_controlsTab);
	layoutControlsTab->setContentsMargins(0, 0, 0, 0);
	layoutControlsTab->setSpacing(DialogBase::verticalSpacing());

	m_dockingChk = new QCheckBox(i18n("Dock in system tray"), m_controlsTab);
	m_dockingChk->setChecked(Settings::showDockWidget());
	connect(m_dockingChk, &QAbstractButton::toggled, this, [this]() { settingChanged(KMixPrefDlg::ChangedControls); });
	addWidgetToLayout(m_dockingChk, layoutControlsTab, 10, i18n("Dock the mixer into the system tray. Click on it to open the popup volume control."));

	layoutControlsTab->addItem(new QSpacerItem(1, 2*DialogBase::verticalSpacing()));

	QLabel *topLabel = new QLabel(i18n("Mixers to show in the popup volume control:"), this);
	layoutControlsTab->addWidget(topLabel);

	m_mixerList = new QListWidget(this);
	m_mixerList->setUniformItemSizes(true);
	m_mixerList->setAlternatingRowColors(true);
	m_mixerList->setSelectionMode(QAbstractItemView::NoSelection);
	m_mixerList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#ifndef QT_NO_ACCESSIBILITY
	m_mixerList->setAccessibleName(i18n("Select Mixers"));
#endif
	m_mixerList->setToolTip(i18n("The mixers that are checked here will be shown in the popup volume control."));

	connect(m_mixerList, &QListWidget::itemChanged, this, [this]()
	{
		settingChanged(KMixPrefDlg::ChangedMaster);
	});
	connect(m_mixerList, &QListWidget::itemActivated, this, [](QListWidgetItem *item)
	{
		item->setCheckState(item->checkState()==Qt::Checked ? Qt::Unchecked : Qt::Checked);
	});

	layoutControlsTab->addWidget(m_mixerList);
	layoutControlsTab->setStretchFactor(m_mixerList, 1);
}


// --- Helper --------------------------------------------------------------------------------------------------

/**
 * Register widget with correct name for KConfigDialog, then add it to the given layout
 *
 * @param widget
 * @param layout
 * @param spacingBefore
 * @param toopTipText
 * @param objectName
 */
void KMixPrefDlg::addWidgetToLayout(QWidget *widget, QBoxLayout *layout, int spacingBefore, const QString &tooltip)
{
	if (!tooltip.isEmpty()) widget->setToolTip(tooltip);

	QBoxLayout *l = new QHBoxLayout();
	l->addSpacing(spacingBefore);
	l->addWidget(widget);
	layout->addItem(l);
}

// --- KConfigDialog CUSTOM WIDGET management ------------------------------------------------------------------------


/**
 * Update Widgets from config.
 * <p>
 * Hint: this get internally called by KConfigdialog on initialization and reset.
 */
void KMixPrefDlg::updateWidgets()
{
	qCDebug(KMIX_LOG);

	const bool toplevelHorizontal = static_cast<Qt::Orientation>(Settings::orientationMainWindow())==Qt::Horizontal;
	_rbHorizontal->setChecked(toplevelHorizontal);
	_rbVertical->setChecked(!toplevelHorizontal);

	const bool trayHorizontal = static_cast<Qt::Orientation>(Settings::orientationTrayPopup())==Qt::Horizontal;
	_rbTraypopupHorizontal->setChecked(trayHorizontal);
	_rbTraypopupVertical->setChecked(!trayHorizontal);
}


/**
 * Updates config from the widgets. And emits the signal kmixConfigHasChanged().
 * <p>
 * Hint: this gets internally called by KConfigDialog after pressing the OK or Apply button.
 */
void KMixPrefDlg::updateSettings()
{
	const Qt::Orientation toplevelOrientation = _rbHorizontal->isChecked() ? Qt::Horizontal : Qt::Vertical;
	Settings::setOrientationMainWindow(toplevelOrientation);

	const Qt::Orientation trayOrientation = _rbTraypopupHorizontal->isChecked() ? Qt::Horizontal : Qt::Vertical;
	Settings::setOrientationTrayPopup(trayOrientation);

	Settings::setBeepOnVolumeChange(m_beepOnVolumeChange->isChecked());
	Settings::setAutoStart(allowAutostart->isChecked());
	Settings::setStartRestore(m_onLogin->isChecked());
	Settings::setVolumeOverdrive(m_volumeOverdrive->isChecked());
	Settings::setVolumePercentageStep(m_volumeStep->value());
	Settings::setShowTicks(m_showTicks->isChecked());
	Settings::setShowLabels(m_showLabels->isChecked());
	Settings::setShowOSD(m_showOSD->isChecked());
	Settings::setShowDockWidget(m_dockingChk->isChecked());

	KConfigGroup grp(KSharedConfig::openConfig(), "SystemTray");
	const int num = m_mixerList->count();
	QStringList trayConfig;

	qDebug() << "saving" << num << "mixers for popup";
	for (int i = 0; i<num; ++i)
	{
		// So that the listed mixers remain ordered.
		// "1000 mixers should be enough for anyone"
		const QString key = QString("%1").arg(i, 3, 10, QLatin1Char('0'));

		const QListWidgetItem *item = m_mixerList->item(i);
		const QString &id = item->data(Qt::UserRole).toString();

		// The settings are stored as "id,enabled,icon[,readable]"
		QStringList data(id);
		const bool isEnabled = item->checkState()==Qt::Checked;
		data.append(isEnabled ? "1" : "0");
		data.append(item->icon().name());

		const Mixer *mixer = MixerToolBox::findMixer(id);
		if (mixer!=nullptr) data.append(mixer->readableName());

		grp.writeEntry(key, data);

		// Save enabled mixers to this list, which is used by the
		// system tray popup.
		if (isEnabled) trayConfig.append(id);
	}

	Settings::setMixersForSoundMenu(trayConfig);	// for the system tray popup
	Settings::self()->save();

	qCDebug(KMIX_LOG) << "controls changed" << m_controlsChanged;
	emit kmixConfigHasChanged(m_controlsChanged);
	m_controlsChanged = KMixPrefDlg::ChangedNone;
}


//  The decision on what needed to be updated, based on any changed settings,
//  was originally made in KMixWindow::applyPrefs().  Now the decision is made
//  here based on the KMixPrefDlg::PrefChanged flag passed to this function.
//  If a control does not need to announce any change then it passes the default
//  argument KMixPrefDlg::ChangedAny which causes the OK/Apply buttons to be
//  updated only.  If a control needs to announce a change then it should pass
//  the appropriate KMixPrefDlg::PrefChanged flag.  The combined change flags
//  will be sent out by the kmixConfigHasChanged() signal.
//
//  The criteria as originally implemented in KMixWindow::applyPrefs() for announcing
//  changes are:
//
//    Show dock widget		Need a complete relayout =>	announce a ControlList change
//    Main window orientation					so that the complete GUI gets
//    Tray popup orientation					rebuilt
//    Tray popup controls
//
//    Show labels		Controls appearance change =>	announce a GUI change so that
//    Show tickmarks						existing controls are updated
//
//  Also checked in KMixPrefDlg::updateSettings() was:
//
//    Sound devices shown	Tray popup needs update =>	announce MasterChanged to
//								rebuild the tray popup
//
//  because the ViewDockAreaPopup primarily shows master volume(s).  In any case,
//  ViewDockAreaPopup treats MasterChanged and ControlList the same, so it is better
//  to announce the "smaller" change.
//
void KMixPrefDlg::settingChanged(KMixPrefDlg::PrefChanged changes)
{
	m_controlsChanged |= changes;
	qCDebug(KMIX_LOG) << "changed" << m_controlsChanged;
	if (!signalsBlocked()) KConfigDialog::settingsChangedSlot();
}


/**
 * Returns whether the custom widgets (orientation checkboxes) has changed.
 * <p>
 * Hint: this get internally called by KConfigDialog from updateButtons().
 * @return
 */
bool KMixPrefDlg::hasChanged()
{
	qCDebug(KMIX_LOG) << "changed" << m_controlsChanged;
	return (m_controlsChanged!=KMixPrefDlg::ChangedNone);
}


void KMixPrefDlg::showEvent(QShowEvent *event)
{
	// -1- Replace widgets ------------------------------------------------------------
	// Hotplug can change mixers or backends => recreate tab
	updateBackends();

	KConfigDialog::showEvent(event);

	// -2- Change visibility and enable status (of the new widgets) ----------------------

	// As GUI can change, the warning will only been shown on demand
	dynamicControlsRestoreWarning->setVisible(MixerToolBox::dynamicBackendsPresent());

	QString autostartConfigFilename =
		QStandardPaths::locate(QStandardPaths::GenericConfigLocation, "/autostart/kmix_autostart.desktop");
	if (Settings::debugConfig())
	    qCDebug(KMIX_LOG) << "autostartConfigFilename = " << autostartConfigFilename;
	bool autostartFileExists = !autostartConfigFilename.isEmpty();
	allowAutostartWarning->setVisible(!autostartFileExists);
	allowAutostart->setEnabled(autostartFileExists);

	m_controlsChanged = KMixPrefDlg::ChangedNone;
	KConfigDialog::updateButtons();
}


void KMixPrefDlg::updateBackends()
{
	QSignalBlocker block(m_mixerList);		// not while updating
	m_mixerList->clear();				// start with empty list

	QStringList mixerIds;
	QMap<QString, QString> mixerNames;
	QMap<QString, QString> mixerIcons;
	QMap<QString, bool> mixerEnabled;

	const KConfigGroup grp(KSharedConfig::openConfig(), "SystemTray");
	if (grp.exists())
	{
		const QStringList keys = grp.keyList();
		for (const QString &key : qAsConst(keys))
		{
			// The stored value is expected to be "id,enabled,icon[,readable]"
			const QStringList data = grp.readEntry(key, QStringList());

			const QString &id = data.value(0);
			if (id.isEmpty()) continue;
			if (!mixerIds.contains(id)) mixerIds.append(id);

			const QString &isEnabled = data.value(1);
			mixerEnabled[id] = isEnabled.toInt()!=0;

			const QString &iconName = data.value(2);
			if (!iconName.isEmpty()) mixerIcons[id] = iconName;

			const QString &readableName = data.value(3);
			if (!readableName.isEmpty()) mixerNames[id] = readableName;
		}

		qCDebug(KMIX_LOG) << "Loaded" << mixerIds.count() << "mixers from new config";
	}
	else
	{
		// Assuming that there can be no duplicates in this list,
		// because it would have been originally written out from
		// the values of a QSet.
		mixerIds = Settings::mixersForSoundMenu();

		// A mixer is always taken to be enabled if it appears in
		// this list.
		for (const QString &id : qAsConst(mixerIds)) mixerEnabled[id] = true;

		qCDebug(KMIX_LOG) << "Loaded" << mixerIds.count() << "mixers from old config";
	}

	// Add all of the currently active mixers, including their
	// current readable names and icons.
	for (const Mixer *mixer : std::as_const(MixerToolBox::mixers()))
	{
		const QString &id = mixer->id();
		if (!mixerIds.contains(id)) mixerIds.append(id);
		mixerIcons[id] = mixer->iconName();
		mixerNames[id] = mixer->readableName();
	}

	qCDebug(KMIX_LOG) << "Total" << mixerIds.count() << "mixers to display";

	for (const QString &id : qAsConst(mixerIds))
	{
		// TODO: No point in showing mixers which do not have any volume controls.
		// See checks done in ViewDockAreaPopup::initLayout()
		// Implement shared test Mixer::hasVolumeCOntrol()

		QListWidgetItem *item = new QListWidgetItem(m_mixerList);
		item->setSizeHint(QSize(1, 16+2));
		item->setData(Qt::UserRole, id);

		// Only currently active mixers can have their state changed.
		// In theory, inactive mixers could as well (which would not
		// affect the system tray GUI until they were actually plugged
		// in again), but to get the disabled item appearance the flag
		// affects the checkable state also.
		item->setFlags(Qt::ItemIsUserCheckable|Qt::ItemNeverHasChildren);
		const Mixer *mixer = MixerToolBox::findMixer(id);
		if (mixer!=nullptr) item->setFlags(item->flags()|Qt::ItemIsEnabled);

		QString iconName = mixerIcons.value(id);
		if (iconName.isEmpty()) iconName = "speaker";
		item->setIcon(QIcon::fromTheme(iconName));

		QString readableName = mixerNames.value(id);
		if (readableName.isEmpty()) readableName = '('+id+')';
		item->setText(readableName);

		const bool mixerShouldBeShown = mixerEnabled.value(id);
		item->setCheckState(mixerShouldBeShown ? Qt::Checked : Qt::Unchecked);
	}
}
