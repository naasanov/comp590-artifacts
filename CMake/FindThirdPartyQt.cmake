get_property(OV_PRINTED GLOBAL PROPERTY OV_TRIED_ThirdPartyQt)

find_package(Qt6 REQUIRED COMPONENTS Charts Core Gui Qml Quick QuickControls2 Widgets Xml)

IF(Qt6_FOUND)

    if(QT_KNOWN_POLICY_QTP0001)
        qt_policy(SET QTP0001 NEW)
    endif()
    set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml)

    ov_print(OV_PRINTED "Found Qt library")
ELSE()
    ov_print(OV_PRINTED "  FAILED to find Qt")
ENDIF()

set_property(GLOBAL PROPERTY OV_TRIED_ThirdPartyQt "Yes")
