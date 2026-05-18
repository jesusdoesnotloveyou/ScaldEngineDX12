set(PROJECT_NAME GameLib)

set(GAME_FRAMEWORK_INCLUDES
	components/ComponentManager.h
	components/Renderer.h
	components/Scene.h
	components/SComponent.h
	components/Transform.h
	objects/SEntity.h)
set(GAME_FRAMEWORK_SOURCES
	components/Renderer.cpp
	components/Scene.cpp
	components/SComponent.cpp
	components/Transform.cpp
	objects/SEntity.cpp)

add_library(${PROJECT_NAME} SHARED ${GAME_FRAMEWORK_INCLUDES} ${GAME_FRAMEWORK_SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE _EXPORTING) # specify PRIVATE to prevent _EXPORTING definition moving to target we will link this lib to

message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")