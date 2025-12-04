import QtQuick              2.15
import QtQuick.Controls     2.15
import QtQuick.Layouts      1.15

import OVStyle as OV
import "." as OV

ItemDelegate {

  id: _control;
  property int textStyle: Text.Normal;

  implicitHeight: OV.Style.comboBoxHeight;

  font: OV.Style.fonts.formLabel;

  background: Rectangle {
    opacity: enabled ? 0.8 : 0.1
    color: getBgColor()
    border.width: 0.5
    border.color: getEmbossColor()
  }

  contentItem: Label {
    anchors.leftMargin: 8;

    font: OV.Style.fonts.formLabel;
    color: style === Text.Outline ? OV.Style.colors.textColorLightNeutral : OV.Style.colors.textColorNeutral;
    text: _control.text;
    style: _control.textStyle;
    styleColor: OV.Style.colors.textColorDarkNeutral;
    verticalAlignment: Text.AlignVCenter;
    elide: Text.ElideRight
  }

  function getEmbossColor() {
    if(down || highlighted) return OV.Style.colors.embossColorBlue;
    return OV.Style.colors.embossColorNeutral;
  }

  function getBgColor() {
    if(down || highlighted) return OV.Style.colors.baseColor;
    if(hovered) return OV.Style.colors.neutralColor;
    return OV.Style.colors.fgColor;
  }

}
