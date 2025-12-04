if(OV_DISABLE_GTK)
  message(STATUS  "Skipping Plugins: Simple Visualization, no GTK")
  return()
endif(OV_DISABLE_GTK)

project(openvibe-plugins-simple-visualization VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.h src/*.hpp src/*.inl)

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

target_link_libraries(${PROJECT_NAME}
					  openvibe
					  openvibe-common
					  openvibe-toolkit
					  openvibe-module-system
					  openvibe-visualization-toolkit
					  Boost::boost
					  gtk2)

set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
	FOLDER ${PLUGINS_FOLDER}
	COMPILE_FLAGS "-DOVP_Exports -DOVP_Shared")

add_definitions(-DTARGET_HAS_ThirdPartyOpenViBEPluginsGlobalDefines)

include("FindOpenViBEModuleTCPTagging")

file(COPY share/ DESTINATION ${BUILD_DATADIR}/plugins/simple-visualization)
file(COPY box-tutorials DESTINATION ${BUILD_DATADIR}/scenarios/)

# -----------------------------
# Install files
# -----------------------------
install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION ${DIST_BINDIR}
	LIBRARY DESTINATION ${DIST_LIBDIR}
	ARCHIVE DESTINATION ${DIST_LIBDIR})

install(DIRECTORY share/ DESTINATION ${DIST_DATADIR}/openvibe/plugins/simple-visualization)
install(DIRECTORY box-tutorials DESTINATION ${DIST_DATADIR}/openvibe/scenarios/)
