#pragma once

#include <engine/graphics/scene_renderer_resources.h>

typedef enum crude_gfx_depth_pyramid_pass_type
{
  CRUDE_GFX_DEPTH_PYRAMID_PASS_TYPE_FARTHEST,
  CRUDE_GFX_DEPTH_PYRAMID_PASS_TYPE_NEAREST,
} crude_gfx_depth_pyramid_pass_type;

typedef struct crude_gfx_depth_pyramid_pass_context
{
  crude_gfx_depth_pyramid_pass_type                        type;
} crude_gfx_depth_pyramid_pass_context;

CRUDE_API void
crude_gfx_depth_pyramid_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);