#project info
cmake_minimum_required(VERSION 3.21 FATAL_ERROR) 
set(PROJECT_NAME ScaldEngineDX12)
project(${PROJECT_NAME} VERSION 0.0.1 LANGUAGES CXX C)

# common options
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# directory for libraries artifacts
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SUPPRESS_REGENERATION TRUE)

# cpp options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# global compile options
#add_compile_options()

# build options
set(CMAKE_CONFIGURATION_TYPES
	${CMAKE_CONFIGURATION_TYPES} # to save its current value
        #Development 			 # from Blainnflare
        #Shipping 				 # from Blainnflare
)

# source files
set(SOURCES
	Engine/Source/Common/KeyboardDevice.cpp
	Engine/Source/Common/MousePad.cpp
	Engine/Source/Common/ScaldUtil.cpp

	Engine/Source/Core/Camera.cpp
	Engine/Source/Core/CascadeShadowMap.cpp
	Engine/Source/Core/CommandQueue.cpp
	Engine/Source/Core/D3D12Sample.cpp
	Engine/Source/Core/Device.cpp
	Engine/Source/Core/Engine.cpp
	Engine/Source/Core/FrameResource.cpp
	Engine/Source/Core/GBuffer.cpp
	Engine/Source/Core/Main.cpp
	Engine/Source/Core/RootSignature.cpp
	Engine/Source/Core/ShadowMap.cpp
	Engine/Source/Core/SSAO.cpp
	Engine/Source/Core/Win32App.cpp
)
	
set(INCLUDES
	Engine/Source/Common/KeyboardDevice.h
	Engine/Source/Common/MousePad.h
	Engine/Source/Common/ScaldCoreDefines.h
	Engine/Source/Common/ScaldCoreTypes.h
	Engine/Source/Common/ScaldUtil.h
	Engine/Source/Common/VertexTypes.h
	
	Engine/Source/Core/Camera.h
	Engine/Source/Core/CascadeShadowMap.h
	Engine/Source/Core/CommandQueue.h
	Engine/Source/Core/D3D12Sample.h
	Engine/Source/Core/Device.h
	Engine/Source/Core/Engine.h
	Engine/Source/Core/FrameResource.h
	Engine/Source/Core/GBuffer.h
	Engine/Source/Core/Mesh.h
	Engine/Source/Core/RootSignature.h
	Engine/Source/Core/ShadowMap.h
	Engine/Source/Core/SSAO.h
	Engine/Source/Core/UploadBuffer.h
	Engine/Source/Core/Win32App.h

	Engine/Source/stdafx.h # optional, just for VS visibility
)

# sources for libraries (static, dynamic)
set(SCALD_COMMON_INCLUDES
	Engine/Source/Common/DXHelper.h
	Engine/Source/Common/ScaldCommonDefines.h
	Engine/Source/Common/ScaldTimer.h
	Engine/Source/Core/Shapes.h
)
set(SCALD_COMMON_SOURCES
	Engine/Source/Common/ScaldTimer.cpp
	Engine/Source/Core/Shapes.cpp
)

set(GAME_FRAMEWORK_INCLUDES
	Engine/Source/GameFramework/components/ComponentManager.h
	Engine/Source/GameFramework/components/Renderer.h
	Engine/Source/GameFramework/components/Scene.h
	Engine/Source/GameFramework/components/SComponent.h
	Engine/Source/GameFramework/components/Transform.h
	Engine/Source/GameFramework/objects/SEntity.h)
set(GAME_FRAMEWORK_SOURCES
	Engine/Source/GameFramework/components/Renderer.cpp
	Engine/Source/GameFramework/components/Scene.cpp
	Engine/Source/GameFramework/components/SComponent.cpp
	Engine/Source/GameFramework/components/Transform.cpp
	Engine/Source/GameFramework/objects/SEntity.cpp)

set(MATH_INCLUDES Engine/Source/Common/ScaldMath.h)
set(MATH_SOURCES Engine/Source/Common/ScaldMath.cpp)

# filters
#source_group("Source files/<SUBFOLDER_FILTER_NAME>" FILES "Source/<SUBFOLDER_NAME>/FileName.cpp")
#source_group("Header files/<SUBFOLDER_FILTER_NAME>" FILES "Include/<SUBFOLDER_NAME>/FileName.h")

