# SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>
# SPDX-License-Identifier: LGPL
# From project gnomon: https://gitlab.inria.fr/gnomon/gnomon 

project(OVParametersQml VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

set(CMAKE_AUTOMOC ON)

add_subdirectory(src)

# Qt QML module
qt_add_qml_module(${PROJECT_NAME} STATIC
URI
  ${PROJECT_NAME}
VERSION
  1.0
PLUGIN_TARGET
  ${PROJECT_NAME}
QML_FILES
  BoolParameter.qml
  Button.qml
  ComboBox.qml
  ComboBoxDelegate.qml
  Icon.qml
  Icons.js
  ListParameter.qml
  NumericParameter.qml
  Panel.qml
  Parameters.qml
  Slider.qml
  StringParameter.qml
  TextField.qml
  ToolTip.qml
RESOURCES
  materialdesignicons-webfont.ttf
NO_CACHEGEN)

target_compile_definitions(${PROJECT_NAME} PRIVATE -DTARGET_HAS_ThirdPartyOpenViBEPluginsGlobalDefines)

target_link_libraries(${PROJECT_NAME} 
PUBLIC
  Qt::Gui
  OVParametersObject
  OVParameters
PRIVATE 
  Qt::Quick
)

# Install files
install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION ${DIST_BINDIR}
		LIBRARY DESTINATION ${DIST_LIBDIR}
		ARCHIVE DESTINATION ${DIST_LIBDIR})
