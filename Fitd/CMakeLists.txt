cmake_minimum_required(VERSION 3.9)

include_directories(
    "."
    "${CMAKE_SOURCE_DIR}/FitdLib"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB FITD_DATA
    "${CMAKE_SOURCE_DIR}/data/*"
)

file(GLOB SRC_ROOT
    "*.cpp"
    "*.h"
)

set(SOURCES
 ${SOURCES}
 ${SRC_ROOT}
)

if(DREAMCAST)
    list(FILTER SOURCES EXCLUDE REGEX ".*/fitd\\.cpp$")
else()
    list(FILTER SOURCES EXCLUDE REGEX ".*/fitd_dc\\.cpp$")
endif()

IF(${CMAKE_SYSTEM_NAME} MATCHES Darwin)
    set(SOURCES
     ${SOURCES}
     "bgfxPatch.mm"
    )
endif()

source_group("src" FILES ${SRC_ROOT})

set( CMAKE_EXE_LINKER_FLAGS_SHIPPING "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /LTCG /OPT:ICF /OPT:REF /SUBSYSTEM:WINDOWS /DEBUG" )

add_executable (Fitd ${SOURCES})

if(DREAMCAST)
    # Add GLdc build output to linker search path
    if(DEFINED ENV{KOS_BASE})
        link_directories($ENV{KOS_BASE}/addons/GLdc/dcbuild)
    endif()
    TARGET_LINK_LIBRARIES(Fitd FitdLib GL z ${EXTRA_LIBS})
else()
    TARGET_LINK_LIBRARIES(Fitd FitdLib bgfx zlibstatic SDL3-static soloud ${EXTRA_LIBS})
endif()

set_target_properties(Fitd PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data/AITD1_CD")
set_target_properties(Fitd PROPERTIES WIN32_EXECUTABLE FALSE)

set_property( DIRECTORY PROPERTY VS_STARTUP_PROJECT Fitd)


