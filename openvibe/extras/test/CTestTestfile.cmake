# This file is the entry with which the test should be executed. It sets the environement to correct values for the tests.
# After building the project a correctly configured copy of this file should be available in the build folder.
# Executing "ctest -T Test" in the build folder should execute the tests automatically using this file.

##################################################

set(ENV{OV_BINARY_PATH} "@CMAKE_RUNTIME_OUTPUT_DIRECTORY@")
set(OV_CONFIG_SUBDIR @OV_CONFIG_SUBDIR@) # This is used in the dart files
set(CMAKE_COMMAND "@CMAKE_COMMAND@")
if(WIN32)
	set(ENV{OV_USERDATA} "$ENV{APPDATA}/${OV_CONFIG_SUBDIR}/")
else()
	SET(ENV{OV_USERDATA} "$ENV{HOME}/.config/${OV_CONFIG_SUBDIR}/")
endif()
SET(OV_LOGFILE "$ENV{OV_USERDATA}/log/openvibe-designer.log") 

set(CTEST_SOURCE_DIRECTORY "@CMAKE_CURRENT_SOURCE_DIR@")

# this is the folder where test scenarios can be run under
set(ENV{OV_TEST_DEPLOY_PATH} "${CTEST_SOURCE_DIRECTORY}/local-tmp/test-deploy/")

# Regex to catch errors in logs when return code is ok
set (failRegex "\\[ *ERROR * \\]")

##################################################
# Function to factorise basic validation test with an output (Run a scenario and compare output csv with a ref)
# How to use : validation_test(your-folder Your-Test-Name Comparison Extension-Of-Output Use-threshold-flag) folder can be ""
# 0 on COMPARE if you don't check output
# 0 on USE_THRESHOLD to have only a strict equality with git command, 1 to have the threshold comparison programm for csv or xml 
function(validation_test TEST_PATH TEST_NAME COMPARE OUTPUT_EXT USE_THRESHOLD THRESHOLD_VALUE)
	if(TEST_PATH)
		set(TEST_FILE ${TEST_PATH}/${TEST_NAME})
	else()
		set(TEST_FILE ${TEST_NAME})
	endif()

	if(COMPARE)
		add_test(clean_${TEST_NAME}		"${CMAKE_COMMAND}" "-E" "remove" "-f" "${TEST_FILE}-output.${OUTPUT_EXT}")
	endif(COMPARE)

	add_test(run_${TEST_NAME}		"$ENV{OV_BINARY_PATH}/openvibe-designer" "--no-session-management" "--invisible" "--play-fast" "${TEST_FILE}-test.xml")

	set_tests_properties (TEST run_${TEST_NAME} DIRECTORY ${TEST_PATH} PROPERTIES FAIL_REGULAR_EXPRESSION "${failRegex}")

	if(COMPARE)
		if(USE_THRESHOLD)
			add_test(compare_${TEST_NAME}	"$ENV{OV_BINARY_PATH}/threshold-comparison" "${TEST_FILE}-output.${OUTPUT_EXT}" "${TEST_FILE}-ref.${OUTPUT_EXT}" ${THRESHOLD_VALUE})
		else()
			add_test(compare_${TEST_NAME}	"git" "diff" "--no-index" "--ignore-space-change" "${TEST_FILE}-output.${OUTPUT_EXT}" "${TEST_FILE}-ref.${OUTPUT_EXT}")
		endif(USE_THRESHOLD)
	endif(COMPARE)

	set_tests_properties(run_${TEST_NAME} PROPERTIES ATTACHED_FILES_ON_FAIL ${OV_CONFIG_SUBDIR})

	if(COMPARE)
		set_tests_properties(compare_${TEST_NAME} PROPERTIES ATTACHED_FILES_ON_FAIL "${TEST_FILE}-output.${OUTPUT_EXT}")
		set_tests_properties(compare_${TEST_NAME} PROPERTIES DEPENDS run_${TEST_NAME})
		set_tests_properties(run_${TEST_NAME} PROPERTIES DEPENDS clean_${TEST_NAME})
	endif(COMPARE)

endfunction()

##################################################
# Function to factorise basic validation test with an output (Run a scenario and compare output with a ref for any file)
# How to use : validation_test_with_git(your-folder Your-Test-Name Extension-Of-Output) folder can be ""
# Test with git command for compare. Useful for not implemented threshold comparison and any binary files
function(validation_test_with_git TEST_PATH TEST_NAME OUTPUT_EXT)
	if(TEST_PATH)
		validation_test(${TEST_PATH} ${TEST_NAME} TRUE ${OUTPUT_EXT} FALSE 0.0001)
	else()
		validation_test("" ${TEST_NAME} TRUE ${OUTPUT_EXT} FALSE 0.0001)
	endif()
