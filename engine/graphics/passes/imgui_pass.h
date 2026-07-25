#pragma once

#include <engine/graphics/scene_renderer_resources.h>

#include <engine/graphics/shaders/common/culling.h>

typedef struct crude_gfx_imgui_generate_buffers_pass_context
{
  crude_gfx_texture_handle                                 imgui_font_texture_handle;
  void                                                    *imgui_context;
} crude_gfx_imgui_generate_buffers_pass_context;

CRUDE_API void
crude_gfx_imgui_generate_buffers_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);

typedef struct crude_gfx_imgui_pass_context
{
  void                                                    *imgui_context;
} crude_gfx_imgui_pass_context;

CRUDE_API void
crude_gfx_imgui_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);