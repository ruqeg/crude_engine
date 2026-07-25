#include "engine/graphics/gpu_device.h"
#include <engine/graphics/shaders/ddgi.crude_shader>

#include <engine/graphics/passes/frame_data_upload_pass.h>

static void
crude_gfx_frame_data_upload_pass_queue_ddgi_constant_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
);

static void
crude_gfx_frame_data_upload_pass_queue_scene_constant_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
);

static void
crude_gfx_frame_data_upload_pass_queue_lights_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
);

static void
crude_gfx_frame_data_upload_pass_queue_shadows_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
);

static void
crude_gfx_frame_data_upload_pass_queue_mesh_instances_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
);

static void
crude_gfx_frame_data_upload_pass_queue_joints_matrices_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
);

static void
crude_gfx_frame_data_upload_pass_queue_meshes_indirect_counnt_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
);

static void
crude_gfx_frame_data_upload_pass_queue_debug_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
);

void
crude_gfx_frame_data_upload_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  CRUDE_GFX_FGN_UTILS();
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_frame_data_upload_pass_context );

  crude_gfx_frame_data_upload_pass_queue_ddgi_constant_( node, primary_cmd, ctx );
  crude_gfx_frame_data_upload_pass_queue_scene_constant_( node, primary_cmd, ctx );
  crude_gfx_frame_data_upload_pass_queue_lights_( node, primary_cmd, ctx );
  crude_gfx_frame_data_upload_pass_queue_shadows_( node, primary_cmd, ctx );
  crude_gfx_frame_data_upload_pass_queue_mesh_instances_( node, primary_cmd, ctx );
  crude_gfx_frame_data_upload_pass_queue_joints_matrices_( node, primary_cmd, ctx );
  crude_gfx_frame_data_upload_pass_queue_meshes_indirect_counnt_( node, primary_cmd, ctx );
  crude_gfx_frame_data_upload_pass_queue_debug_( node, primary_cmd, ctx );
}

void
crude_gfx_frame_data_upload_pass_queue_ddgi_constant_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
)
{
  crude_gfx_device                                        *gpu;
  crude_gfx_ddgi_constants                                *ddgi_constants;
  crude_gfx_memory_allocation                              ddgi_constants_tca;

  CRUDE_GFX_FGN_MEMORY( ddgi_hga );

  gpu = node->graph->gpu;

  ddgi_constants_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, sizeof( crude_gfx_ddgi_constants ) );
  ddgi_constants = CRUDE_CAST( crude_gfx_ddgi_constants*, ddgi_constants_tca.cpu_address );

  *ddgi_constants = CRUDE_COMPOUNT_EMPTY( crude_gfx_ddgi_constants );
  ddgi_constants->probe_counts.x = ctx->ddgi_area.probe_count.x;
  ddgi_constants->probe_counts.y = ctx->ddgi_area.probe_count.y;
  ddgi_constants->probe_counts.z = ctx->ddgi_area.probe_count.z;
  ddgi_constants->probe_rays = ctx->ddgi_area.probe_rays;
  ddgi_constants->hysteresis = ctx->ddgi_area.hysteresis;
  ddgi_constants->self_shadow_bias = ctx->ddgi_area.self_shadow_bias;
  ddgi_constants->probe_grid_position = ctx->ddgi_area.probe_grid_position;
  ddgi_constants->max_probe_offset = ctx->ddgi_area.max_probe_offset;
  ddgi_constants->infinite_bounces_multiplier = ctx->ddgi_area.infinite_bounces_multiplier;
  ddgi_constants->probe_spacing = ctx->ddgi_area.probe_spacing;
  ddgi_constants->probe_update_per_frame = ctx->ddgi_area.probe_update_per_frame;
  ddgi_constants->reciprocal_probe_spacing = CRUDE_COMPOUNT( XMFLOAT3, { 1.f / ctx->ddgi_area.probe_spacing.x, 1.f / ctx->ddgi_area.probe_spacing.y, 1.f / ctx->ddgi_area.probe_spacing.z } );
  ddgi_constants->shadow_weight_power = ctx->ddgi_area.shadow_weight_power;
  ddgi_constants->probe_update_offset = ctx->ddgi_probe_update_offset;
  ddgi_constants->irradiance_side_length = ctx->ddgi_area.irradiance_side_length;
  ddgi_constants->visibility_side_length = ctx->ddgi_area.visibility_side_length;
  ddgi_constants->irradiance_texture_width = crude_gfx_ddgi_area_cpu_calculate_visibility_atlas_width( &ctx->ddgi_area );
  ddgi_constants->irradiance_texture_height = crude_gfx_ddgi_area_cpu_calculate_visibility_atlas_height( &ctx->ddgi_area );
  ddgi_constants->visibility_texture_width = crude_gfx_ddgi_area_cpu_calculate_visibility_atlas_width( &ctx->ddgi_area );
  ddgi_constants->visibility_texture_height = crude_gfx_ddgi_area_cpu_calculate_visibility_atlas_height( &ctx->ddgi_area );
  XMStoreFloat4x4( &ddgi_constants->random_rotation, XMMatrixRotationRollPitchYaw( ctx->ddgi_rotation_scaler * crude_random_unit_f32( ), ctx->ddgi_rotation_scaler * crude_random_unit_f32( ), ctx->ddgi_rotation_scaler * crude_random_unit_f32( ) ) ); //XMStoreFloat4x4( &ddgi_mapped_data->random_rotation, XMMatrixRotationAxis( XMVector3Normalize( XMVectorSet( crude_random_unit_f32( ), crude_random_unit_f32( ), crude_random_unit_f32( ), 1.0 ) ), crude_random_unit_f32( ) * XM_2PI ) );//get_random_value( -1,1 ) * rotation_scaler, get_random_value( -1,1 ) * rotation_scaler, get_random_value( -1,1 ) * rotation_scaler ) );

  crude_gfx_cmd_memory_copy( primary_cmd, ddgi_constants_tca, ddgi_hga, 0, 0 );
}

