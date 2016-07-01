set(SOURCE "")
if(MFX_VC1_PARSER)
  set(SOURCE
      "${CMAKE_CURRENT_SOURCE_DIR}/gstvc1parse.c")
endif()
set(GST_PARSE ${SOURCE} PARENT_SCOPE)
