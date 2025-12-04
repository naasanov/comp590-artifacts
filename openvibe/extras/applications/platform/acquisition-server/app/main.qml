import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import QtCore

import OVASInterface as OV
import OVParametersQml as OV
import OVStyle as OV 
import "." as OV

//window containing the application
ApplicationWindow {
  id: window
  visible: true
  minimumWidth: 600
  minimumHeight: 600

  //title of the application
  title: qsTr("Openvibe Acquisition Manager")

  palette {
    window: OV.Style.colors.bgColor
    windowText: OV.Style.colors.textColorNeutral
    base: OV.Style.colors.fgColor
    alternateBase: OV.Style.colors.alternateFgColor
    //toolTipBase: OV.Style.colors.
    //toolTipText: OV.Style.colors.
    placeholderText: OV.Style.colors.textColorFaded
    text: OV.Style.colors.textColorBase
    button: OV.Style.colors.baseColor
    buttonText: OV.Style.colors.textColorBase
    brightText: OV.Style.colors.textColorDarkNeutral

    disabled {

    }

    inactive {

    }
  }

  Settings {
    id: stt
    property alias x: window.x
    property alias y: window.y
    property alias width: window.width
    property alias height: window.height
    property int mode: 1
  }


  OV.OVMaster {
    id: master
  }

  OV.Parameters {
    id: _device_param
    parameters: master.deviceParameters
  }

  OV.Parameters {
    id: _driver_param
    parameters: master.driverParameters
  }

  OV.Settgs {
    id: settgs
  }

  //menu containing two menu items
  menuBar: MenuBar {
    Menu {
      title: qsTr("File")
      MenuItem {
        text: qsTr("&Open")
        onTriggered: console.log("Open action triggered");
      }
      MenuItem {
        text: qsTr("Exit")
        onTriggered: Qt.quit();
      }
    }
  }

  StackLayout { 
    id: _container
    anchors.fill: parent

    currentIndex: 0

    Control {
      ColumnLayout {
        id: mainLayout
        anchors.fill: parent

        RowLayout {
          Layout.alignment: Qt.AlignTop
          Layout.preferredWidth: parent.width
          GridLayout {
            columns: 3
            flow: GridLayout.LeftToRight
            Layout.fillHeight: true
            Layout.fillWidth: true

            Text {
              text: "Driver"
              font: OV.Style.fonts.formLabel
              color: OV.Style.colors.textColorBase
            }
            OV.ComboBox {
              id: cb_drivers
              model: master.drivers

              onCurrentTextChanged: {
                console.log("set new Driver to ", currentText)
                master.setCurrentDriver(currentText)
                _driver_param.updateParametersModel()
              }
            }
            OV.Button {
              text: "Driver Properties"
              iconName: "cogs"
              onClicked: {
                _container.currentIndex = 1
              }
            }

            Text {
              text: "Connection Port"
              font: OV.Style.fonts.formLabel
              color: OV.Style.colors.textColorBase
            }
            OV.ComboBox {
              Layout.columnSpan: 2
              Layout.fillWidth: true

              model: ["1024 ", "Second", "Third"]
            }

            Text {
              text: "Sample count per sent block"
              font: OV.Style.fonts.formLabel
              color: OV.Style.colors.textColorBase
            }
            OV.ComboBox {
              Layout.columnSpan: 2
              Layout.fillWidth: true

              model: ["32", "Second", "Third"]
            }

          }

          ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            OV.Button {
              Layout.alignment: Qt.AlignRight
              text: qsTr("Preferences")
              iconName: "book"
              onClicked: {
                settgs.open()
              }

            }
            OV.Button {
              Layout.alignment: Qt.AlignRight
              text: qsTr("Connect")
              iconName: "connection"
              onClicked:  {
                if(text === "Connect") {
                  text= "disconnect";
                  button_play.enabled = true;
                  button_stop.enabled = true;
                } else {
                  text= "Connect";
                  button_play.enabled = false;
                  button_stop.enabled = false;
                  }
              }
            }
            OV.Button {
              id: button_play
              Layout.alignment: Qt.AlignRight
              text: qsTr("Play")
              iconName: "play"
              enabled: false
            }
            OV.Button {
              id: button_stop
              Layout.alignment: Qt.AlignRight
              text: qsTr("Stop")
              iconName: "stop"
              enabled: false
            }
          }
        } // RowLayout

        Text {
          id: drift_text
          Layout.alignment: Qt.AlignHCenter
          text: "Device drift: 0.00ms"
        }

        RowLayout {
          Layout.fillWidth: true
          ProgressBar {
            width: window.width/2
            value: 0.5
          }

          ProgressBar {
            width: window.width/2
            value: 0.2
          }
        }
      }
    }

    //params menu
    Control {
      ColumnLayout {
        anchors.fill: parent
        anchors.margins: OV.Style.smallPadding;

        Label {
          Layout.alignment: Qt.AlignHCenter
          text: "Device Configuration"
          font: OV.Style.fonts.header
          color: OV.Style.colors.textColorBase
        }

        ListView {
          Layout.alignment: Qt.AlignHCenter
          Layout.fillWidth: true;
          Layout.fillHeight: true;
          spacing: OV.Style.smallColumnSpacing;

          model: _device_param.params_model

          delegate: Loader {
            required property var param;
            required property var component;
            property var lparam: param
            sourceComponent: component
          }
        }

        ListView {
          Layout.alignment: Qt.AlignHCenter
          Layout.fillWidth: true;
          Layout.fillHeight: true;
  
          spacing: OV.Style.smallColumnSpacing;
          clip: true;
  
          model: _device_param.params_model
  
          delegate: Loader {
            required property var param;
            required property var component;
            property var lparam: param
            //sourceComponent: component
          }
        }

        OV.Button {
          Layout.alignment: Qt.AlignHCenter
          text: "Finish"
          onClicked: {
            _container.currentIndex = 0
          }
        }
      }
    }
  }

  Component.onCompleted: {
    OV.Style.mode = stt.mode
    _device_param.updateParametersModel()

  }
}
