import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import OVStyle as OV
import "." as OV

Control {
  id: _control

    required property var param;
    readonly property var paramType: _control.param ? _control.param.type : undefined

    property var value: _control.param ? _control.param.value : 0
    property var min: _control.param ? _control.param.min : 0
    property var max: _control.param ? _control.param.max : 0
    property var name: _control.param ? _control.param.name : ""
    property var doc: _control.param ? _control.param.description : ""
    property var decimals: 2

    width: OV.Style.smallPanelWidth
    //implicitHeight: _label.implicitHeight + _slider.implicitHeight + _value_input.implicitHeight
    implicitHeight: _label.implicitHeight + OV.Style.sizes.s6
    hoverEnabled: true
    background: Rectangle{
        color: "transparent"
    }

    OV.ToolTip {
        visible: _control.hovered
        text: _control.doc
    }

    Label {
        id: _label

        anchors.left: parent.left
        anchors.top: parent.top

        text: _control.name.toUpperCase()
        font: OV.Style.fonts.label
        color: OV.Style.colors.textColorBase
    }

    //text input instead ?? 
    RowLayout {
        anchors.top: _label.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        OV.Icon {
            Layout.alignment: Qt.AlignLeft
            Layout.fillHeight: true
            icon: "chevron-left"

            MouseArea {
                anchors.fill: parent
                onClicked: { 
                    if(!_control.param) {
                        return
                    }
                    var delta = 0;
                    if(_control.param.type == "int" ) {
                        delta = -1;
                    } 
                    if(_control.param.type == "double" ) {
                        delta = -(max - min)/100.;
                    } 

                    _control.value += delta
                 }
            }
        }
        OV.TextField {
            id: _text_value
            Layout.fillWidth: true
            Layout.fillHeight: true

            text: _control.value
            validator: getValidator()
        }
        OV.Icon {
            Layout.alignment: Qt.AlignRight
            Layout.fillHeight: true
            icon: "chevron-right"

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if(!_control.param) {
                        return
                    }
                    var delta = 0;
                    if(_control.param.type == "int" ) {
                        delta = 1;
                    } 
                    if(_control.param.type == "double" ) {
                        delta = (max - min)/100.;
                    } 

                    _control.value += delta
                 }
            }
        }
    }


/*
    OV.Slider {
        id: _slider

        anchors.top: _label.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        from: _control.min
        to: _control.max
        value: _control.value

        onValueChanged: {
            if (_slider.value != _control.value)
                _control.value = _slider.value
        }
    }

    OV.TextField {
        id: _value_input;

        implicitHeight: OV.Style.sizes.s6
        implicitWidth: OV.Style.controlWidth

        anchors.top: _slider.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter

        selectByMouse: true
        mouseSelectionMode: TextInput.SelectCharacters
        readOnly: false

        text: _slider.value.toFixed(_control.decimals)

        //validator: DoubleValidator {
        //    bottom: _control.min
        //    top: _control.max
        //    decimals: _control.decimals
        //    notation: DoubleValidator.StandardNotation
        //}

        onEditingFinished: {
            _control.value = parseFloat(text);
        }

        Keys.onReturnPressed: editingFinished()
    }
*/

    IntValidator {
        id: _int_validator
        bottom: _control.min
        top: _control.max
    }

    DoubleValidator {
        id: _double_validator
        bottom: _control.min
        top: _control.max
        decimals: _control.decimals
        notation: DoubleValidator.StandardNotation
    }

    function getValidator() {
        if(_control.param && _control.param.type == "int") {
            return _int_validator
        }
        if(_control.param && _control.param.type == "double") {
            return _double_validator
        }
        return null
    }

    onValueChanged: {
        if(_control.param)
            //_control.param.value = _slider.value
            _control.param.value = Number(_text_value.text)
    }
}
