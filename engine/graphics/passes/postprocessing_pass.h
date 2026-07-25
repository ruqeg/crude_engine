#pragma once

#include <engine/graphics/scene_renderer_resources.h>

#include <engine/graphics/shaders/common/culling.h>

typedef struct crude_gfx_luminance_calculation_pass_context
{
  float32                                                  min_log_lum;
  float32                                                  max_log_lum;
  int64                                                    luminance_avarge_last_update_delta_time;
} crude_gfx_luminance_calculation_pass_context;

CRUDE_API void
crude_gfx_luminance_calculation_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);

typedef struct crude_gfx_postprocessing_pass_context
{
  float32                                                  gamma;
} crude_gfx_postprocessing_pass_context;

CRUDE_API void
crude_gfx_postprocessing_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);