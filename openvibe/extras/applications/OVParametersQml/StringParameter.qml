import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Qt.labs.platform  1.0 as P

import OVStyle as OV
import "." as OV

Control {
    id: _control

    required property var param

    implicitHeight: _label.implicitHeight + _value.implicitHeight + 2*OV.Style.smallPadding

    OV.ToolTip {
        visible: _control.hovered
        text: param.description
    }

    RowLayout {
        anchors.fill: parent

        Label {
            id: _label

            text: _control.param.name.toUpperCase()
            font: OV.Style.fonts.label
            color: OV.Style.colors.textColorBase
        }

        OV.TextField {
            id: _value

            Layout.alignment: Qt.AlignRight

            text: _control.param ? _control.param.value : ""
            color: OV.Style.colors.hoveredBaseColor
            font: OV.Style.fonts.value

            onTextChanged: {
                if(_control.param) {
                    _control.param.value = text
                }
            }
        }
    }

}
