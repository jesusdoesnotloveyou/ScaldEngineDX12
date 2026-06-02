set(PROJECT_NAME GameFrameworkLib)

set(GAME_FRAMEWORK_INCLUDES
	components/ComponentManager.h
	components/Renderer.h
	components/SComponent.h
	components/Transform.h
	objects/SEntity.h)
set(GAME_FRAMEWORK_SOURCES
	components/Renderer.cpp
	components/SComponent.cpp
	components/Transform.cpp
	objects/SEntity.cpp)

add_library(${PROJECT_NAME} SHARED ${GAME_FRAMEWORK_INCLUDES} ${GAME_FRAMEWORK_SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE _EXPORTING) # specify PRIVATE to prevent _EXPORTING definition moving to target we will link this lib to

# includes
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/Source
)

# =============================================================
# debug info
message("================== ${PROJECT_NAME} ==================")
message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message("==================================================")