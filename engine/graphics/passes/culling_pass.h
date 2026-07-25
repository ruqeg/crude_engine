#pragma once

#include <engine/graphics/scene_renderer_resources.h>

#include <engine/graphics/shaders/common/culling.h>

typedef enum crude_gfx_culling_pass_type
{
  CRUDE_GFX_CULLING_PASS_TYPE_EARLY,
  CRUDE_GFX_CULLING_PASS_TYPE_LATE,
} crude_gfx_culling_pass_type;

typedef struct crude_gfx_culling_pass_context
{
  crude_gfx_scene_context                                  scene;
  crude_gfx_culling_pass_type                              culling_type;

} crude_gfx_culling_pass_context;

CRUDE_API void
crude_gfx_culling_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);