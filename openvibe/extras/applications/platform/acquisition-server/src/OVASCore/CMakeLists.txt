project(OVASCore VERSION ${OPENVIBE_MAJOR_VERSION}.${OPENVIBE_MINOR_VERSION}.${OPENVIBE_PATCH_VERSION})

set(SRC
  ovasCAcquisitionServer.h
  ovasCAcquisitionServer.cpp
  ovasCSettingsHelper.h
  ovasCSettingsHelper.cpp
  ovasCHeader.h
  ovasCHeader.cpp
  ovasCDriftCorrection.h
  ovasCDriftCorrection.cpp
)

add_definitions(-DTARGET_HAS_ThirdPartyOpenViBEPluginsGlobalDefines)


add_library(${PROJECT_NAME} SHARED
  ${SRC}
)

target_include_directories(${PROJECT_NAME}
  PUBLIC include .)

target_link_libraries(${PROJECT_NAME}
  openvibe
  openvibe-common
  openvibe-toolkit
  openvibe-module-socket
  openvibe-module-system
  openvibe-module-fs
  openvibe-module-lsl
  OVParameters
  Boost::boost
  Boost::system
  Eigen3::Eigen
  LSL::lsl
  # drivers
  brainproducts-amplifier-sdk
)

if(WIN32)
    # @FIXME CERT getting timeBeginPeriod() linker issues without this
    target_link_libraries(${PROJECT_NAME} winmm)
endif(WIN32)


# These scripts will also add directives to copy the necessary bin dlls if any
# include("FindThirdPartyBioSemiLabViewAPI")
# include("FindThirdPartyEmotivAPI")
# include("FindThirdPartyNeXus")
# include("FindThirdPartyMCS")
# include("FindThirdPartyMensiaAcquisition")
# include("FindThirdPartyNeuroelectricsEnobio3G")
# include("FindThirdPartyNeuroServoDeps")
# include("FindThirdPartyThinkGearAPI")
# include("FindThirdPartyUSBFirstAmpAPI")
#
# include("FindThirdPartyPThread")	# for Windows Fieldtrip
# include("FindThirdPartyRT")  # external stims shared memory needs this
#
#
# if(PATH_OPENVIBE_CONTRIBUTIONS)
#     include("${CMAKE_SOURCE_DIR}/extras/contrib/common/contribAcquisitionServerLinkLibs.cmake")
# endif(PATH_OPENVIBE_CONTRIBUTIONS)
#
# if(NOT(DISABLE_MENSIA_ACQUISITION_DRIVER))
#     add_definitions(-DTARGET_HasMensiaAcquisitionDriver)
# endif()

# Install files
install(TARGETS ${PROJECT_NAME}
		RUNTIME DESTINATION ${DIST_BINDIR}
		LIBRARY DESTINATION ${DIST_LIBDIR}
		ARCHIVE DESTINATION ${DIST_LIBDIR})
