#pragma once

#include "engine/graphics/shaders/common/platform.h"
#include <engine/graphics/scene_renderer_resources.h>

typedef enum crude_gfx_opaque_pass_geometry_type
{
  CRUDE_GFX_OPAQUE_PASS_GEOMETRY_TYPE_CLASSIC,
  CRUDE_GFX_OPAQUE_PASS_GEOMETRY_TYPE_MESHLET,
} crude_gfx_opaque_pass_geometry_type;

typedef enum crude_gfx_opaque_pass_type
{
  CRUDE_GFX_OPAQUE_PASS_TYPE_EARLY,
  CRUDE_GFX_OPAQUE_PASS_TYPE_LATE,
} crude_gfx_opaque_pass_type;

typedef struct crude_gfx_opaque_pass_context
{
  crude_gfx_opaque_pass_type                               type;
  crude_gfx_opaque_pass_geometry_type                      geometry_type;
  crude_gfx_scene_context                                  scene;
  VkDeviceAddress                                          tlas_address;
} crude_gfx_opaque_pass_context;

CRUDE_API void
crude_gfx_opaque_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);