void
crude_gfx_frame_data_upload_pass_queue_scene_constant_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
)
{
  crude_gfx_device                                        *gpu;
  crude_gfx_scene                                         *scene;
  crude_gfx_memory_allocation                              scene_tca;

  CRUDE_GFX_FGN_MEMORY( scene_hga );

  CRUDE_GFX_FG_TEXTURE_OPTIONAL( indirect_radiance_texture );
  CRUDE_GFX_FG_TEXTURE( culled_tetrahedron_shadow_texture );

  gpu = node->graph->gpu;

  CRUDE_ASSERT( culled_tetrahedron_shadow_texture );

  scene_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, sizeof( crude_gfx_scene ) );
  scene = CRUDE_CAST( crude_gfx_scene*, scene_tca.cpu_address );

  *scene = CRUDE_COMPOUNT_EMPTY( crude_gfx_scene );
  scene->flags = 0u;
  scene->camera_previous = scene->camera;
  scene->resolution.x = gpu->renderer_size.x;
  scene->resolution.y = gpu->renderer_size.y;
  scene->resolution_ratio = CRUDE_CAST( float32, gpu->renderer_size.x ) / gpu->renderer_size.y;
  scene->meshes_instances_count = CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws );
  scene->culled_lights_count = CRUDE_ARRAY_LENGTH( ctx->scene.culled_lights );
  scene->total_lights_count = CRUDE_ARRAY_LENGTH( ctx->scene.lights );
  scene->culled_tiled_shadowmap_texture_index = culled_tetrahedron_shadow_texture->handle.index;
  scene->inv_shadow_map_size.x = 1.f / CRUDE_GFX_TETRAHEDRON_SHADOWMAP_SIZE;
  scene->inv_shadow_map_size.y = 1.f / CRUDE_GFX_TETRAHEDRON_SHADOWMAP_SIZE;
  scene->indirect_light_texture_index = indirect_radiance_texture ? indirect_radiance_texture->handle.index : -1;
  scene->background_radiance.x = ctx->world_environment.background_color.x * ctx->world_environment.background_intencity;
  scene->background_radiance.y = ctx->world_environment.background_color.y * ctx->world_environment.background_intencity;
  scene->background_radiance.z = ctx->world_environment.background_color.z * ctx->world_environment.background_intencity;
  scene->ambient_radiance.x = ctx->world_environment.ambient_color.x * ctx->world_environment.ambient_intencity;
  scene->ambient_radiance.y = ctx->world_environment.ambient_color.y * ctx->world_environment.ambient_intencity;
  scene->ambient_radiance.z = ctx->world_environment.ambient_color.z * ctx->world_environment.ambient_intencity;
  scene->absolute_time = ctx->scene.absolute_time;
  scene->absolute_frame = gpu->absolute_frame;
  crude_gfx_camera_to_camera_gpu( &ctx->scene.camera, ctx->scene.camera_view_to_world, &scene->camera );

  crude_gfx_cmd_memory_copy( primary_cmd, scene_tca, scene_hga, 0, 0 );
}

