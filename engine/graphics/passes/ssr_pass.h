#pragma once

#include <engine/graphics/scene_renderer_resources.h>

#include <engine/graphics/shaders/common/culling.h>

typedef struct crude_gfx_ssr_options
{
  float32                                                  max_steps;
  float32                                                  fade_start;
  float32                                                  fade_end;
} crude_gfx_ssr_options;

typedef struct crude_gfx_ssr_pass_context
{
  crude_gfx_ssr_options                                    ssr;
} crude_gfx_ssr_pass_context;

CRUDE_API void
crude_gfx_ssr_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);