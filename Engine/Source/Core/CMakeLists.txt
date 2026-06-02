set(PROJECT_NAME ScaldCoreLib)

# source files
set(SCALD_CORE_SOURCES
	Camera.cpp
	CascadeShadowMap.cpp
	CommandQueue.cpp
	D3D12Sample.cpp
	Device.cpp
	Engine.cpp
	FileSystemObject.cpp
	FrameResource.cpp
	GBuffer.cpp
	Material.cpp
	Model.cpp
	RootSignature.cpp
	ScaldUtil.cpp
	Scene.cpp
    ShadowMap.cpp
	Shapes.cpp
	SSAO.cpp
	Texture.cpp
	Win32App.cpp
)
	
set(SCALD_CORE_INCLUDES
	AssetLoader.h
	Camera.h
	CascadeShadowMap.h
	CommandQueue.h
	D3D12Sample.h
	Device.h
	DXHelper.h
	Engine.h
	FileSystemObject.h
	FrameResource.h
	GBuffer.h
	Material.h
	Mesh.h
	Model.h
	RootSignature.h
	ScaldCoreDefines.h
	ScaldCoreTypes.h
    ScaldUtil.h
	Scene.h
	ShadowMap.h
	Shapes.h
	SSAO.h
	Texture.h
	UploadBuffer.h
	VertexTypes.h
	Win32App.h
)

add_library(${PROJECT_NAME} STATIC ${SCALD_CORE_INCLUDES} ${SCALD_CORE_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE # PRIVATE because shaders need to be accessed only from the core (at least for now)
	${CMAKE_SOURCE_DIR}/Engine/Assets/Shaders
)

# includes
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/Source
)

target_compile_definitions(${PROJECT_NAME} PRIVATE _IMPORTING) # to import dll here

# =============================================================
# debug info
message("================== ${PROJECT_NAME} ==================")
message("CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")
message("CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message("==================================================")