void
crude_gfx_frame_data_upload_pass_queue_lights_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
)
{
  crude_gfx_device                                        *gpu;

  CRUDE_GFX_FGN_MEMORY( culled_lights_hga );
  CRUDE_GFX_FGN_MEMORY( total_lights_hga );

  gpu = node->graph->gpu;

  if ( CRUDE_ARRAY_LENGTH( ctx->scene.culled_lights ) )
  {
    crude_gfx_light                                       *culled_lights_gpu;
    crude_gfx_memory_allocation                            culled_lights_tca;

    culled_lights_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, sizeof( crude_gfx_light ) * CRUDE_ARRAY_LENGTH( ctx->scene.culled_lights ) );
    culled_lights_gpu = CRUDE_CAST( crude_gfx_light*, culled_lights_tca.cpu_address );

    for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( ctx->scene.culled_lights ); ++i )
    {
      crude_gfx_culled_light_cpu const                    *culled_light_cpu;
      crude_gfx_light_cpu const                           *light_cpu;

      culled_light_cpu = &ctx->scene.culled_lights[ i ];
      light_cpu = &ctx->scene.lights[ culled_light_cpu->light_index ];

      culled_lights_gpu[ i ] = CRUDE_COMPOUNT_EMPTY( crude_gfx_light );
      culled_lights_gpu[ i ].color = light_cpu->light.color;
      culled_lights_gpu[ i ].intensity = light_cpu->light.intensity;
      culled_lights_gpu[ i ].world_position = light_cpu->translation;
      culled_lights_gpu[ i ].radius = light_cpu->light.radius;
    }

    crude_gfx_cmd_memory_copy( primary_cmd, culled_lights_tca, culled_lights_hga, 0, 0 );
  }

  if ( CRUDE_ARRAY_LENGTH( ctx->scene.lights ) )
  {
    crude_gfx_light                                       *total_lights_gpu;
    crude_gfx_memory_allocation                            total_lights_tca;

    total_lights_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, sizeof( crude_gfx_light ) * CRUDE_ARRAY_LENGTH( ctx->scene.lights ) );
    total_lights_gpu = CRUDE_CAST( crude_gfx_light*, total_lights_tca.cpu_address );

    for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( ctx->scene.lights ); ++i )
    {
      crude_gfx_light_cpu const                           *light_cpu;

      light_cpu = &ctx->scene.lights[ i ];

      total_lights_gpu[ i ] = CRUDE_COMPOUNT_EMPTY( crude_gfx_light );
      total_lights_gpu[ i ].color = light_cpu->light.color;
      total_lights_gpu[ i ].intensity = light_cpu->light.intensity;
      total_lights_gpu[ i ].world_position = light_cpu->translation;
      total_lights_gpu[ i ].radius = light_cpu->light.radius;
    }

    crude_gfx_cmd_memory_copy( primary_cmd, total_lights_tca, total_lights_hga, 0, 0 );
  }
}