endfunction()

##################################################
# Function to factorise basic validation test with a csv output (Run a scenario and compare output csv with a ref)
# How to use : validation_test_with_csv(your-folder Your-Test-Name) folder can be ""
function(validation_test_with_csv TEST_PATH TEST_NAME)
	if(TEST_PATH)
		validation_test(${TEST_PATH} ${TEST_NAME} TRUE csv TRUE 0.0001)
	else()
		validation_test("" ${TEST_NAME} TRUE csv TRUE 0.0001)
	endif()
endfunction()

##################################################
# Function to factorise basic validation test with a xml output (Run a scenario and compare output xml with a ref.)
# How to use : validation_test_with_xml(your-folder Your-Test-Name) folder can be ""
function(validation_test_with_xml TEST_PATH TEST_NAME)
	if(TEST_PATH)
		validation_test(${TEST_PATH} ${TEST_NAME} TRUE xml TRUE 0.0001)
	else()
		validation_test("" ${TEST_NAME} TRUE xml TRUE 0.0001)
	endif()
endfunction()

##################################################
# Function to factorise basic validation test without output
# How to use : validation_test_without_comparison(your-folder Your-Test-Name) folder can be ""
function(validation_test_without_comparison TEST_PATH TEST_NAME)
	if(TEST_PATH)
		validation_test(${TEST_PATH} ${TEST_NAME} FALSE FALSE FALSE 0.0001)
	else()
		validation_test("" ${TEST_NAME} FALSE FALSE FALSE 0.0001)
	endif()	
endfunction()

##################################################
# Function to factorise basic validation test for visualization. There is no output generally,  it's just a run test in normal mode (not fast). 
# Crash is only detected and cause an error during tests. Invisible tag must be still here because openvibe can't be closed automaticly without that
# Only manual run with human validation can be optimal. 
# How to use : validation_test_visualization(your-folder Your-Test-Name) folder can be ""
function(validation_test_visualization TEST_PATH TEST_NAME)
	if(TEST_PATH)
		set(TEST_FILE ${TEST_PATH}/${TEST_NAME})
	else()
		set(TEST_FILE ${TEST_NAME})
	endif()

	add_test(run_${TEST_NAME} "$ENV{OV_BINARY_PATH}/openvibe-designer" "--no-session-management" "--invisible" "--play" "${TEST_FILE}-test.xml")
	set_tests_properties(run_${TEST_NAME} PROPERTIES FAIL_REGULAR_EXPRESSION "${failRegex}")
	set_tests_properties(run_${TEST_NAME} PROPERTIES ATTACHED_FILES_ON_FAIL ${OV_CONFIG_SUBDIR})
endfunction()

##################################################
# subdirs command is deprecated and should be replaced by add_subdirectory calls as per the documentation recommendations, 
# however the 2 command do not have the same behavior with ctest. Doing the change currently breaks tests.
subdirs("${CTEST_SOURCE_DIRECTORY}/contrib/plugins/server-extensions/tcp-tagging/test")
#subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/acquisition/test")					# No tests here
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/artifact/test")
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/classification/test")
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/data-generation/test")
#subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/dll-bridge/test")					# No tests here
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/evaluation/test")
#subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/examples/test")						# No tests here
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/features-selection/test")
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/file-io/test")
#subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/matlab/test")						# No tests here
#subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/network-io/test")					# No tests here
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/riemannian/test")
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/signal-processing/test")
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/simple-visualization/test")
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/stimulation/test")
subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/streaming/test")
#subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/tests/test")							# No tests here
#subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/tools/test")							# No tests here
#subdirs("${CTEST_SOURCE_DIRECTORY}/plugins/processing/vrpn/test")							# No tests here
#subdirs("${CTEST_SOURCE_DIRECTORY}/applications/platform/acquisition-server/test")			# No tests here
#subdirs("${CTEST_SOURCE_DIRECTORY}/applications/developer-tools/plugin-inspector/test")	# No tests here
#subdirs("${CTEST_SOURCE_DIRECTORY}/applications/developer-tools/skeleton-generator/test")	# No tests here
#subdirs("${CTEST_SOURCE_DIRECTORY}/applications/developer-tools/vrpn-simulator/test")		# No tests here
