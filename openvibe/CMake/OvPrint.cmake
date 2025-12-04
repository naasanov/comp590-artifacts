
function(OV_PRINT ALREADY_PRINTED TEXT)

if("${${ALREADY_PRINTED}}" STREQUAL "")
  message(STATUS ${TEXT})
endif("${${ALREADY_PRINTED}}" STREQUAL "")

endfunction(OV_PRINT)

