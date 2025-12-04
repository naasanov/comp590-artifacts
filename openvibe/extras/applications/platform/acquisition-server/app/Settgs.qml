import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QtCore

import OVStyle as OV

Dialog {
    id: settingsDialog

    x: Math.round((parent.width - width) / 2)
    y: Math.round(parent.height / 6)

    width: OV.Style.mediumDialogWidth;
    height: OV.Style.largeDialogHeight;

    modal: true
    focus: true
    title: "Settings"

    standardButtons: Dialog.Ok | Dialog.Cancel

    leftMargin: 0;
    rightMargin: 0;

    leftPadding: 0;
    rightPadding: 0;
    bottomPadding: 0;
    topPadding: 0;

    spacing: 0;

    ColumnLayout {
        Layout.fillWidth: true;
        Layout.fillHeight: true;
        spacing: 0

        Label {
            width: parent.width;
            color: OV.Style.colors.textColorBase
            font: OV.Style.fonts.subHeader
            text: "Variant";
        }

        SwitchDelegate {
            width: parent.width;
            text: OV.Style.mode == OV.Style.Mode.Dark ? "Dark" : "Light";
            palette.text: OV.Style.colors.textColorBase
            onToggled: {
                OV.Style.mode = checked ? OV.Style.Mode.Light :  OV.Style.Mode.Dark;
            }
            checked: OV.Style.mode == OV.Style.Mode.Light;
        }
    }

    Settings {
        id: settings
        property int mode: OV.Style.Mode.Light
    }

    onAccepted: {
        settings.mode = OV.Style.mode
        settingsDialog.close()
    }

    onRejected: {
        OV.Style.mode = settings.mode
        settingsDialog.close()
    }


}