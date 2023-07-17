include(CMakeParseArguments)
#
# Creates launch script from a common OpenViBE template (in "cmake-modules/launchers/"), but dedicated to scenarios to be executed with the Designer
#
# The mandatory 1st argument SCRIPT_PREFIX specifies what the resulting script is called. A platform specific postfix will be added.
# The mandatory 2nd argument EXECUTABLE_NAME specifies what the resulting script will called eventually.
# The optional 3nd argument ARGV1 specifies some extra argument or switch that is given to the launched executable by the script
#
function(OV_INSTALL_LAUNCH_SCRIPT)
	set(options PAUSE CONFIG_RC)
	set(oneValueArgs SCRIPT_PREFIX EXECUTABLE_NAME ICON_PATH)
	set(multiValueArgs PARAMETERS)

	cmake_parse_arguments(OV_INSTALL_LAUNCH_SCRIPT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
	if (OV_INSTALL_LAUNCH_SCRIPT_CONFIG_RC)
		OV_CONFIGURE_RC(NAME ${OV_INSTALL_LAUNCH_SCRIPT_SCRIPT_PREFIX} ICON_PATH ${OV_INSTALL_LAUNCH_SCRIPT_ICON_PATH})
	endif()

	# Install executable launcher if install_exe option is set to on, the os is WIN32, and no argument has been specified
	if(WIN32 AND INSTALL_EXE AND NOT(OV_INSTALL_LAUNCH_SCRIPT_PAUSE) AND NOT(OV_INSTALL_LAUNCH_SCRIPT_PARAMETERS))
		# Add the dir to be parsed for documentation later. We need to do this before adding subdir, in case the subdir is the actual docs dir
		get_property(OV_TMP GLOBAL PROPERTY OV_EXE_PROJECTS_TO_INSTALL)
		set(OV_TMP "${OV_TMP};${OV_INSTALL_LAUNCH_SCRIPT_SCRIPT_PREFIX}")
		set_property(GLOBAL PROPERTY OV_EXE_PROJECTS_TO_INSTALL ${OV_TMP})
	else()
		if(WIN32)
			set(SCRIPT_POSTFIX ".cmd")
		elseif(APPLE)
			set(SCRIPT_POSTFIX ".sh")
		elseif(UNIX)
			# Debian recommends that extensions such as .sh are not used; On Linux, scripts with such extensions shouldn't be packaged
			set(SCRIPT_POSTFIX ".sh")
		endif()

		get_target_property(TMP_PROJECT_TARGET_PATH ${OV_INSTALL_LAUNCH_SCRIPT_EXECUTABLE_NAME} LOCATION)
		get_filename_component(OV_CMD_EXECUTABLE ${TMP_PROJECT_TARGET_PATH} NAME)

		if(${OV_CMD_EXECUTABLE} STREQUAL "TMP_PROJECT_TARGET_PATH-NOTFOUND")
			set(OV_CMD_EXECUTABLE ${OV_INSTALL_LAUNCH_SCRIPT_EXECUTABLE_NAME})
		endif()
		
		set(SCRIPT_NAME ${OV_INSTALL_LAUNCH_SCRIPT_SCRIPT_PREFIX}${SCRIPT_POSTFIX})
		set(OV_CMD_ARGS ${OV_INSTALL_LAUNCH_SCRIPT_PARAMETERS})
		
		if(OV_INSTALL_LAUNCH_SCRIPT_PAUSE)
			set(OV_PAUSE "PAUSE")
		else()
			set(OV_PAUSE "")
		endif()
		
		configure_file(${OV_LAUNCHER_SOURCE_PATH}/openvibe-launcher${SCRIPT_POSTFIX}-base ${CMAKE_CURRENT_BINARY_DIR}/${SCRIPT_NAME} @ONLY)
		install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${SCRIPT_NAME} DESTINATION ${DIST_ROOT})
	endif()
endfunction()

function(ov_configure_rc)
	set(options )
	set(oneValueArgs NAME ICON_PATH)
	SET(multiValueArgs )
	cmake_parse_arguments(OV_CONFIGURE_RC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
	SET(GENERATED_RC_FILE "${CMAKE_BINARY_DIR}/resource-files/${OV_CONFIGURE_RC_NAME}.rc")
	if(OV_CONFIGURE_RC_ICON_PATH)
		set(CONFIGURE_ICON "ID_Icon ICON DISCARDABLE \"${OV_CONFIGURE_RC_ICON_PATH}\"")
	endif()
	if(NOT(PROJECT_PRODUCT_NAME))
		set(PROJECT_PRODUCT_NAME "${OV_CONFIGURE_RC_NAME}")
	endif()
	set(FILE_DESCRIPTION "${PROJECT_PRODUCT_NAME} for Win32")
	
	configure_file(
		${OV_LAUNCHER_SOURCE_PATH}/resource-file.rc-base 
		${GENERATED_RC_FILE}
		@ONLY)
endfunction()
