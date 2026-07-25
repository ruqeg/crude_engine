#pragma once

#include <engine/graphics/scene_renderer_resources.h>

#include <engine/graphics/shaders/common/culling.h>

typedef struct crude_gfx_ray_tracing_solid_pass_context
{
  VkDeviceAddress                                          tlas_address;
} crude_gfx_ray_tracing_solid_pass_context;

CRUDE_API void
crude_gfx_ray_tracing_solid_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);
