#pragma once

#include <engine/scene/scene_ecs.h>
#include <engine/graphics/gpu_resources.h>
#include <engine/graphics/frame_graph.h>

#include <engine/graphics/shaders/common/scene.h>
#include <engine/graphics/shaders/common/light.h>
#include <engine/graphics/shaders/common/debug.h>

#define CRUDE_LIGHT_TETRAHEDRON_DEBUG_MODEL_REALTIVE_FILEPATH CRUDE_STRING_VIEW( "editor/models/crude_light_tetrahedron.gltf" )
#define CRUDE_CAMERA_DEBUG_MODEL_REALTIVE_FILEPATH CRUDE_STRING_VIEW( "editor/models/crude_camera.gltf" )
#define CRUDE_CAPSULE_DEBUG_MODEL_REALTIVE_FILEPATH CRUDE_STRING_VIEW( "editor/models/crude_capsule.gltf" )
#define CRUDE_PHYSICS_BOX_COLLISION_SHAPE_DEBUG_MODEL_REALTIVE_FILEPATH CRUDE_STRING_VIEW( "editor/models/crude_physics_box_collision_shape.gltf" )
#define CRUDE_AUDIO_PLAYER_DEBUG_MODEL_REALTIVE_FILEPATH CRUDE_STRING_VIEW( "editor/models/crude_audio_player.gltf" )
#define CRUDE_AUDIO_LISTENER_DEBUG_MODEL_REALTIVE_FILEPATH CRUDE_STRING_VIEW( "editor/models/crude_audio_listener.gltf" )
#define CRUDE_RAY_DEBUG_MODEL_REALTIVE_FILEPATH CRUDE_STRING_VIEW( "editor/models/crude_ray.gltf" )

typedef struct crude_gfx_light_cpu
{
  crude_light                                              light;
  XMFLOAT3                                                 translation;
} crude_gfx_light_cpu;

typedef struct crude_gfx_culled_light_cpu
{
  uint32                                                   light_index;
  float32                                                  screen_area;
  XMFLOAT2                                                 tile_position;
  float32                                                  tile_size;
} crude_gfx_culled_light_cpu;

typedef struct crude_gfx_ddgi_area_cpu
{
  XMFLOAT3                                                 probe_spacing;
  XMFLOAT3                                                 probe_grid_position;
  float32                                                  hysteresis;
  float32                                                  self_shadow_bias;
  float32                                                  infinite_bounces_multiplier;
  float32                                                  max_probe_offset;
  uint32                                                   probe_debug_flags;
  float32                                                  shadow_weight_power;
  int32                                                    probe_update_per_frame;
  XMINT3                                                   probe_count;
  int32                                                    probe_rays;
  int32                                                    offsets_calculations_count;
  int32                                                    irradiance_side_length;
  int32                                                    visibility_side_length;
} crude_gfx_ddgi_area_cpu;

typedef struct crude_gfx_debug_context
{
  crude_gfx_debug_line_vertex                             *line_vertices_3d;
} crude_gfx_debug_context;

typedef struct crude_gfx_scene_context
{
  crude_gfx_light_cpu                                     *lights;
  crude_gfx_culled_light_cpu                              *culled_lights;
  crude_gfx_mesh_instance_draw                            *meshes_instances_draws;
  XMFLOAT4X4                                              *joint_matrices;
  uint32                                                   absolute_time;
  crude_camera                                             camera;
  XMFLOAT4X4                                               camera_view_to_world;
} crude_gfx_scene_context;

CRUDE_API void
crude_gfx_camera_to_camera_gpu
(
  _In_ crude_camera                                       *camera,
  _In_ XMFLOAT4X4                                          camera_view_to_world,
  _Out_ crude_gfx_camera                                  *camera_gpu
);

CRUDE_API uint32
crude_gfx_ddgi_area_cpu_calculate_probes_count
(
  _In_ crude_gfx_ddgi_area_cpu const                      *area
);

CRUDE_API uint32
crude_gfx_ddgi_area_cpu_calculate_irradiance_side_length_with_borders
(
  _In_ crude_gfx_ddgi_area_cpu const                      *area
);

CRUDE_API uint32
crude_gfx_ddgi_area_cpu_calculate_visibility_side_length_with_borders
(
  _In_ crude_gfx_ddgi_area_cpu const                      *area
);

CRUDE_API uint32
crude_gfx_ddgi_area_cpu_calculate_irradiance_atlas_width
(
  _In_ crude_gfx_ddgi_area_cpu const                      *area
);

CRUDE_API uint32
crude_gfx_ddgi_area_cpu_calculate_irradiance_atlas_height
(
  _In_ crude_gfx_ddgi_area_cpu const                      *area
);

CRUDE_API uint32
crude_gfx_ddgi_area_cpu_calculate_visibility_atlas_width
(
  _In_ crude_gfx_ddgi_area_cpu const                      *area
);

CRUDE_API uint32
crude_gfx_ddgi_area_cpu_calculate_visibility_atlas_height
(
  _In_ crude_gfx_ddgi_area_cpu const                      *area
);