/* GPU Pro 6 Tile-Based Omnidirectional Shadows Hawar Doghramachi, mixed with http://www.hd-prg.com/tileBasedShadows.html */
void
crude_gfx_frame_data_upload_pass_queue_shadows_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
)
{
  XMFLOAT4                                                *pointlight_spheres;
  XMFLOAT4X4                                              *pointlight_world_to_clip;
  crude_gfx_memory_allocation                              pointlight_spheres_tca;
  crude_gfx_memory_allocation                              pointlight_world_to_clip_tca;
  float32                                                  fov0, fov1;

  CRUDE_GFX_FGN_MEMORY( culled_lights_world_to_texture_hga );
  CRUDE_GFX_FGN_MEMORY( culled_pointlight_spheres_hga );

  fov0 = 143.98570868f + 1.99273682f;
  fov1 = 125.26438968f + 2.78596497f;

  pointlight_spheres_tca = crude_gfx_linear_allocator_allocate( &node->graph->gpu->frame_linear_allocator, culled_pointlight_spheres_hga.size );
  pointlight_spheres = CRUDE_CAST( XMFLOAT4*, pointlight_spheres_tca.cpu_address );

  pointlight_world_to_clip_tca = crude_gfx_linear_allocator_allocate( &node->graph->gpu->frame_linear_allocator, culled_lights_world_to_texture_hga.size );
  pointlight_world_to_clip = CRUDE_CAST( XMFLOAT4X4*, pointlight_world_to_clip_tca.cpu_address );

  CRUDE_ASSERT( pointlight_spheres && pointlight_world_to_clip );
  if ( pointlight_spheres && pointlight_world_to_clip )
  {
    XMMATRIX                                             clip_to_face_clip[ 4 ];
    XMMATRIX                                             view_to_faceed_view[ 4 ];
    XMMATRIX                                             x_rot_matrix, y_rot_matrix, z_rot_matrix;

    x_rot_matrix = XMMatrixRotationX( XMConvertToRadians( 27.36780516f ) );
    y_rot_matrix = XMMatrixRotationY( XMConvertToRadians( 0.f ) );
    z_rot_matrix = XMMatrixRotationZ( XMConvertToRadians( 0.f ) );
    view_to_faceed_view[ 0 ] = XMMatrixMultiply( XMMatrixMultiply( y_rot_matrix, x_rot_matrix ), z_rot_matrix );
    x_rot_matrix = XMMatrixRotationX( XMConvertToRadians( 27.36780516f ) );
    y_rot_matrix = XMMatrixRotationY( XMConvertToRadians( 180.f ) );
    z_rot_matrix = XMMatrixRotationZ( XMConvertToRadians( 90.f ) );
    view_to_faceed_view[ 1 ] = XMMatrixMultiply( XMMatrixMultiply( y_rot_matrix, x_rot_matrix ), z_rot_matrix );
    y_rot_matrix = XMMatrixRotationY( XMConvertToRadians( 90.f ) );
    x_rot_matrix = XMMatrixRotationX( XMConvertToRadians( -27.36780516f ) );
    view_to_faceed_view[ 2 ] = XMMatrixMultiply( y_rot_matrix, x_rot_matrix );
    x_rot_matrix = XMMatrixRotationX( XMConvertToRadians( -27.36780516f ) );
    y_rot_matrix = XMMatrixRotationY( XMConvertToRadians( 270.f ) );
    z_rot_matrix = XMMatrixRotationZ( XMConvertToRadians( 90.f ) );
    view_to_faceed_view[ 3 ] = XMMatrixMultiply( XMMatrixMultiply( y_rot_matrix, x_rot_matrix ), z_rot_matrix );

    for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( ctx->scene.culled_lights ); ++i )
    {
      crude_gfx_culled_light_cpu const                  *culled_light_cpu;
      crude_gfx_light_cpu const                         *light_cpu;
      crude_light const                                 *light;
      XMMATRIX                                           view_to_clip[ 2 ];
      XMMATRIX                                           world_to_view, world_to_faced_view, world_to_clip;
      float32                                            tile_position_x, tile_position_y;

      culled_light_cpu = &ctx->scene.culled_lights[ i ];
      light_cpu = &ctx->scene.lights[ culled_light_cpu->light_index ];
      light = &light_cpu->light;

      /* from [0;1] to [-1;1] with tile_size offset from begining */
      tile_position_x = -1.f + culled_light_cpu->tile_position.x * 2 + culled_light_cpu->tile_size;
      tile_position_y = 1.f - culled_light_cpu->tile_position.y * 2 - culled_light_cpu->tile_size;

      clip_to_face_clip[ 0 ] = XMMatrixSet(
        culled_light_cpu->tile_size, 0.f , 0.f, 0.f,
        0.f, culled_light_cpu->tile_size * 0.5f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        tile_position_x, tile_position_y - ( culled_light_cpu->tile_size * 0.5f ), 0.f, 1.f
      );
      clip_to_face_clip[ 1 ] = XMMatrixSet(
        culled_light_cpu->tile_size * 0.5f, 0.f, 0.f, 0.f,
        0.f, culled_light_cpu->tile_size, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        tile_position_x + ( culled_light_cpu->tile_size * 0.5f ), tile_position_y, 0.f, 1.f
      );
      clip_to_face_clip[ 2 ] = XMMatrixSet(
        culled_light_cpu->tile_size, 0.f, 0.f, 0.f,
        0.f, culled_light_cpu->tile_size * 0.5f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        tile_position_x, tile_position_y + ( culled_light_cpu->tile_size * 0.5f ), 0.f, 1.f
      );
      clip_to_face_clip[ 3 ] = XMMatrixSet(
        culled_light_cpu->tile_size * 0.5f, 0.f, 0.f, 0.f,
        0.f, culled_light_cpu->tile_size, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        tile_position_x - ( culled_light_cpu->tile_size * 0.5f ), tile_position_y, 0.f, 1.f
      );

      XMStoreFloat4( &pointlight_spheres[ i ], XMVectorSet( light_cpu->translation.x, light_cpu->translation.y, light_cpu->translation.z, light->radius ) );

      view_to_clip[ 0 ] = XMMatrixPerspectiveFovRH( XMConvertToRadians( fov1 ), tanf( XMConvertToRadians( fov0 ) * 0.5f ) / tanf( XMConvertToRadians( fov1 ) * 0.5f ), CRUDE_LIGHT_NEARZ, light->radius );
      view_to_clip[ 1 ] = XMMatrixPerspectiveFovRH( XMConvertToRadians( fov0 ), tanf( XMConvertToRadians( fov1 ) * 0.5f ) / tanf( XMConvertToRadians( fov0 ) * 0.5f ), CRUDE_LIGHT_NEARZ, light->radius );

      world_to_view = XMMatrixTranslation( -1.f * light_cpu->translation.x, -1.f * light_cpu->translation.y, -1.f * light_cpu->translation.z );

      world_to_faced_view = XMMatrixMultiply( world_to_view, view_to_faceed_view[ 0 ] );
      world_to_clip = XMMatrixMultiply( world_to_faced_view, view_to_clip[ 0 ] );
      XMStoreFloat4x4( &pointlight_world_to_clip[ i * 4 + 0 ], XMMatrixMultiply( world_to_clip, clip_to_face_clip[ 0 ] ) );

      world_to_faced_view = XMMatrixMultiply( world_to_view, view_to_faceed_view[ 1 ] );
      world_to_clip = XMMatrixMultiply( world_to_faced_view, view_to_clip[ 1 ] );
      XMStoreFloat4x4( &pointlight_world_to_clip[ i * 4 + 1], XMMatrixMultiply( world_to_clip, clip_to_face_clip[ 1 ] ) );

      world_to_faced_view = XMMatrixMultiply( world_to_view, view_to_faceed_view[ 2 ] );
      world_to_clip = XMMatrixMultiply( world_to_faced_view, view_to_clip[ 0 ] );
      XMStoreFloat4x4( &pointlight_world_to_clip[ i * 4 + 2 ], XMMatrixMultiply( world_to_clip, clip_to_face_clip[ 2 ] ) );

      world_to_faced_view = XMMatrixMultiply( world_to_view, view_to_faceed_view[ 3 ] );
      world_to_clip = XMMatrixMultiply( world_to_faced_view, view_to_clip[ 1 ] );
      XMStoreFloat4x4( &pointlight_world_to_clip[ i * 4 + 3 ], XMMatrixMultiply( world_to_clip, clip_to_face_clip[ 3 ] ) );
    }

    crude_gfx_cmd_memory_copy( primary_cmd, pointlight_world_to_clip_tca, culled_lights_world_to_texture_hga, 0, 0 );
    crude_gfx_cmd_memory_copy( primary_cmd, pointlight_spheres_tca, culled_pointlight_spheres_hga, 0, 0 );
  }
}

