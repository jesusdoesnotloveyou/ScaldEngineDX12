# general info
cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
# project info
set(PROJECT_NAME ScaldEngine)
project(${PROJECT_NAME} VERSION 0.0.1)

# cpp options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# common options
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME}) # set ScaldEngine as startup project

# version
configure_file(EngineConfig.h.template ${CMAKE_CURRENT_SOURCE_DIR}/Source/EngineConfig.h)

# build options
set(CMAKE_CONFIGURATION_TYPES
	${CMAKE_CONFIGURATION_TYPES} # to save its current value
        #Development 			 # from Blainnflare
        #Shipping 				 # from Blainnflare
)

# source files
set(SOURCES Source/Main.cpp)
set(INCLUDES
	Source/EngineConfig.h
	Source/stdafx.h # optional, just for VS visibility
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${INCLUDES})

# filters
#source_group("Source files/<SUBFOLDER_FILTER_NAME>" FILES "Source/<SUBFOLDER_NAME>/FileName.cpp")
#source_group("Header files/<SUBFOLDER_FILTER_NAME>" FILES "Include/<SUBFOLDER_NAME>/FileName.h")

# 3rd party
include(FetchContent)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
)

# specify nested source directories (and libraries at the same time) with CMakeLists files inside
add_subdirectory(Source/Common) # static
add_subdirectory(Source/Core) # static
add_subdirectory(Source/Log) # static
add_subdirectory(Source/GameFramework) # dynamic

include(${CMAKE_SOURCE_DIR}/Automation/CMAKE/CMakeHelpers.cmake)

# link targets to different targets
target_link_libraries(${PROJECT_NAME} ScaldCoreLib)
target_link_libraries(${PROJECT_NAME} ScaldLogLib)
target_link_libraries(GameFrameworkLib PRIVATE ScaldCommonLib)
target_link_libraries(ScaldCoreLib
	PUBLIC ScaldCommonLib # to access from other current project too
	PRIVATE GameFrameworkLib
)

# target includes
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_include_directories(ScaldCoreLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_include_directories(ScaldLogLib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

# target compile definitions
if (MSVC)
	target_compile_definitions(${PROJECT_NAME} PUBLIC UNICODE _UNICODE) # CHAR WCHAR windows stuff, UNICODE forces using of wide strings and so on
endif()

# target compile options
target_compile_options(${PROJECT_NAME} PRIVATE
		# force UTF8, but looks like it does nothing
		$<$<CXX_COMPILER_ID:MSVC>:/utf-8>
		$<$<CXX_COMPILER_ID:Clang>:/utf-8>
)

########### other actions
# for copying assets to the directory with the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Engine/Assets"
		"$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets"
)