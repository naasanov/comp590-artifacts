get_property(OV_PRINTED GLOBAL PROPERTY OV_TRIED_ThirdPartyQt)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Xml)

IF(Qt6_FOUND)

    if(QT_KNOWN_POLICY_QTP0001)
        qt_policy(SET QTP0001 NEW)
    endif()

    ov_print(OV_PRINTED "Found Qt library")
ELSE()
    ov_print(OV_PRINTED "  FAILED to find Qt")
ENDIF()

set_property(GLOBAL PROPERTY OV_TRIED_ThirdPartyQt "Yes")
