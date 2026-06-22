# general info
cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
# project info
set(PROJECT_NAME ScaldLogLib)

# cpp options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# current targets' includes
set(SCALD_LOG_INCLUDES Log.h)
# current targets' sources
set(SCALD_LOG_SOURCES Log.cpp)

add_library(${PROJECT_NAME} STATIC ${SCALD_LOG_INCLUDES} ${SCALD_LOG_SOURCES})

# ===== FetchContent =====
# spdlog
FetchContent_Declare(
	spdlog
	GIT_REPOSITORY https://github.com/gabime/spdlog.git
	GIT_TAG v1.17.0
	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)

# current target
# link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
# includes
#target_include_directories(${PROJECT_NAME} PUBLIC/PRIVATE/INTERFACE ...)
# pch
target_precompile_headers(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Engine/Source/stdafx.h)

# =============================================================
# debug info
message("================== ${PROJECT_NAME} ==================")
message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message("=======================================================")