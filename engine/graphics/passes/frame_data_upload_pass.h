#pragma once

#include <engine/graphics/scene_renderer_resources.h>

typedef struct crude_gfx_frame_data_upload_pass_context
{
  crude_gfx_ddgi_area_cpu                                  ddgi_area;
  int32                                                    ddgi_probe_update_offset;
  float32                                                  ddgi_rotation_scaler;
  crude_world_environment                                  world_environment;
  crude_gfx_scene_context                                  scene;
  crude_gfx_debug_context                                  debug;
} crude_gfx_frame_data_upload_pass_context;

CRUDE_API void
crude_gfx_frame_data_upload_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);
