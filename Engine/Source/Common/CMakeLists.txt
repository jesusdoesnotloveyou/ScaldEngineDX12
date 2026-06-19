# general info
cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
# project info
set(PROJECT_NAME ScaldCommonLib)

# cpp options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# current targets' sources
set(SCALD_COMMON_INCLUDES
	KeyboardDevice.h
	MousePad.h
	ScaldCommonDefines.h
	ScaldMath.h
	ScaldTimer.h
)
set(SCALD_COMMON_SOURCES
	KeyboardDevice.cpp
	MousePad.cpp
	ScaldMath.cpp
	ScaldTimer.cpp
)

add_library(${PROJECT_NAME} STATIC ${SCALD_COMMON_INCLUDES} ${SCALD_COMMON_SOURCES})

# ===== FetchContent =====
# DX12 setup
if(${WIN32})
	FetchContent_Declare(
		DirectxTex
		GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
		GIT_TAG oct2025
		GIT_SHALLOW TRUE
	)
	FetchContent_MakeAvailable(DirectxTex)

	FetchContent_Declare(
        DirectXTK
        GIT_REPOSITORY https://github.com/microsoft/DirectXTK12.git
        GIT_TAG may2026
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(DirectXTK)

	get_property(_targets DIRECTORY "${directxtk_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
	message("DirectXTK12 targets: ${_targets}")
	get_target_property(DXTK_INCLUDES DirectXTK12 INTERFACE_INCLUDE_DIRECTORIES)
	message("DirectXTK12 includes: ${DXTK_INCLUDES}")
else()
    message(WARNING "DirectXTex and DirectXTK are WIN32 only libraries, they will not be loaded")
endif()

# current target
# link libraries
target_link_libraries(${PROJECT_NAME}
	# TODO: I think it's better to separate DirectXTK12 to different library (to avoid linking to GameFramework), but for now it is here
	PUBLIC dxgi.lib dxguid.lib d3d12.lib d3dcompiler.lib DirectXTex DirectXTK12 # PUBLIC for all users of ScaldCommonLib
)
# includes
target_include_directories(${PROJECT_NAME} PUBLIC # PUBLIC to expand using of this for ALL users of ScaldCommonLib
	${directxtk_SOURCE_DIR}/Src # for d3dx12.h
)
# pch
target_precompile_headers(${PROJECT_NAME} PUBLIC # PUBLIC to expand using of this pch for ALL users of ScaldCommonLib
	${CMAKE_SOURCE_DIR}/Engine/Source/stdafx.h
)
if (MSVC)
	target_compile_definitions(${PROJECT_NAME} PUBLIC UNICODE _UNICODE) # CHAR WCHAR windows stuff, UNICODE forces using of wide strings and so on
endif()

# =============================================================
# debug info
message("================== ${PROJECT_NAME} ==================")
message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message("=======================================================")