void
crude_gfx_frame_data_upload_pass_queue_mesh_instances_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
)
{
  crude_gfx_device                                        *gpu;
  crude_gfx_mesh_instance_draw                            *meshes_instances_draws;
  crude_gfx_memory_allocation                              meshes_instances_draws_tca;

  CRUDE_GFX_FGN_MEMORY( meshes_instances_draws_hga );

  gpu = node->graph->gpu;

  meshes_instances_draws_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, sizeof( crude_gfx_mesh_instance_draw ) * CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws ) );
  meshes_instances_draws = CRUDE_CAST( crude_gfx_mesh_instance_draw*, meshes_instances_draws_tca.cpu_address );

  for ( uint32 i = 0u; i < CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws ); ++i )
  {
    meshes_instances_draws[ i ] = ctx->scene.meshes_instances_draws[ i ];
  }

  if ( CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws ) )
  {
    crude_gfx_cmd_memory_copy( primary_cmd, meshes_instances_draws_tca, meshes_instances_draws_hga, 0, 0 );
  }
}

void
crude_gfx_frame_data_upload_pass_queue_joints_matrices_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
)
{
  crude_gfx_device                                        *gpu;
  XMFLOAT4X4                                              *joint_matrices;
  crude_gfx_memory_allocation                              joint_matrices_tca;

  CRUDE_GFX_FGN_MEMORY( joint_matrices_hga );

  gpu = node->graph->gpu;

  joint_matrices_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, sizeof( XMFLOAT4X4 ) * CRUDE_ARRAY_LENGTH( ctx->scene.joint_matrices ) );
  joint_matrices = CRUDE_CAST( XMFLOAT4X4*, joint_matrices_tca.cpu_address );

  for ( uint32 i = 0u; i < CRUDE_ARRAY_LENGTH( ctx->scene.joint_matrices ); ++i )
  {
    joint_matrices[ i ] = ctx->scene.joint_matrices[ i ];
  }

  if ( CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws ) )
  {
    crude_gfx_cmd_memory_copy( primary_cmd, joint_matrices_tca, joint_matrices_hga, 0, 0 );
  }
}

