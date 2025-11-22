# Real time - Ray traced global illumination

The core of the algorithm relies on a 3D irradiance grid, where each cell contains irradiance information for the 6 faces of a cube.

<p align="center">
  <img src="./Screenshots/voxelGI/grid.png"  width="1080"/>
</p>

Cell faces are computed only if requested and computation position is adjusted depending on the request.

<p align="center">
  <img src="./Screenshots/voxelGI/requests.png"  width="1080"/>
</p>

<p align="center">
  <img src="./Screenshots/voxelGI/irradiance_debug_no_trilinear.png"  width="1080"/>
</p>

Trilinear filtering is performed to avoid seeing "blocky" patterns.

<p align="center">
  <img src="./Screenshots/voxelGI/irradiance_debug_trilinear.png"  width="1080"/>
</p>

Here the result with a sunny environment:
<p align="center">
  <img src="./Screenshots/voxelGI/final_result_sunny.png"  width="1080"/>
</p>

And with a cloudy environment:
<p align="center">
  <img src="./Screenshots/voxelGI/final_result_cloudy.png"  width="1080"/>
</p>