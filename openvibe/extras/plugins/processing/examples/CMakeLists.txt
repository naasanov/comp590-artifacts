project(openvibe-plugins-examples VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.h src/*.hpp src/*.inl)
add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

target_link_libraries(${PROJECT_NAME}
					  openvibe
					  openvibe-common
					  openvibe-toolkit
					  openvibe-visualization-toolkit
					  gtk2
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
	FOLDER ${PLUGINS_FOLDER}
	COMPILE_FLAGS "-DOVP_Exports -DOVP_Shared")


add_definitions(-DTARGET_HAS_ThirdPartyOpenViBEPluginsGlobalDefines)

file(COPY box-tutorials DESTINATION ${BUILD_DATADIR}/scenarios/)
file(COPY metaboxes DESTINATION ${BUILD_DATADIR}/)

# -----------------------------
# Install files
# -----------------------------
install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION ${DIST_BINDIR}
	LIBRARY DESTINATION ${DIST_LIBDIR}
	ARCHIVE DESTINATION ${DIST_LIBDIR})

file(COPY box-tutorials DESTINATION ${BUILD_DATADIR}/scenarios/)
file(COPY metaboxes DESTINATION ${BUILD_DATADIR}/)

install(DIRECTORY box-tutorials  DESTINATION ${DIST_DATADIR}/openvibe/scenarios/)
install(DIRECTORY metaboxes  DESTINATION ${DIST_DATADIR}/openvibe/)


if(BUILD_DOCUMENTATION)
	file(COPY "metaboxes/doc/Doc_BoxAlgorithm_AsymmetryIndex_DesignerLayout.png"
		DESTINATION ${CMAKE_SOURCE_DIR}/build/documentation/html)
endif()
