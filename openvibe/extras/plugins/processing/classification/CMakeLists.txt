project(openvibe-plugins-classification VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.h src/*.hpp src/*.inl)
add_library(${PROJECT_NAME} SHARED ${SRC_FILES}
	"../../../contrib/packages/libSVM/svm.cpp"
	"../../../contrib/packages/libSVM/svm.h")

# Find ONNX Runtime from conda environment
if(DEFINED ENV{CONDA_PREFIX})
    set(ONNXRUNTIME_ROOT_DIR "$ENV{CONDA_PREFIX}")
    set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT_DIR}/include")
    set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_ROOT_DIR}/lib")
    
    # Find the library
    if(WIN32)
        find_library(ONNXRUNTIME_LIB onnxruntime PATHS ${ONNXRUNTIME_LIB_DIR} NO_DEFAULT_PATH)
    else()
        find_library(ONNXRUNTIME_LIB onnxruntime PATHS ${ONNXRUNTIME_LIB_DIR} NO_DEFAULT_PATH)
    endif()
    
    if(ONNXRUNTIME_LIB)
        message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIB}")
    else()
        message(FATAL_ERROR "ONNX Runtime not found in conda environment")
    endif()
else()
    message(FATAL_ERROR "CONDA_PREFIX not set. Please activate conda environment.")
endif()

# Add include directory
target_include_directories(${PROJECT_NAME} PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME}
    openvibe
    openvibe-common
    openvibe-toolkit
    Eigen3::Eigen
    ${ONNXRUNTIME_LIB}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
	FOLDER ${PLUGINS_FOLDER}
	COMPILE_FLAGS "-DOVP_Exports -DOVP_Shared")

add_definitions(-DTARGET_HAS_ThirdPartyOpenViBEPluginsGlobalDefines)

# ---------------------------------
# Test applications
# ---------------------------------
if(OV_COMPILE_TESTS)
	add_subdirectory(test)
endif(OV_COMPILE_TESTS)

# -----------------------------
# Install files
# -----------------------------
install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION ${DIST_BINDIR}
	LIBRARY DESTINATION ${DIST_LIBDIR}
	ARCHIVE DESTINATION ${DIST_LIBDIR})
