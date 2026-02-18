# Scald Engine DirectX 12

# Screenshots
![](Screenshots/final_scene1.png)
</br>
![](Screenshots/final_scene2.png)
</br>
![](Screenshots/final_scene3.png)
</br>

# Features

## Graphics

- Deferred Rendering
  - GBuffer
    - Diffuse Albedo
    - Normal
    - Specular (FresnelR0 + Roughness)
    - Motion Vectors (work in progress)
    - Depth
  - Screen Space Ambient Occlusion
- Lighting & Shadows
  - Directional Lights
    - Cascaded Shadows
    - PCF
  - Point Lights (Instancing)
  - Spot Lights (Instancing, work in progress)
- Textures
  - Bindless
  - SkySphere
  - Normal Mapping