# 3rd party
include(FetchContent)

FetchContent_Declare(
	Assimp
    	GIT_REPOSITORY https://github.com/assimp/assimp.git
    	GIT_TAG v6.0.5
    	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Assimp)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)
message("Imgui source dir: ${imgui_SOURCE_DIR}")

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
    message(WARNING "DirectXTex and DirectXTK are WIN32 only libraries, they would not be loaded")
endif()

# manually define imgui as a library since it has no CMakeLists.txt
set(IMGUI_SOURCES
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
   	${imgui_SOURCE_DIR}/imgui_demo.cpp
)
if(${WIN32}) # dx12 + win32
	set(IMGUI_SOURCES
		${IMGUI_SOURCES}
    	${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    	${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
	)
endif()

# targets
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${INCLUDES})
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)

add_library(MathLib STATIC ${MATH_INCLUDES} ${MATH_SOURCES})
add_library(ScaldCommonLib STATIC ${SCALD_COMMON_INCLUDES} ${SCALD_COMMON_SOURCES})
add_library(GameLib SHARED ${GAME_FRAMEWORK_INCLUDES} ${GAME_FRAMEWORK_SOURCES})
add_library(imgui STATIC ${IMGUI_SOURCES})

target_compile_definitions(GameLib PRIVATE _EXPORTING) # PRIVATE to prevent _exporing move to target we will link DDSLib to
target_compile_definitions(${PROJECT_NAME} PUBLIC _IMPORTING)

# options
option(ENABLE_IMGUI_OPT "Enable/disable ImGUI" ON)
if(ENABLE_IMGUI_OPT)
	target_compile_definitions(${PROJECT_NAME} PUBLIC ENABLE_IMGUI)
endif()

# pch
target_precompile_headers(ScaldCommonLib PUBLIC # PUBLIC to expand using of this pch for ALL users of ScaldCommonLib
	${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source/stdafx.h
)

# includes
target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/Engine/Assets/Shaders
)
target_include_directories(ScaldCommonLib PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/Engine/Source
	${directxtk_SOURCE_DIR}/Src # for d3dx12.h
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# link libraries to different targets
target_link_libraries(ScaldCommonLib
	PUBLIC dxgi.lib dxguid.lib d3d12.lib d3dcompiler.lib # PUBLIC for all users of ScaldCommonLib
	# TODO: I think it's better to separate DirectXTK12 to different library (to avoid linking to GameFramework), but for now it is here
	PUBLIC DirectXTK12 						  # PUBLIC for all users of ScaldCommonLib 
)
target_link_libraries(GameLib
	PRIVATE ScaldCommonLib
)
target_link_libraries(${PROJECT_NAME}
	PRIVATE assimp::assimp DirectXTex imgui
	PRIVATE ScaldCommonLib GameLib
	PUBLIC MathLib # TODO: probably should be moved to ScaldCommon
)

# compile options for target
# force UTF8, but looks like it does nothing
target_compile_options(${PROJECT_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
		$<$<CXX_COMPILER_ID:Clang>:/utf-8>
)

if (MSVC)
	target_compile_definitions(${PROJECT_NAME} PUBLIC UNICODE _UNICODE) # CHAR WCHAR windows stuff, UNICODE forces using of wide strings and so on
endif()

########### other actions
# for copying assets to the directory with the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Engine/Assets"
		"$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets"
)

# =============================================================
# debug info
if(${WIN32})
	message("Running on Windows")
elseif(${LINUX})
	message("Running on Linux")
endif()

if(${MSVC})
	message("MSVC version: ${MSVC_VERSION}")
	message("MSVC toolset version: ${MSVC_TOOLSET_VERSION}")
endif()

message("Using ${CMAKE_CXX_COMPILER_ID} compiler")
message("Compiler flags: ${CMAKE_CXX_FLAGS}")
message("Compiler debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
message("Compiler release flags: ${CMAKE_CXX_FLAGS_RELEASE}")

message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message("CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message("CMAKE_RUNTIME_OUTPUT_DIRECTORY: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

message("Configuration types: ${CMAKE_CONFIGURATION_TYPES}")