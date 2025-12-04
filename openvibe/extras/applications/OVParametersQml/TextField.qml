import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import OVStyle as OV
import "." as OV

TextField {
    id: _control

    property double backgroundBorderHeight: 2
    property color backgroundColor: OV.Style.colors.bgColor
    property color backgroundHighlightColor: OV.Style.colors.baseColor

    cursorDelegate: Rectangle
    {
        color: OV.Style.colors.baseColor;
        width: 1.5
        visible: parent.activeFocus && !parent.readOnly && parent.selectionStart === parent.selectionEnd
    }

    background: Rectangle
    {
        y: _control.height - height + 4
        width: parent.width
        height: _control.activeFocus || _control.hovered ? _control.backgroundBorderHeight : 1
        color: _control.backgroundColor
        radius: OV.Style.panelRadius

        Rectangle
        {
            height: _control.backgroundBorderHeight
            color: _control.backgroundHighlightColor
            width: _control.activeFocus ? parent.width : 0
            x: _control.activeFocus ? 0 : parent.width / 2

            Behavior on width
            {
                enabled: !_control.activeFocus
                NumberAnimation
                {
                    easing.type: Easing.OutCubic;
                    duration: 300
                }
            }

            Behavior on x
            {
                enabled: !_control.activeFocus
                NumberAnimation
                {
                    easing.type: Easing.OutCubic;
                    duration: 300
                }
            }

        }
    }
}
