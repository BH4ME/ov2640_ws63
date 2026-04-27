#===============================================================================
# OV2640 WS63 library component
#===============================================================================
set(SOURCES
    "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ov2640_ws63.c"
    PARENT_SCOPE
)

set(COMPONENT_CCFLAGS
    "${COMPONENT_CCFLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/include"
    PARENT_SCOPE
)

if(DEFINED CONFIG_OV2640_WS63_DEMO)
    set(SOURCES
        "${SOURCES}"
        "${CMAKE_CURRENT_SOURCE_DIR}/demo/ov2640_ws63_demo.c"
        PARENT_SCOPE
    )
endif()
