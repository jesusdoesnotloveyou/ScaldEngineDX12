# general info
cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
# project info
set(PROJECT_NAME ScaldCoreLib)

# cpp options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# source files
set(SCALD_CORE_SOURCES
	Camera.cpp
	CascadeShadowMap.cpp
	CommandList.cpp
	CommandQueue.cpp
	D3D12Sample.cpp
	Device.cpp
	Engine.cpp
	FileSystemObject.cpp
	FrameResource.cpp
	GBuffer.cpp
	GraphicsContext.cpp
	Material.cpp
	Model.cpp
	RootSignature.cpp
	ScaldUtil.cpp
	Scene.cpp
	SceneNode.cpp
    ShadowMap.cpp
	Shapes.cpp
	SSAO.cpp
	SwapChain.cpp
	Texture.cpp
	Win32App.cpp
)
	
set(SCALD_CORE_INCLUDES
	AssetLoader.h
	Camera.h
	CascadeShadowMap.h
	CommandList.h
	CommandQueue.h
	D3D12Sample.h
	Device.h
	DXHelper.h
	Engine.h
	FileSystemObject.h
	FrameResource.h
	GBuffer.h
	GraphicsContext.h
	Material.h
	Mesh.h
	Model.h
	RootSignature.h
	ScaldCoreDefines.h
	ScaldCoreTypes.h
    ScaldUtil.h
	Scene.h
	SceneNode.h
	ShadowMap.h
	Shapes.h
	SSAO.h
	SwapChain.h
	Texture.h
	UploadBuffer.h
	VertexTypes.h
	Win32App.h
)

add_library(${PROJECT_NAME} STATIC ${SCALD_CORE_INCLUDES} ${SCALD_CORE_SOURCES})

# import definitions for dll
target_compile_definitions(${PROJECT_NAME} PUBLIC _IMPORTING) # current target IMPORTING

# ===== FetchContent =====
# Assimp setup
FetchContent_Declare(
	Assimp
    	GIT_REPOSITORY https://github.com/assimp/assimp.git
    	GIT_TAG v6.0.5
    	GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Assimp)

# imgui setup
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)
message("Imgui source dir: ${imgui_SOURCE_DIR}")

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

# ===== targets =====
# imgui target
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

# current target
target_include_directories(${PROJECT_NAME} PRIVATE # PRIVATE because shaders need to be accessed only from the core (at least for now)
	${CMAKE_SOURCE_DIR}/Engine/Assets/Shaders
)
target_compile_definitions(${PROJECT_NAME} PRIVATE _IMPORTING) # to import dll here
target_link_libraries(ScaldCoreLib PRIVATE assimp::assimp imgui)

# options
option(ENABLE_IMGUI_OPT "Enable/disable ImGUI" ON)
if(ENABLE_IMGUI_OPT)
	target_compile_definitions(${PROJECT_NAME} PUBLIC ENABLE_IMGUI)
endif()

# ======================================================================
# debug info
message("================== ${PROJECT_NAME} ==================")
message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message("==================================================")