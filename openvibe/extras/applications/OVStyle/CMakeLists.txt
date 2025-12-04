# SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>
# SPDX-FileCopyrightText: Inria <romain.tetley@inria.fr>
# SPDX-License-Identifier: LGPL
# From project gnomon: https://gitlab.inria.fr/gnomon/gnomon 

project(OVStyle VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

set(CMAKE_AUTOMOC ON)

set_source_files_properties(Style.qml PROPERTIES
  QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(${PROJECT_NAME} STATIC
URI
${PROJECT_NAME}
VERSION
  1.0
PLUGIN_TARGET
${PROJECT_NAME}
QML_FILES
  Style.qml
  style.qml.js
RESOURCES
  Poppins-Black.ttf
  Poppins-BlackItalic.ttf
  Poppins-Bold.ttf
  Poppins-BoldItalic.ttf
  Poppins-ExtraBold.ttf
  Poppins-ExtraBoldItalic.ttf
  Poppins-ExtraLight.ttf
  Poppins-ExtraLightItalic.ttf
  Poppins-Italic.ttf
  Poppins-Light.ttf
  Poppins-LightItalic.ttf
  Poppins-Medium.ttf
  Poppins-MediumItalic.ttf
  Poppins-Regular.ttf
  Poppins-SemiBold.ttf
  Poppins-SemiBoldItalic.ttf
  Poppins-Thin.ttf
  Poppins-ThinItalic.ttf
OUTPUT_DIRECTORY
  ${CMAKE_BINARY_DIR}/qml/OVStyle
OUTPUT_TARGETS
${PROJECT_NAME}Targets
NO_CACHEGEN)

add_library(OV::Style ALIAS ${PROJECT_NAME})

set(${PROJECT_NAME}Targets ${${PROJECT_NAME}Targets} ${PROJECT_NAME})

install(TARGETS ${${PROJECT_NAME}Targets} EXPORT ${PROJECT_NAME}-targets
  OBJECTS DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(EXPORT ${PROJECT_NAME}-targets
FILE
${PROJECT_NAME}Targets.cmake
DESTINATION
  ${CMAKE_INSTALL_LIBDIR}/cmake/OVStyle)

export(EXPORT ${PROJECT_NAME}-targets
FILE
  ${CMAKE_BINARY_DIR}/${PROJECT_NAME}Targets.cmake)
