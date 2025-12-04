if(OV_DISABLE_GTK)
	message(STATUS  "Skipping Plugin Inspector, no GTK")
	return()
endif(OV_DISABLE_GTK)

project(openvibe-plugin-inspector VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.h src/*.hpp src/*.inl)
add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(${PROJECT_NAME}
					  openvibe
					  openvibe-common
					  openvibe-toolkit
					  openvibe-module-system
					  gtk2
)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER ${APP_FOLDER})

add_definitions(-DTARGET_HAS_ThirdPartyOpenViBEPluginsGlobalDefines)

file(COPY share/ DESTINATION ${BUILD_DATADIR}/applications/plugin-inspector)

install(CODE "execute_process( \
		COMMAND ${CMAKE_COMMAND} -E create_symlink \
		${DIST_BINDIR}/$<TARGET_FILE_NAME:${PROJECT_NAME}> \
		${DIST_ROOT}/$<TARGET_FILE_NAME:${PROJECT_NAME}>   \
		)"
)

# -----------------------------
# Install files
# -----------------------------
install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION ${DIST_BINDIR}
	LIBRARY DESTINATION ${DIST_LIBDIR}
	ARCHIVE DESTINATION ${DIST_LIBDIR})

install(DIRECTORY share/ DESTINATION ${DIST_DATADIR}/openvibe/applications/plugin-inspector)
