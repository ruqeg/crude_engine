#pragma once

#include <engine/graphics/scene_renderer_resources.h>

#include <engine/graphics/shaders/common/light.h>

typedef struct crude_gfx_pointlight_shadow_culling_pass_context
{
  crude_gfx_scene_context                                  scene;
} crude_gfx_pointlight_shadow_culling_pass_context;

CRUDE_API void
crude_gfx_pointlight_shadow_culling_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);

typedef struct crude_gfx_pointlight_shadow_commands_generation_pass_context
{
  uint32                                                   culled_lights_count;
} crude_gfx_pointlight_shadow_commands_generation_pass_context;

CRUDE_API void
crude_gfx_pointlight_shadow_commands_generation_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);

typedef struct crude_gfx_pointlight_shadow_draw_pass_context
{
  crude_gfx_culled_light_cpu                              *culled_lights;
  crude_gfx_light_cpu                                     *lights;
} crude_gfx_pointlight_shadow_draw_pass_context;

CRUDE_API void
crude_gfx_pointlight_shadow_draw_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);