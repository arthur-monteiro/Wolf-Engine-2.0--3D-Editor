# Snowy sponza

This scene shows the famous Sponza (https://www.intel.com/content/www/us/en/developer/topic-technology/graphics-research/samples.html) with snow.

<p align="center">
  <img src="./Screenshots/snowySponza/scene.gif"  width="1080"/>
</p>
<p align="center" style="margin-top: -20px;">
    <sub><sup>Work in progress</sup></sub>
</p>

# Geometry

First of all, we render sponza with a cloudy sky:

<p align="center">
  <img src="./Screenshots/snowySponza/geometry_albedo.png" width="350"/>
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="./Screenshots/snowySponza/sky.png" width="350"/>
</p>

Due to the cloudy sky, direct lighting will no be very useful (as well as shadows), almost all the lighting is done using [ray traced global illumination](voxelGI_README.md)

<p align="center">
  <img src="./Screenshots/snowySponza/geometry_lighting.png"  width="1080"/>
</p>

# Particles

The main focus here is the snow. It's done using 160'000 particles randomly spawned in a rectangle 30m above the scene:

<p align="center">
  <img src="./Screenshots/snowySponza/particles_params.png" width="304"/>
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="./Screenshots/snowySponza/particles_albedo.gif" width="385"/>
</p>

Particles are lit using the global irradiance system:

<p align="center">
  <img src="./Screenshots/snowySponza/particles_lit.png"  width="1080"/>
</p>

## Collision

An important feature here is particles collision. It's handled by rendering a top-down depth image of the scene and comparing the depths in projection space:

<p align="center">
  <img src="./Screenshots/snowySponza/depth_buffer.png" width="350"/>
  &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
  <img src="./Screenshots/snowySponza/particles_collision.gif" width="380"/>
</p>