void
crude_gfx_frame_data_upload_pass_queue_meshes_indirect_counnt_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
)
{
  crude_gfx_mesh_draw_count                               *mesh_draw_counts;
  crude_gfx_memory_allocation                              mesh_task_indirect_count_tca;

  CRUDE_GFX_FGN_UTILS( );
  CRUDE_GFX_FGN_MEMORY( mesh_task_indirect_count_hga );
  CRUDE_GFX_FG_GLOBAL_TEXTURE( depth_pyramid_farthest_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );

  mesh_task_indirect_count_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, sizeof( crude_gfx_mesh_draw_count ) );
  mesh_draw_counts = CRUDE_CAST( crude_gfx_mesh_draw_count*, mesh_task_indirect_count_tca.cpu_address );

  *mesh_draw_counts = CRUDE_COMPOUNT_EMPTY( crude_gfx_mesh_draw_count );
  mesh_draw_counts->opaque_mesh_visible_early_count = 0u;
  mesh_draw_counts->opaque_mesh_culled_count = 0u;
  mesh_draw_counts->opaque_mesh_visible_late_count = 0u;
  mesh_draw_counts->transparent_mesh_visible_count = 0u;
  mesh_draw_counts->transparent_mesh_culled_count = 0u;
  mesh_draw_counts->total_mesh_count = CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws );
  mesh_draw_counts->depth_pyramid_farthest_texture_index = depth_pyramid_farthest_texture->handle.index;

  crude_gfx_cmd_memory_copy( primary_cmd, mesh_task_indirect_count_tca, mesh_task_indirect_count_hga, 0, 0 );
}

void
crude_gfx_frame_data_upload_pass_queue_debug_
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd,
  _In_ crude_gfx_frame_data_upload_pass_context           *ctx
)
{
  crude_gfx_device                                        *gpu;
  crude_gfx_debug_line_vertex                            *debug_line_vertices;
  crude_gfx_debug_counts                                 *debug_draw_command;
  crude_gfx_memory_allocation                             debug_line_vertices_tca;
  crude_gfx_memory_allocation                             debug_draw_command_tca;

  CRUDE_GFX_FGN_MEMORY( debug_line_vertices_hga );
  CRUDE_GFX_FGN_MEMORY( debug_commands_hga );

  gpu = node->graph->gpu;

  if ( CRUDE_ARRAY_LENGTH( ctx->debug.line_vertices_3d ) > 0 )
  {

    debug_line_vertices_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, sizeof( crude_gfx_debug_line_vertex ) * CRUDE_ARRAY_LENGTH( ctx->debug.line_vertices_3d ) );
    debug_line_vertices = CRUDE_CAST( crude_gfx_debug_line_vertex*, debug_line_vertices_tca.cpu_address );

    for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( ctx->debug.line_vertices_3d ); ++i )
    {
      debug_line_vertices[ i ] = ctx->debug.line_vertices_3d[ i ];
    }

    crude_gfx_cmd_memory_copy( primary_cmd, debug_line_vertices_tca, debug_line_vertices_hga, 0, 0 );
  }

  debug_draw_command_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, sizeof( crude_gfx_debug_counts ) );
  debug_draw_command = CRUDE_CAST( crude_gfx_debug_counts*, debug_draw_command_tca.cpu_address );
  *debug_draw_command = CRUDE_COMPOUNT_EMPTY( crude_gfx_debug_counts );
  debug_draw_command->debug_lines_2d_instances_count = 1u;
  debug_draw_command->debug_lines_3d_instances_count = 1u;
  debug_draw_command->debug_lines_3d_vertices_count = CRUDE_ARRAY_LENGTH( ctx->debug.line_vertices_3d );
  debug_draw_command->debug_cubes_vertices_count = 36u;
  crude_gfx_cmd_memory_copy( primary_cmd, debug_draw_command_tca, debug_commands_hga, 0, 0 );
}
