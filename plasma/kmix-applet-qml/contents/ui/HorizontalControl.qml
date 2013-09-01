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

Row {
    id: _control
    property bool muted: false
    property bool isMasterControl: false
    property int volume: 0
    property alias dataSource: _volumeEngine.connectedSources
    spacing: 5

    PlasmaComponents.ToolButton {
        id: _volumeMuteButton
        height: theme.mediumIconSize
        width: theme.mediumIconSize
        checked: _control.muted
        onClicked: {
            //toggle mute/unmute state
            var service = _volumeEngine.serviceForSource( _control.dataSource )
            var op = service.operationDescription( "setMute" )
            op["muted"] = !_control.muted
            service.startOperationCall( op )
        }
    }

    PlasmaComponents.Slider {
        id: _volumeSlider
        minimumValue: 0
        maximumValue: 100
        stepSize: 1
        orientation: QtHorizontal
        enabled: !_control.muted
        value: _control.volume
        onValueChanged: {
            // set volume level
            var service = _volumeEngine.serviceForSource( _control.dataSource )
            var op = service.operationDescription( "setVolume" );
            op["level"] = value;
            service.startOperationCall( op )
        }
    }

    PlasmaComponents.Label {
        id: _volumeIndicator
        width: theme.largeIconSize
        height: theme.largeIconSize
        horizontalAlignment: Text.AlignRight
        text: i18n( "%1%", _control.volume )
        elide: Text.ElideRight
        font.weight: Font.Bold
    }

    PlasmaCore.ToolTip {
        id: _tooltip
        target: _volumeSlider
    }

    // needed to have a plasma-like popupIcon
    PlasmaCore.Svg{
        id: _iconSvg
        imagePath: "icons/audio"
    }

    PlasmaCore.DataSource {
        id: _volumeEngine
        engine: "mixer"
        interval: 0
        onDataChanged: {
            _control.muted = data[connectedSources]["Mute"]
            _volumeMuteButton.checkable = data[connectedSources]["Can Be Muted"]
            _volumeMuteButton.checked = _control.muted
            _volumeSlider.enabled = !_control.muted
            _control.volume = data[connectedSources]["Volume"]
            _volumeSlider.value = _control.volume

            _controlIcon.icon = data[connectedSources]["Icon"]
            _controlText.text = data[connectedSources]["Readable Name"]

            _tooltip.mainText = i18n( ( _control.muted ? "Volume at %1% (Muted)" : "Volume at %1%" ) ,  _control.volume)
            _tooltip.subText = _controlText.text

            var icon = _control.retrieveIcon()
            _volumeMuteButton.iconSource = icon
            _tooltip.image = QIcon( _iconSvg.pixmap( icon ) )

            if ( _control.isMasterControl ) {
                // setup tooltip data
                var obj = new Object
                obj["image"] = QIcon( _iconSvg.pixmap( icon ) )
                obj["mainText"] = _tooltip.mainText
                obj["subText"] = _tooltip.subText

                // set tooltip information
                plasmoid.popupIconToolTip = obj
                // set popup icon too
                plasmoid.popupIcon = QIcon( _iconSvg.pixmap( icon ) )
            }
        }
    }

    function retrieveIcon() {
        var icon
        if( _control.muted ) {
            icon = "audio-volume-muted"
        } else if(_control.volume >= 0 && _control.volume <= 25) {
            icon = "audio-volume-low"
        } else if(_control.volume > 25 && _control.volume <= 75) {
            icon = "audio-volume-medium"
        } else {
            icon = "audio-volume-high"
        }
        return icon
    }

    Component.onCompleted: {
        retrieveIcon()
        _volumeIndicator.anchors.right = _control.right
        _volumeIndicator.anchors.verticalCenter = _control.verticalCenter
        _volumeSlider.anchors.verticalCenter = _volumeIndicator.verticalCenter
        _volumeMuteButton.anchors.verticalCenter = _volumeIndicator.verticalCenter
    }
}