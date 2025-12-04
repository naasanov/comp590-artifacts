# SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>
# SPDX-License-Identifier: LGPL

project(OVParameters VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

add_definitions(-DTARGET_HAS_ThirdPartyOpenViBEPluginsGlobalDefines)

add_library(${PROJECT_NAME} SHARED
parameterBool.h
parameterBool.cpp
parameterDouble.h
parameterDouble.cpp
parameterInt.h
parameterInt.cpp
parameterListString.h
parameterListString.cpp
parameters.h
parameterString.h
parameterString.cpp
parameterTraits.h
parameterTraits.cpp
)

target_include_directories(${PROJECT_NAME}
                           PUBLIC
                             $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                            $<INSTALL_INTERFACE:include> #TODO check
                          )

# Install files
install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION ${DIST_BINDIR}
		LIBRARY DESTINATION ${DIST_LIBDIR}
		ARCHIVE DESTINATION ${DIST_LIBDIR})
