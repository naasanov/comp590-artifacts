import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import OVStyle as OV
import "." as OV

Control {
	id: _control

	required property var param;
    //for crossParameters
    readonly property var paramType: _control.param ? _control.param.type : undefined

    implicitHeight: OV.Style.comboBoxHeight;
    implicitWidth: OV.Style.smallPanelWidth;

    background: Rectangle{
        color: "transparent"
    }

    hoverEnabled: true

    OV.ToolTip {
        visible: _control.hovered && !_combobox.down
        text: param ? param.description : ""
    }


    RowLayout {
        anchors.fill: _control;
        spacing: OV.Style.largeRowSpacing

        Label {
            id: _label;
            Layout.preferredWidth: _label.contentWidth;
            verticalAlignment: Text.AlignVCenter;

            text: _control.param ? _control.param.name.toUpperCase() : ""
            font: OV.Style.fonts.label
            color: OV.Style.colors.textColorBase
        }

        OV.ComboBox {
          id: _combobox;
          Layout.fillWidth: true;
          model: _control.param ? _control.param.available_values : []
          currentIndex: _control.param ? param.index : -1
          onCurrentIndexChanged: {
			if(param)
				param.index = _combobox.currentIndex
          }
        }
  }
}
