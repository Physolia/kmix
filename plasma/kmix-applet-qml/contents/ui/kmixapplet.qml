// -*- coding: iso-8859-1 -*-
/*
 *   Author: Diego [Po]lentino Casella <polentino911@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick 1.1
import org.kde.plasma.components 0.1 as PlasmaComponents
import org.kde.plasma.core 0.1 as PlasmaCore
import org.kde.qtextracomponents 0.1

Item {
    id: _kmixApplet
    // we will update those two after all the components are setup properly
    property int minimumWidth: 300
    property int minimumHeight: 300
    // properties from kcfg
    property bool showAllMixers: false
    property bool showMasterControl: false
    property bool horizontalSlider: false
    property bool verticalSlider: false
    //
    property variant _currentMasterControl: undefined

    Column {
        id: _container
        spacing: 5
        anchors {
            fill: _kmixApplet
        }

        MixersList {
            id: _mixersList
            orientation: _kmixApplet.horizontalSlider ? QtVertical : QtHorizontal
        }

        ButtonBar {
            id: _buttonBar
        }
    }

    PlasmaCore.DataSource {
        id: _engine
        engine: "mixer"
        interval: 0
        onDataChanged: {
            // if the mixer is not running, disable the applet until it comes usable again
            if ( data["Mixers"]["Running"] == true ) {
                for( var i = 0; i < data["Mixers"]["Mixers"].length; ++i ) {
                    _controlSource.connectSource( data["Mixers"]["Mixers"][i] )
                }

                // save the current master control ID, so we can use it later to setup the
                // pupup icon when the data changes
                _currentMasterControl = data["Mixers"]["Current Master Control"]
            }

            // load mixers now
            reloadMixers()
        }

        Component.onCompleted: _engine.connectSource("Mixers")
    }

    PlasmaCore.DataSource {
        id: _controlSource
        engine: "mixer"
        interval: 0
        onDataChanged: reloadMixers()
    }

    Component.onCompleted: {
        plasmoid.aspectRatioMode = IgnoreAspectRatio
        plasmoid.setAction( "kmixSetup", i18n( "KMix setup" ), "kmix" )
        plasmoid.setAction( "phononSetup", i18n( "Phonon setup" ), "preferences-desktop-sound" )
        plasmoid.addEventListener( 'ConfigChanged', reloadConfiguration );
    }

    // Functions definition from here

    function reloadConfiguration() {
        showAllMixers = plasmoid.readConfig( "showAllMixers" )
        showMasterControl = plasmoid.readConfig( "showMasterControl" )
        horizontalSlider= plasmoid.readConfig( "horizontalSlider" )
        verticalSlider= plasmoid.readConfig( "verticalSlider" )
        reloadMixers()
    }

    function action_kmixSetup() {
        _buttonBar.execute( "kmix" )
    }

    function action_phononSetup() {
        _buttonBar.execute( "kcmshell4 phonon" )
    }

    function reloadMixers() {
        // TODO: il could be better to create a temporary model first with all the updated mixers,
        // then pass the new model: this will fix the flickering that sometimes shows up
        _mixersList.model.clear()
        // to type less :P
        var data = _controlSource.data
        var connectedSources = _controlSource.connectedSources

        for(var dsIndex = 0; dsIndex<connectedSources.length; ++dsIndex) {
            for( var cIndex = 0; cIndex<data[connectedSources[dsIndex]]["Controls"].length; ++cIndex ) {
                var isMasterControl = ( data[connectedSources[dsIndex]]["Controls"][cIndex] == _currentMasterControl )
                if( isMasterControl || _kmixApplet.showAllMixers ) {
                    _mixersList.model.append( {"_source":connectedSources[dsIndex] + "/" + data[connectedSources[dsIndex]]["Controls"][cIndex], "_isMasterControl": isMasterControl} )
                }
            }
        }
    }
}