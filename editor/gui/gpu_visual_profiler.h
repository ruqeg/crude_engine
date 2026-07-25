#pragma once

#include <editor/gui/base.h>
#include <engine/graphics/gpu_profiler.h>

CRUDE_GUI_BASE_DECLARE( gpu_visual_profiler, {
  crude_gfx_device                                        *gpu;
  float32                                                  max_duration;
  uint32                                                   max_frames;
  uint32                                                   max_queries_per_frame;
  uint32                                                   current_frame;
  int32                                                    max_visible_depth;
  float32                                                  average_time;
  float32                                                  max_time;
  float32                                                  min_time;
  float32                                                  new_average;
  uint32                                                   framebuffer_pixel_count;
  uint16                                                  *per_frame_active;
  crude_gfx_gpu_time_query                                *timestamps;
  crude_gfx_gpu_pipeline_statistics                       *pipeline_statistics;
  crude_heap_allocator                                    *allocator;
  CRUDE_HASHMAPSTR( uint32 )                              *name_to_color_index;
  uint32                                                   initial_frames_paused;
  bool                                                     paused;
  bool                                                     enabled;
} );
