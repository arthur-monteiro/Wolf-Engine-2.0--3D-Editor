#pragma once

#include <cstdint>

namespace CommonCameraIndices
{
	constexpr uint32_t CAMERA_IDX_MAIN = 0;
	constexpr uint32_t CAMERA_IDX_SHADOW_CASCADE_0 = 1; // make sure to keep cascade idx in the same order
	constexpr uint32_t CAMERA_IDX_SHADOW_CASCADE_1 = 2;
	constexpr uint32_t CAMERA_IDX_SHADOW_CASCADE_2 = 3;
	constexpr uint32_t CAMERA_IDX_SHADOW_CASCADE_3 = 4;
	constexpr uint32_t CAMERA_IDX_THUMBNAIL_GENERATION = 5;
}

namespace CommonPipelineIndices
{
	constexpr uint32_t PIPELINE_IDX_PRE_DEPTH = 0;
	constexpr uint32_t PIPELINE_IDX_SHADOW_MAP = 1;
	constexpr uint32_t PIPELINE_IDX_FORWARD = 2;
	constexpr uint16_t PIPELINE_IDX_OUTPUT_IDS = 3;
}

namespace DescriptorSetSlots
{
	constexpr uint32_t DESCRIPTOR_SET_SLOT_CAMERA = 0;
	constexpr uint32_t DESCRIPTOR_SET_SLOT_FORWARD_COMMON = 1; // for all in forward pass
	constexpr uint32_t DESCRIPTOR_SET_SLOT_BINDLESS = 2; // for textured meshes (not debug mesh) in forward pass
	constexpr uint32_t DESCRIPTOR_SET_SLOT_MESH_DEBUG = 2; // used for matrices for debug meshes
	constexpr uint32_t DESCRIPTOR_SET_SLOT_LIGHT_INFO = 3;
	constexpr uint32_t DESCRIPTOR_SET_SLOT_SHADOW_MASK_INFO = 4;
	constexpr uint32_t DESCRIPTOR_SET_SLOT_COUNT = 5;
}

namespace AdditionalDescriptorSetsMaskBits
{
	constexpr uint32_t SHADOW_MASK_INFO = 1 << 0;
}
