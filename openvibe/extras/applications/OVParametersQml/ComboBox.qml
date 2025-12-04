import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import OVStyle as OV
import "." as OV

ComboBox {

  id: _control;

  property color textColor: OV.Style.colors.textColorBase;

  implicitWidth: OV.Style.smallPanelWidth;
  implicitHeight: OV.Style.comboBoxHeight;
  padding: 8;

  background: OV.Panel {
    //anchors.fill: parent;
  }

  delegate: OV.ComboBoxDelegate {
      width: _control.width;
      text: model[textRole] ? model[textRole] : modelData[textRole] ? modelData[textRole] : modelData
      ToolTip.text: ""
      ToolTip.delay: 1000
      ToolTip.timeout: 5000
      ToolTip.visible: ToolTip.text ? hovered : false
      highlighted: _control.highlightedIndex === index

      Component.onCompleted: {
          if (typeof modelData !== "undefined") {
              ToolTip.text = modelData.doc ? modelData.key + "\n" + modelData.doc : null
          }
      }
  }

  indicator: OV.Icon {

    id: _indicator;

    anchors.right: parent.right;
    anchors.verticalCenter: parent.verticalCenter;
    anchors.rightMargin: 8;

    size: OV.Style.iconSmall;
    icon: "apple-keyboard-control"

    rotation: _control.down ? 0 : 180

    Behavior on rotation {
      NumberAnimation { duration: 200 }
    }
  }

  contentItem: Label {

    anchors.verticalCenter: parent.verticalCenter;
    anchors.left: parent.left;
    anchors.leftMargin: 8;
    width: _control.implicitWidth - _indicator.width;

    font: OV.Style.fonts.formLabel;
    color: _control.textColor;
    text: _control.displayText;
    verticalAlignment: Text.AlignVCenter;
    elide: Text.ElideRight

  }

  //TODO: MAKE A BETTER LOOKING POPUP
  popup: Popup {
    y: _control.height - 1
    width: _control.width
    implicitHeight: contentItem.implicitHeight
    padding: 1

    contentItem: ListView {

      clip: true
      spacing: 2;
      implicitHeight: contentHeight
      model: _control.popup.visible ? _control.delegateModel : null
      currentIndex: _control.highlightedIndex

      ScrollIndicator.vertical: ScrollIndicator { }
    }

    background: Rectangle {
      color: OV.Style.colors.gutterColor;
      radius: OV.Style.panelRadius;
    }
  }

}
