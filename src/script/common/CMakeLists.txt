# --- UTF8Sign Engine Options ---
option(ENABLE_UTF8_ATLAS "Enable Standard Atlas (Irrlicht)" OFF)
option(ENABLE_UTF8_SDL2_ATLAS "Enable Extended Atlas (SDL2)" ON)
option(ENABLE_UTF8_SDL2_FREETYPE "Enable FreeType Engine" OFF)

if(ENABLE_UTF8_ATLAS)
    add_definitions(-DUTF8_ATLAS=1)
else()
    add_definitions(-DUTF8_ATLAS=0)
endif()

if(ENABLE_UTF8_SDL2_ATLAS)
    add_definitions(-DUTF8_SDL2_ATLAS=1)
else()
    add_definitions(-DUTF8_SDL2_ATLAS=0)
endif()

if(ENABLE_UTF8_SDL2_FREETYPE)
    add_definitions(-DUTF8_SDL2_FREETYPE=1)
else()
    add_definitions(-DUTF8_SDL2_FREETYPE=0)
endif()

file(GLOB common_SCRIPT_COMMON_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(common_SCRIPT_COMMON_SRCS
	${common_SCRIPT_COMMON_HDRS}
	${CMAKE_CURRENT_SOURCE_DIR}/c_content.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/c_converter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/c_internal.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/c_packer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/helper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/l_utf8sign.cpp  # ★ここに追加
	PARENT_SCOPE)

set(client_SCRIPT_COMMON_SRCS
	PARENT_SCOPE)

