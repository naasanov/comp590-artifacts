import QtQuick
import QtQuick.Controls

import OVStyle as OV
import "." as OV

Control {
    id: _control

    required property var param
    implicitHeight: _label.implicitHeight + _switch.implicitHeight

    OV.ToolTip {
        visible: _control.hovered && !_switch.pressed
        text: _control.param ? _control.param.description : ""
    }

    Label {
        id: _label

        anchors.left: _control.left
        anchors.top: _control.top

        text: _control.param ? _control.param.name.toUpperCase() : ""
        font: OV.Style.fonts.label
        color: OV.Style.colors.textColorBase

    }

    Label {
        id: _value

        anchors.left: _control.left
        anchors.top: _label.bottom
        width: 32

        text: _switch.checked ? "TRUE" : "FALSE"
        font: OV.Style.fonts.value
        color: OV.Style.colors.hoveredBaseColor
    }

    Switch {
        id: _switch

        anchors.top: _label.bottom
        anchors.left: _value.right
        anchors.leftMargin: 16

        onToggled: {
            if (_control.param)
                _control.param.value = _switch.checked ? 1 : 0
        }
    }

    Component.onCompleted: {
        if (_control.param)
            _switch.checked = _control.param.value
    }
}
