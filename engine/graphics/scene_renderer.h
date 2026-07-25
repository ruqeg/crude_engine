#pragma once

#include <cstdint>
#include <threads.h>

#include <engine/graphics/graphics_config.h>
#include <engine/graphics/asynchronous_loader.h>
#include <engine/graphics/scene_renderer_resources.h>
#include <engine/graphics/passes/imgui_pass.h>
#include <engine/graphics/model_renderer_resources_manager.h>
#include <engine/graphics/frame_graph.h>

#if CRUDE_DEVELOP
#include <engine/physics/physics_shapes_manager.h>
#endif

typedef struct crude_gfx_scene_renderer_creation
{
  crude_gfx_model_renderer_resources_manager              *model_renderer_resources_manager;
  void                                                    *imgui_context;
  crude_gfx_asynchronous_loader                           *async_loader;
  crude_heap_allocator                                    *allocator;
  crude_gfx_frame_graph                                   *frame_graph;
  bool                                                     imgui_pass_enalbed;
#if CRUDE_DEVELOP
  crude_physics_shapes_manager                            *physics_shapes_manager;
#endif
} crude_gfx_scene_renderer_creation;

typedef struct crude_gfx_scene_renderer_options
{
  float32                                                  gamma;
  crude_camera                                             camera;
  XMFLOAT4X4                                               camera_view_to_world;

#if CRUDE_DEVELOP
  struct 
  {
    uint32                                                 debug_mode;
    bool                                                   show_debug_collision;
    bool                                                   show_debug_gltf;
    uint32                                                 flags1;
    float32                                                force_roughness;
    float32                                                force_metalness;
  } debug;
#endif
  float32                                                  absolute_time;
} crude_gfx_scene_renderer_options;

typedef struct crude_gfx_scene_renderer
{
  /***********************
   * Context 
   **********************/
  void                                                    *imgui_context;
  crude_gfx_device                                        *gpu;
  crude_gfx_frame_graph                                   *frame_graph;
  crude_gfx_asynchronous_loader                           *async_loader;
  crude_heap_allocator                                    *allocator;
  crude_gfx_model_renderer_resources_manager              *model_renderer_resources_manager;
#if CRUDE_DEVELOP
  crude_physics_shapes_manager                            *physics_shapes_manager;
#endif
  
  /***********************
   * Base
   **********************/
  crude_gfx_cmd_buffer                                    *primary_cmd;
  crude_gfx_scene_renderer_options                         options;
  bool                                                     imgui_pass_enalbed;
  
#if CRUDE_DEVELOP
  crude_gfx_model_renderer_resources_instance             *debug_model_renderer_resoruces_instances;
  crude_gfx_debug_line_vertex                             *debug_line_vertices_3d;

  crude_gfx_model_renderer_resources_instance              light_model_renderer_resources_instance;
  crude_gfx_model_renderer_resources_instance              camera_model_renderer_resources_instance;
  crude_gfx_model_renderer_resources_instance              capsule_model_renderer_resources_instance;
  crude_gfx_model_renderer_resources_instance              physics_box_collision_model_renderer_resources_instance;
  crude_gfx_model_renderer_resources_instance              audo_player_model_renderer_resources_instance;
  crude_gfx_model_renderer_resources_instance              audio_listener_model_renderer_resources_instance;
  crude_gfx_model_renderer_resources_instance              ray_model_renderer_resources_instance;
#endif /* CRUDE_DEVELOP */

  /***********************
   * Common Mesh & Meshlets CPU & GPU Data
   **********************/
  crude_gfx_model_renderer_resources_instance             *model_renderer_resoruces_instances;
  crude_gfx_model_renderer_resources_instance             *prev_model_renderer_resoruces_instances;

  bool                                                     ddgi_debug;
  bool                                                     ddgi_enabled;
  crude_gfx_ddgi_area_cpu                                  ddgi_area;
  crude_gfx_ddgi_area_cpu                                  prev_ddgi_area;
  crude_world_environment                                  world_environment;

  /***********************
   * Common CPU
   **********************/
  crude_gfx_light_cpu                                     *lights;
  crude_gfx_culled_light_cpu                              *culled_lights;
  crude_gfx_mesh_instance_draw                            *meshes_instances_draws;
  XMFLOAT4X4                                              *joint_matrices;

#if CRUDE_GFX_RAY_TRACING_ENABLED
  crude_gfx_rhi_acceleration_structure                     rhi_tlas;
  crude_gfx_memory_allocation                              tlas_hga;
  crude_gfx_memory_allocation                              tlas_instances_hga;
  crude_gfx_memory_allocation                              tlas_scratch_hga;
#endif /* CRUDE_GFX_RAY_TRACING_ENABLED */

  float32                                                  ddgi_rotation_scaler;
  int32                                                    ddgi_probe_update_offset;
  int32                                                    ddgi_offsets_calculations_count;
  crude_gfx_sampler_handle                                 tetrahedron_shadow_sampler;
  crude_gfx_sampler_handle                                 depth_pyramid_farthest_sampler;
  crude_gfx_sampler_handle                                 depth_pyramid_nearest_sampler;
  crude_gfx_sampler_handle                                 radiance_hierarchy_sampler;
  crude_gfx_sampler_handle                                 probe_grid_sampler_handle;
  crude_gfx_texture_handle                                 imgui_font_texture_handle;

  float32                                                  luminance_avarge_last_update_time;
} crude_gfx_scene_renderer;


/**
 *
 * Renderer Scene
 * 
 */
CRUDE_API void
crude_gfx_scene_renderer_initialize
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ crude_gfx_scene_renderer_creation                  *creation
);

CRUDE_API void
crude_gfx_scene_renderer_deinitialize
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
);

CRUDE_API bool
crude_gfx_scene_renderer_update_instances_from_node
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ crude_ecs                                          *world,
  _In_ crude_entity                                        main_node
);

CRUDE_API void
crude_gfx_scene_renderer_start_frame
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
);

CRUDE_API void
crude_gfx_scene_renderer_update
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
);

CRUDE_API void
crude_gfx_scene_renderer_render
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
);

CRUDE_API void
crude_gfx_scene_renderer_queue
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
);

CRUDE_API void
crude_gfx_scene_renderer_on_resize
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
);

#if CRUDE_DEVELOP
CRUDE_API void
crude_gfx_scene_renderer_debug_queue_clear
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
);

CRUDE_API void
crude_gfx_scene_renderer_debug_queue_draw_line3d
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ XMVECTOR                                            start,
  _In_ XMVECTOR                                            end,
  _In_ XMVECTOR                                            color
);

CRUDE_API void
crude_gfx_scene_renderer_debug_queue_draw_capsule
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ XMMATRIX                                            model_to_world,
  _In_ float32                                             height,
  _In_ float32                                             radius
);
#endif

/* Utils */
#define CRUDE_GFX_PASS_TEXTURE_HANDLE( name )\
  crude_gfx_render_graph_builder_access_resource_by_name(\
    pass->scene_renderer->render_graph->builder,\
    pass->scene_renderer->options.name\
  )->resource_info.texture.handle

#define CRUDE_GFX_PASS_TEXTURE_INDEX( name ) CRUDE_GFX_PASS_TEXTURE_HANDLE( name ).index
