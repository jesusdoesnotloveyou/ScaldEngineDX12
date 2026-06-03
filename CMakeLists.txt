#project info
cmake_minimum_required(VERSION 3.21 FATAL_ERROR) 
set(PROJECT_NAME ScaldEngineDX12)
project(${PROJECT_NAME} VERSION 0.0.1 LANGUAGES CXX C)

# common options
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for executables (.EXE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for static lib (.LIB)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # Output directory for shared lib (.DLL)

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
set(SOURCES Engine/Source/Core/Main.cpp)
set(INCLUDES
	Engine/Source/stdafx.h # optional, just for VS visibility
)

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
add_library(imgui STATIC ${IMGUI_SOURCES})

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${INCLUDES})
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)
# place current executable in its own folder named after itself
# set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${PROJECT_NAME})

# specify nested source directories with CMakeLists files inside
# and
# sources for libraries
add_subdirectory(Engine/Source/Common) # static
add_subdirectory(Engine/Source/Core) # static
add_subdirectory(Engine/Source/GameFramework) # dynamic

include(${CMAKE_SOURCE_DIR}/Automation/CMAKE/CMakeHelpers.cmake)

# import definitions for dll
target_compile_definitions(${PROJECT_NAME} PUBLIC _IMPORTING) # current executable takes IMPORTING

# options
option(ENABLE_IMGUI_OPT "Enable/disable ImGUI" ON)
if(ENABLE_IMGUI_OPT)
	target_compile_definitions(${PROJECT_NAME} PUBLIC ENABLE_IMGUI)
endif()

# includes
include_directories(
	${CMAKE_SOURCE_DIR}/Engine/Source # for all subdirectories
)
# includes for targets
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# link targets to different targets
target_link_libraries(GameFrameworkLib
	PRIVATE ScaldCommonLib
)
target_link_libraries(ScaldCoreLib
	PUBLIC ScaldCommonLib # to access from other current project too
	PRIVATE GameFrameworkLib
	PRIVATE assimp::assimp imgui
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE ScaldCoreLib
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
print_system_info()