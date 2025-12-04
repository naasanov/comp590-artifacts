
#
# Disabled until it behaves gracefully on a system without Python dll
#
return()

project(openvibe-py2-check VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.h src/*.hpp src/*.inl)
add_executable(${PROJECT_NAME} ${SRC_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER ${APP_FOLDER})
include("FindThirdPartyPython2")

# ---------------------------------




# ---------------------------------

if(WIN32)
	add_definitions("-DWIN32")
endif()

# -----------------------------
# Install files
# -----------------------------
install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION ${DIST_BINDIR}
	LIBRARY DESTINATION ${DIST_LIBDIR}
	ARCHIVE DESTINATION ${DIST_LIBDIR})
