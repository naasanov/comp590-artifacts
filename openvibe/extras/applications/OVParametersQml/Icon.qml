import QtQuick
import QtQuick.Controls

import OVStyle as OV

import "Icons.js" as I

Control
{
    id: _control;

    required property string icon;
    property int size: OV.Style.iconMedium;
    property color color: OV.Style.colors.textColorBase;
    property alias rotation: _text.rotation;
    property bool flip: false;

    readonly property real _implicitSize: I.icons[_control.icon].toString() ? size : 0

    implicitWidth: _implicitSize
    implicitHeight: _implicitSize

    FontLoader {
        id: _loader;
        source: "materialdesignicons-webfont.ttf";
    }

    Text {
        id: _text
        anchors.fill: _control;

        color: _control.color;
        text: I.icons[_control.icon];
        font.pixelSize: _control.size;
        font.family: _loader.name;
        verticalAlignment: Text.AlignVCenter;
        horizontalAlignment: Text.AlignHCenter;
        transform: Scale {
            origin.x: _text.x + _text.width/2;
            xScale: _control.flip ? -1 : 1;
        }

    }
}
