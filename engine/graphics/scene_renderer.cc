#include "engine/graphics/frame_graph.h"
#include "engine/graphics/rhi/rhi.h"
#include <TaskScheduler_c.h>
#include <thirdparty/cgltf/cgltf.h>
#include <thirdparty/stb/stb_image.h>
#include <meshoptimizer.h>

#include <engine/core/time.h>
#include <engine/core/profiler.h>
#include <engine/core/array.h>
#include <engine/core/file.h>
#include <engine/core/hashmapstr.h>

#include <engine/audio/audio_ecs.h>
#include <engine/scene/scene_ecs.h>
#include <engine/physics/physics_ecs.h>
#include <engine/scene/scene_debug_ecs.h>
#include <engine/graphics/shaders/common/debug.h>

#include <engine/graphics/gpu_profiler.h>
#include <engine/graphics/passes/frame_data_upload_pass.h>
#include <engine/graphics/passes/culling_pass.h>
#include <engine/graphics/passes/opaque_pass.h>
#include <engine/graphics/passes/ssr_pass.h>
#include <engine/graphics/passes/indirect_light_pass.h>
#include <engine/graphics/passes/depth_pyramid_pass.h>
#include <engine/graphics/passes/debug_pass.h>
#include <engine/graphics/passes/compose_pass.h>
#include <engine/graphics/passes/pointlight_shadow_pass.h>
#include <engine/graphics/passes/postprocessing_pass.h>
#include <engine/graphics/passes/ray_tracing_solid_pass.h>
#include <engine/graphics/passes/indirect_light_debug_pass.h>
#include <engine/graphics/passes/translucent_pass.h>
#include <engine/graphics/passes/opaque_terrain_pass.h>
#include <engine/graphics/passes/volumetric_fog_pass.h>

#define SAMPLE_IRRADIANCE
#define PROBE_UPDATE_IRRADIANCE
#define PROBE_UPDATE_VISIBILITY
#define CALCULATE_PROBE_OFFSETS
#define PROBE_RAYTRACER
#define PROBE_DEBUG
#include <engine/graphics/shaders/ddgi.crude_shader>

#include <engine/graphics/scene_renderer.h>

/**
 * Scene Renderer Other
 */
#if CRUDE_GFX_RAY_TRACING_ENABLED

#if CRUDE_GFX_VULKAN_AVAILABLE
static void
crude_gfx_scene_renderer_fill_top_level_acceleration_structures_instances_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ VkAccelerationStructureInstanceKHR                 *vk_acceleration_structure_instances
);
#endif /* CRUDE_GFX_VULKAN_AVAILABLE */

static void
crude_gfx_scene_renderer_create_top_level_acceleration_structure_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);

static void
crude_gfx_scene_renderer_update_top_level_acceleration_structure_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);
#endif

/**
 * Scene Renderer Utils
 */
static void
crude_scene_renderer_register_nodes_instances_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ crude_ecs                                          *world,
  _In_ crude_entity                                        node
);

static void
crude_scene_renderer_update_lights_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
);

static void
crude_scene_renderer_update_meshes_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
);

/**
 *
 * Renderer Scene Function
 * 
 */
void
crude_gfx_scene_renderer_initialize
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ crude_gfx_scene_renderer_creation                  *creation
)
{
  crude_gfx_buffer_creation                                buffer_creation;
 
  CRUDE_LOG_INFO( CRUDE_CHANNEL_GRAPHICS, "Initialize scene renderer." );

  /* Context */
  scene_renderer->allocator = creation->allocator;
  scene_renderer->gpu = creation->async_loader->gpu;
  scene_renderer->async_loader = creation->async_loader;
  scene_renderer->imgui_context = creation->imgui_context;
  scene_renderer->model_renderer_resources_manager = creation->model_renderer_resources_manager;
  scene_renderer->imgui_pass_enalbed = creation->imgui_pass_enalbed;
  scene_renderer->frame_graph = creation->frame_graph;
#if CRUDE_DEVELOP
  scene_renderer->physics_shapes_manager = creation->physics_shapes_manager;
#endif

  scene_renderer->luminance_avarge_last_update_time = crude_time_now( );

  scene_renderer->ddgi_rotation_scaler = 0.001f;
  scene_renderer->ddgi_probe_update_offset = 0u;
  scene_renderer->ddgi_offsets_calculations_count = 0u;

#if CRUDE_EDITOR
  scene_renderer->ddgi_debug = true;
#else
  scene_renderer->ddgi_debug = false;
#endif
  scene_renderer->ddgi_enabled = false;
  scene_renderer->ddgi_area = CRUDE_COMPOUNT_EMPTY( crude_gfx_ddgi_area_cpu );

  scene_renderer->options = CRUDE_COMPOUNT_EMPTY( crude_gfx_scene_renderer_options );

  scene_renderer->options.gamma = 2.2;
   
  scene_renderer->world_environment = crude_world_environment_empty( );

#if CRUDE_DEVELOP
  scene_renderer->options.debug.debug_mode = CRUDE_SHADER_DEBUG_MODE_NONE;
  scene_renderer->options.debug.flags1 = 0;
  scene_renderer->options.debug.force_roughness = 1;
  scene_renderer->options.debug.force_metalness = 0;
#endif /* CRUDE_DEVELOP */
  
  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( scene_renderer->lights, 0u, crude_heap_allocator_pack( scene_renderer->allocator ) );
  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( scene_renderer->culled_lights, 0u, crude_heap_allocator_pack( scene_renderer->allocator ) );
  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( scene_renderer->model_renderer_resoruces_instances, 0u, crude_heap_allocator_pack( scene_renderer->allocator ) );
  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( scene_renderer->prev_model_renderer_resoruces_instances, 0u, crude_heap_allocator_pack( scene_renderer->allocator ) );
  
#if CRUDE_GFX_RAY_TRACING_ENABLED
  scene_renderer->tlas_instances_hga = crude_gfx_memory_allocation_empty( );
  scene_renderer->tlas_scratch_hga = crude_gfx_memory_allocation_empty( );
  scene_renderer->tlas_hga = crude_gfx_memory_allocation_empty( );
#endif /* CRUDE_GFX_RAY_TRACING_ENABLED */

#if CRUDE_DEVELOP
  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( scene_renderer->debug_model_renderer_resoruces_instances, 0u, crude_heap_allocator_pack( scene_renderer->allocator ) );
  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( scene_renderer->debug_line_vertices_3d, 0u, crude_heap_allocator_pack( scene_renderer->allocator ) );

  crude_gfx_model_renderer_resources_instance_initialize(
    &scene_renderer->light_model_renderer_resources_instance,
    scene_renderer->model_renderer_resources_manager,
    crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_LIGHT_TETRAHEDRON_DEBUG_MODEL_REALTIVE_FILEPATH ) );
  scene_renderer->light_model_renderer_resources_instance.cast_shadow = false;
  scene_renderer->light_model_renderer_resources_instance.rtx_affected = false;
  
  crude_gfx_model_renderer_resources_instance_initialize(
    &scene_renderer->camera_model_renderer_resources_instance,
    scene_renderer->model_renderer_resources_manager,
    crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_CAMERA_DEBUG_MODEL_REALTIVE_FILEPATH ) );
  scene_renderer->camera_model_renderer_resources_instance.cast_shadow = false;
  scene_renderer->camera_model_renderer_resources_instance.rtx_affected = false;
  
  crude_gfx_model_renderer_resources_instance_initialize(
    &scene_renderer->capsule_model_renderer_resources_instance,
    scene_renderer->model_renderer_resources_manager,
    crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_CAPSULE_DEBUG_MODEL_REALTIVE_FILEPATH ) );
  scene_renderer->capsule_model_renderer_resources_instance.cast_shadow = false;
  scene_renderer->capsule_model_renderer_resources_instance.rtx_affected = false;
  
  crude_gfx_model_renderer_resources_instance_initialize(
    &scene_renderer->physics_box_collision_model_renderer_resources_instance,
    scene_renderer->model_renderer_resources_manager,
    crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_PHYSICS_BOX_COLLISION_SHAPE_DEBUG_MODEL_REALTIVE_FILEPATH ) );
  scene_renderer->physics_box_collision_model_renderer_resources_instance.cast_shadow = false;
  scene_renderer->physics_box_collision_model_renderer_resources_instance.rtx_affected = false;
  
  crude_gfx_model_renderer_resources_instance_initialize(
    &scene_renderer->audo_player_model_renderer_resources_instance,
    scene_renderer->model_renderer_resources_manager,
    crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_AUDIO_PLAYER_DEBUG_MODEL_REALTIVE_FILEPATH ) );
  scene_renderer->audo_player_model_renderer_resources_instance.cast_shadow = false;
  scene_renderer->audo_player_model_renderer_resources_instance.rtx_affected = false;
  
  crude_gfx_model_renderer_resources_instance_initialize(
    &scene_renderer->audio_listener_model_renderer_resources_instance,
    scene_renderer->model_renderer_resources_manager,
    crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_AUDIO_LISTENER_DEBUG_MODEL_REALTIVE_FILEPATH ) );
  scene_renderer->audio_listener_model_renderer_resources_instance.cast_shadow = false;
  scene_renderer->audio_listener_model_renderer_resources_instance.rtx_affected = false;
  
  crude_gfx_model_renderer_resources_instance_initialize(
    &scene_renderer->ray_model_renderer_resources_instance,
    scene_renderer->model_renderer_resources_manager,
    crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_RAY_DEBUG_MODEL_REALTIVE_FILEPATH ) );
  scene_renderer->ray_model_renderer_resources_instance.cast_shadow = false;
  scene_renderer->ray_model_renderer_resources_instance.rtx_affected = false;
#endif /* CRUDE_DEVELOP */

  crude_gfx_scene_renderer_on_resize( scene_renderer );

  CRUDE_ARRAY_INITIALIZE_WITH_LENGTH( scene_renderer->joint_matrices, 0, crude_heap_allocator_pack( scene_renderer->allocator ) );
  CRUDE_ARRAY_INITIALIZE_WITH_LENGTH( scene_renderer->meshes_instances_draws, 0, crude_heap_allocator_pack( scene_renderer->allocator ) );
  
  crude_gfx_sampler_creation                               sampler_creation;
  sampler_creation = crude_gfx_sampler_creation_empty();
  sampler_creation.address_mode_u = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.address_mode_v = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.address_mode_w = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.mag_filter = CRUDE_GFX_RHI_FILTER_LINEAR;
  sampler_creation.min_filter = CRUDE_GFX_RHI_FILTER_LINEAR;
  sampler_creation.mip_filter = CRUDE_GFX_RHI_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_creation.reduction_mode = CRUDE_GFX_RHI_SAMPLER_REDUCTION_MODE_MAX;
  sampler_creation.name = CRUDE_STRING_VIEW( "tetrahedron_shadow_sampler" );

  scene_renderer->tetrahedron_shadow_sampler = crude_gfx_create_sampler( scene_renderer->gpu, &sampler_creation );


  ImGuiIO                                                 *imgui_io;
  uint8                                                   *font_pixels;
  int32                                                    font_width, font_height;

  ImGui::SetCurrentContext( CRUDE_CAST( ImGuiContext*, scene_renderer->imgui_context ) );
  imgui_io = &ImGui::GetIO();
  imgui_io->BackendRendererName = "Raptor_ImGui";
  imgui_io->BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

  imgui_io->Fonts->GetTexDataAsRGBA32( &font_pixels, &font_width, &font_height );

  crude_gfx_texture_creation texture_creation = crude_gfx_texture_creation_empty();
  texture_creation.format = CRUDE_GFX_RHI_FORMAT_R8G8B8A8_UNORM;
  texture_creation.type = CRUDE_GFX_TEXTURE_TYPE_TEXTURE_2D;
  texture_creation.initial_data = font_pixels;
  texture_creation.width = font_width;
  texture_creation.height = font_height;
  texture_creation.depth = 1;
  CRUDE_STRING_RAW_COPY( texture_creation.name, "imgui_font" );
  scene_renderer->imgui_font_texture_handle = crude_gfx_create_texture( scene_renderer->gpu, &texture_creation );

  imgui_io->Fonts->TexID = CRUDE_CAST( ImTextureID, &scene_renderer->imgui_font_texture_handle );


  sampler_creation = crude_gfx_sampler_creation_empty();
  sampler_creation.address_mode_u = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.address_mode_v = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.address_mode_w = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.mag_filter = CRUDE_GFX_RHI_FILTER_LINEAR;
  sampler_creation.min_filter = CRUDE_GFX_RHI_FILTER_LINEAR;
  sampler_creation.mip_filter = CRUDE_GFX_RHI_SAMPLER_MIPMAP_MODE_NEAREST;

  sampler_creation.reduction_mode = CRUDE_GFX_RHI_SAMPLER_REDUCTION_MODE_MAX;
  sampler_creation.name = CRUDE_STRING_VIEW( "depth_pyramid_farthest_sampler" );
  scene_renderer->depth_pyramid_farthest_sampler = crude_gfx_create_sampler( scene_renderer->gpu, &sampler_creation );

  sampler_creation.reduction_mode = CRUDE_GFX_RHI_SAMPLER_REDUCTION_MODE_MIN;
  sampler_creation.name = CRUDE_STRING_VIEW( "depth_pyramid_nearest_sampler" );
  scene_renderer->depth_pyramid_nearest_sampler = crude_gfx_create_sampler( scene_renderer->gpu, &sampler_creation );

  sampler_creation = crude_gfx_sampler_creation_empty( );
  sampler_creation.address_mode_u = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.address_mode_v = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.address_mode_w = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.mag_filter = CRUDE_GFX_RHI_FILTER_LINEAR;
  sampler_creation.min_filter = CRUDE_GFX_RHI_FILTER_LINEAR;
  sampler_creation.mip_filter = CRUDE_GFX_RHI_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_creation.name = CRUDE_STRING_VIEW( "probe_grid_sampler_handle" );
  scene_renderer->probe_grid_sampler_handle = crude_gfx_create_sampler( scene_renderer->gpu, &sampler_creation );

  sampler_creation = crude_gfx_sampler_creation_empty();
  sampler_creation.address_mode_u = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.address_mode_v = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.address_mode_w = CRUDE_GFX_RHI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_creation.mag_filter = CRUDE_GFX_RHI_FILTER_LINEAR;
  sampler_creation.min_filter = CRUDE_GFX_RHI_FILTER_LINEAR;
  sampler_creation.mip_filter = CRUDE_GFX_RHI_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_creation.name = CRUDE_STRING_VIEW( "radiance_hierarchy_sampler" );
  scene_renderer->radiance_hierarchy_sampler = crude_gfx_create_sampler( scene_renderer->gpu, &sampler_creation );
}

void
crude_gfx_scene_renderer_deinitialize
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
)
{
  crude_gfx_rhi_wait_idle( &scene_renderer->gpu->rhi_device );

  CRUDE_ARRAY_DEINITIALIZE( scene_renderer->joint_matrices );
  CRUDE_ARRAY_DEINITIALIZE( scene_renderer->meshes_instances_draws );

  crude_gfx_destroy_texture( scene_renderer->gpu, scene_renderer->imgui_font_texture_handle );
  crude_gfx_destroy_sampler( scene_renderer->gpu, scene_renderer->radiance_hierarchy_sampler );
  crude_gfx_destroy_sampler( scene_renderer->gpu, scene_renderer->probe_grid_sampler_handle );
  crude_gfx_destroy_sampler( scene_renderer->gpu, scene_renderer->depth_pyramid_nearest_sampler );
  crude_gfx_destroy_sampler( scene_renderer->gpu, scene_renderer->depth_pyramid_farthest_sampler );

#if CRUDE_DEVELOP
  crude_gfx_model_renderer_resources_instance_deinitialize( &scene_renderer->light_model_renderer_resources_instance );
  crude_gfx_model_renderer_resources_instance_deinitialize( &scene_renderer->camera_model_renderer_resources_instance );
  crude_gfx_model_renderer_resources_instance_deinitialize( &scene_renderer->capsule_model_renderer_resources_instance );
  crude_gfx_model_renderer_resources_instance_deinitialize( &scene_renderer->physics_box_collision_model_renderer_resources_instance );
  crude_gfx_model_renderer_resources_instance_deinitialize( &scene_renderer->audio_listener_model_renderer_resources_instance );
  crude_gfx_model_renderer_resources_instance_deinitialize( &scene_renderer->audo_player_model_renderer_resources_instance );
  crude_gfx_model_renderer_resources_instance_deinitialize( &scene_renderer->ray_model_renderer_resources_instance );

  CRUDE_ARRAY_DEINITIALIZE( scene_renderer->debug_model_renderer_resoruces_instances );
  CRUDE_ARRAY_DEINITIALIZE( scene_renderer->debug_line_vertices_3d );
#endif /* CRUDE_DEVELOP */

#if CRUDE_GFX_RAY_TRACING_ENABLED
  crude_gfx_rhi_destroy_acceleration_structure( &scene_renderer->gpu->rhi_device, scene_renderer->rhi_tlas );
  crude_gfx_memory_deallocate( scene_renderer->gpu, scene_renderer->tlas_instances_hga );
  crude_gfx_memory_deallocate( scene_renderer->gpu, scene_renderer->tlas_scratch_hga );
  crude_gfx_memory_deallocate( scene_renderer->gpu, scene_renderer->tlas_hga );
#endif /* CRUDE_GFX_RAY_TRACING_ENABLED */

  crude_gfx_destroy_sampler( scene_renderer->gpu, scene_renderer->tetrahedron_shadow_sampler );

  CRUDE_ARRAY_DEINITIALIZE( scene_renderer->prev_model_renderer_resoruces_instances );
  CRUDE_ARRAY_DEINITIALIZE( scene_renderer->model_renderer_resoruces_instances );
  CRUDE_ARRAY_DEINITIALIZE( scene_renderer->lights );
  CRUDE_ARRAY_DEINITIALIZE( scene_renderer->culled_lights );

}

bool
crude_gfx_scene_renderer_update_instances_from_node
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ crude_ecs                                          *world,
  _In_ crude_entity                                        main_node
)
{
  bool                                                     should_recreated_tlas, ddgi_enabled_prev, model_initialized;
 
  CRUDE_PROFILER_ZONE_NAME( "crude_gfx_scene_renderer_update_instances_from_node" );

  model_initialized = false;

#if CRUDE_DEVELOP
  /* load debug models in case it was cleaned (look at the model manager resource clean) */
  scene_renderer->light_model_renderer_resources_instance.model_renderer_resources_handle = crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_LIGHT_TETRAHEDRON_DEBUG_MODEL_REALTIVE_FILEPATH );
  scene_renderer->camera_model_renderer_resources_instance.model_renderer_resources_handle = crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_CAMERA_DEBUG_MODEL_REALTIVE_FILEPATH );
  scene_renderer->capsule_model_renderer_resources_instance.model_renderer_resources_handle = crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_CAPSULE_DEBUG_MODEL_REALTIVE_FILEPATH );
  scene_renderer->physics_box_collision_model_renderer_resources_instance.model_renderer_resources_handle = crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_PHYSICS_BOX_COLLISION_SHAPE_DEBUG_MODEL_REALTIVE_FILEPATH );
  scene_renderer->audo_player_model_renderer_resources_instance.model_renderer_resources_handle = crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_AUDIO_PLAYER_DEBUG_MODEL_REALTIVE_FILEPATH );
  scene_renderer->audio_listener_model_renderer_resources_instance.model_renderer_resources_handle = crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_AUDIO_LISTENER_DEBUG_MODEL_REALTIVE_FILEPATH );
  scene_renderer->ray_model_renderer_resources_instance.model_renderer_resources_handle = crude_gfx_model_renderer_resources_manager_get_gltf_model( scene_renderer->model_renderer_resources_manager, CRUDE_RAY_DEBUG_MODEL_REALTIVE_FILEPATH );
#endif /* CRUDE_DEVELOP */
  
  CRUDE_SWAP( scene_renderer->model_renderer_resoruces_instances, scene_renderer->prev_model_renderer_resoruces_instances );
  
  CRUDE_ARRAY_SET_LENGTH( scene_renderer->model_renderer_resoruces_instances, 0u );
  CRUDE_ARRAY_SET_LENGTH( scene_renderer->lights, 0u );
  CRUDE_ARRAY_SET_LENGTH( scene_renderer->culled_lights, 0u );
  
  ddgi_enabled_prev = scene_renderer->ddgi_enabled;

  scene_renderer->ddgi_enabled = false;

  scene_renderer->prev_ddgi_area = scene_renderer->ddgi_area;
  
  scene_renderer->world_environment = crude_world_environment_empty( );

  for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( scene_renderer->debug_model_renderer_resoruces_instances ); ++i )
  {
    CRUDE_ARRAY_PUSH( scene_renderer->model_renderer_resoruces_instances, scene_renderer->debug_model_renderer_resoruces_instances[ i ] );
  }

  crude_scene_renderer_register_nodes_instances_( scene_renderer, world, main_node );

  bool ddgi_probes_count_changed = scene_renderer->ddgi_area.probe_count.x != scene_renderer->prev_ddgi_area.probe_count.x
    || scene_renderer->ddgi_area.probe_count.y != scene_renderer->prev_ddgi_area.probe_count.y 
    || scene_renderer->ddgi_area.probe_count.z != scene_renderer->prev_ddgi_area.probe_count.z;

  bool probes_spaching_changed = scene_renderer->ddgi_area.probe_spacing.x != scene_renderer->prev_ddgi_area.probe_spacing.x
    || scene_renderer->ddgi_area.probe_spacing.y != scene_renderer->prev_ddgi_area.probe_spacing.y 
    || scene_renderer->ddgi_area.probe_spacing.z != scene_renderer->prev_ddgi_area.probe_spacing.z;

  bool probes_grid_changed = scene_renderer->ddgi_area.probe_grid_position.x != scene_renderer->prev_ddgi_area.probe_grid_position.x
    || scene_renderer->ddgi_area.probe_grid_position.y != scene_renderer->prev_ddgi_area.probe_grid_position.y 
    || scene_renderer->ddgi_area.probe_grid_position.z != scene_renderer->prev_ddgi_area.probe_grid_position.z;

  bool ddgi_offsets_reseted = false;
  ddgi_offsets_reseted |= scene_renderer->ddgi_area.max_probe_offset != scene_renderer->prev_ddgi_area.max_probe_offset;
  ddgi_offsets_reseted |= scene_renderer->ddgi_area.offsets_calculations_count != scene_renderer->prev_ddgi_area.offsets_calculations_count;
  ddgi_offsets_reseted |= ddgi_probes_count_changed;
  ddgi_offsets_reseted |= probes_spaching_changed;
  ddgi_offsets_reseted |= probes_grid_changed;
  ddgi_offsets_reseted |= !ddgi_enabled_prev && scene_renderer->ddgi_enabled;
  if ( ddgi_offsets_reseted )
  {
    scene_renderer->ddgi_offsets_calculations_count = scene_renderer->ddgi_area.offsets_calculations_count + 1;
  }

  should_recreated_tlas = false;

  if ( CRUDE_ARRAY_LENGTH( scene_renderer->prev_model_renderer_resoruces_instances ) != CRUDE_ARRAY_LENGTH( scene_renderer->model_renderer_resoruces_instances ) )
  {
    should_recreated_tlas = true;
  }
  else
  {
    for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( scene_renderer->prev_model_renderer_resoruces_instances ); ++i )
    {
      if ( scene_renderer->prev_model_renderer_resoruces_instances[ i ].unique_id != scene_renderer->model_renderer_resoruces_instances[ i ].unique_id )
      {
        should_recreated_tlas = true;
      }
    }
  }
  
#if CRUDE_GFX_RAY_TRACING_ENABLED
  if ( should_recreated_tlas ) 
  {
    if ( scene_renderer->tlas_hga.gpu_address )
    {
      crude_gfx_memory_deallocate( scene_renderer->gpu, scene_renderer->tlas_hga );
      crude_gfx_memory_deallocate( scene_renderer->gpu, scene_renderer->tlas_instances_hga );
      crude_gfx_memory_deallocate( scene_renderer->gpu, scene_renderer->tlas_scratch_hga );
      crude_gfx_rhi_destroy_acceleration_structure( &scene_renderer->gpu->rhi_device, scene_renderer->rhi_tlas );
    }

    crude_gfx_cmd_buffer *cmd = crude_gfx_access_cmd_buffer( scene_renderer->gpu, scene_renderer->gpu->immediate_transfer_cmd_buffer );
    crude_gfx_cmd_begin_primary( cmd );
    crude_gfx_scene_renderer_create_top_level_acceleration_structure_( scene_renderer, cmd );
    crude_gfx_submit_immediate( cmd );
  }
#endif /* CRUDE_GFX_RAY_TRACING_ENABLED */
  
  CRUDE_PROFILER_ZONE_END;
  return should_recreated_tlas | model_initialized;
}

void
crude_gfx_scene_renderer_start_frame
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
)
{
  scene_renderer->primary_cmd = crude_gfx_get_primary_cmd( scene_renderer->gpu, 0, true );
}

void
crude_gfx_scene_renderer_update
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
)
{
  CRUDE_PROFILER_ZONE_NAME( "crude_gfx_scene_renderer_update" );
  CRUDE_GFX_CMD_PUSH_MARKER( scene_renderer->primary_cmd, "update_dynamic_buffers" );
  crude_scene_renderer_update_lights_( scene_renderer );
  crude_scene_renderer_update_meshes_( scene_renderer );
#if CRUDE_GFX_RAY_TRACING_ENABLED
  crude_gfx_scene_renderer_update_top_level_acceleration_structure_( scene_renderer, scene_renderer->primary_cmd );
#endif /* CRUDE_GFX_RAY_TRACING_ENABLED */

  scene_renderer->ddgi_probe_update_offset = ( scene_renderer->ddgi_probe_update_offset + scene_renderer->ddgi_area.probe_update_per_frame ) % crude_gfx_ddgi_area_cpu_calculate_probes_count( &scene_renderer->ddgi_area );
  scene_renderer->ddgi_offsets_calculations_count = CRUDE_MAX( -1, scene_renderer->ddgi_offsets_calculations_count - 1 ); 

  CRUDE_GFX_CMD_POP_MARKER( scene_renderer->primary_cmd );
  CRUDE_PROFILER_ZONE_END;
}

void
crude_gfx_scene_renderer_render
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
)
{
  /********************************************************
   *
   * Main
   *
   ********************************************************/
  crude_gfx_frame_graph                                   *graph;
  XMFLOAT2                                                 resolution;
  XMFLOAT2                                                 depth_hierarchy_resolution, radiance_hierarchy_resolution;
  uint32                                                   depth_pyramid_levels, radiance_hierarchy_levels;
  /********************************************************
   *
   * Common context
   *
   ********************************************************/
  crude_gfx_debug_context                                  debug_context;
  crude_gfx_scene_context                                  scene_context;
  /********************************************************
   *
   * Passes context
   *
   ********************************************************/
  crude_gfx_frame_data_upload_pass_context                 frame_data_upload_pass_context;
  crude_gfx_pointlight_shadow_culling_pass_context         pointlight_shadow_culling_pass_context;
  crude_gfx_pointlight_shadow_commands_generation_pass_context pointlight_shadow_commands_generation_pass_context;
  crude_gfx_pointlight_shadow_draw_pass_context            pointlight_shadow_draw_pass_context;
  crude_gfx_culling_pass_context                           culling_early_pass_context;
  crude_gfx_culling_pass_context                           culling_late_pass_context;
  crude_gfx_opaque_pass_context                            opaque_early_pass_context;
  crude_gfx_opaque_pass_context                            opaque_late_pass_context;
  crude_gfx_depth_pyramid_pass_context                     depth_pyramid_farthest_pass_context;
  crude_gfx_depth_pyramid_pass_context                     depth_pyramid_nearest_pass_context;
  crude_gfx_ssr_pass_context                               ssr_pass_context;
  crude_gfx_compose_pass_context                           compose_pass_context;
  crude_gfx_indirect_light_pass_context                    indirect_light_pass_context;
  crude_gfx_indirect_light_debug_pass_context              indirect_light_debug_pass_context;
  crude_gfx_translucent_pass_context                       translucent_pass_context;
  crude_gfx_luminance_calculation_pass_context             luminance_calculation_pass_context;
  crude_gfx_postprocessing_pass_context                    postprocessing_pass_context;
  crude_gfx_debug_pass_context                             debug_pass_context;
  crude_gfx_imgui_pass_context                             imgui_pass_context;
  crude_gfx_imgui_generate_buffers_pass_context            imgui_generate_buffers_pass_context;
  /********************************************************
   *
   * Other
   *
   ********************************************************/
  crude_gfx_frame_graph_node_creation                      graph_node_creation;
  crude_gfx_frame_graph_memory_creation                    graph_memory_creation;
  crude_gfx_frame_graph_texture_creation                   graph_texture_creation;

  CRUDE_PROFILER_ZONE_NAME( "crude_gfx_scene_renderer_render" );

  graph = scene_renderer->frame_graph;
  resolution = graph->gpu->renderer_size;
  
  crude_gfx_frame_graph_new_frame( graph );

  depth_hierarchy_resolution.x = resolution.x / 2;
  depth_hierarchy_resolution.y = resolution.y / 2;

  radiance_hierarchy_resolution.x = resolution.x / 2;
  radiance_hierarchy_resolution.y = resolution.y / 2;

  {
    uint32                                                 width, height;

    width = depth_hierarchy_resolution.x;
    height = depth_hierarchy_resolution.y;

    depth_pyramid_levels = 0;
    while ( width >= 1 && height >= 1 )
    {
      ++depth_pyramid_levels;

      width = width / 2;
      height = height / 2;
    }

    width = radiance_hierarchy_resolution.x;
    height = radiance_hierarchy_resolution.y;

    radiance_hierarchy_levels = 0;
    while ( width >= 1 && height >= 1 )
    {
      ++radiance_hierarchy_levels;

      width = width / 2;
      height = height / 2;
    }
  }

  /********************************************************
   *
   * Common context
   *
   ********************************************************/
  debug_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_debug_context );
  debug_context.line_vertices_3d = scene_renderer->debug_line_vertices_3d;

  scene_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_scene_context );
  scene_context.lights = scene_renderer->lights;
  scene_context.culled_lights = scene_renderer->culled_lights;
  scene_context.meshes_instances_draws = scene_renderer->meshes_instances_draws;
  scene_context.joint_matrices = scene_renderer->joint_matrices;
  scene_context.absolute_time = scene_renderer->options.absolute_time;
  scene_context.camera = scene_renderer->options.camera;
  scene_context.camera_view_to_world = scene_renderer->options.camera_view_to_world;

  /********************************************************
   *
   * Frame Data Upload Pass
   *
   ********************************************************/
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( scene_hga, sizeof( crude_gfx_scene ) );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( ddgi_hga, sizeof( crude_gfx_ddgi_constants ) );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( joint_matrices_hga, sizeof( XMFLOAT4X4 ) * CRUDE_ARRAY_LENGTH( scene_renderer->joint_matrices ) );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( meshes_instances_draws_hga, sizeof( crude_gfx_mesh_instance_draw )  * CRUDE_ARRAY_LENGTH( scene_renderer->meshes_instances_draws ) );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( mesh_task_indirect_count_hga, sizeof( crude_gfx_mesh_draw_count ) );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( debug_line_vertices_hga, sizeof( crude_gfx_debug_line_vertex ) * CRUDE_GFX_SCENE_RENDERER_MAX_DEBUG_LINES * 2u );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( debug_commands_hga, sizeof( crude_gfx_debug_counts ) );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( culled_lights_hga, sizeof( crude_gfx_light ) * CRUDE_LIGHTS_MAX_COUNT );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( total_lights_hga, sizeof( crude_gfx_light ) * CRUDE_LIGHTS_MAX_COUNT );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( culled_pointlight_spheres_hga, sizeof( XMUINT4 ) * CRUDE_LIGHTS_MAX_COUNT );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( culled_lights_world_to_texture_hga, sizeof( XMFLOAT4X4 ) * CRUDE_LIGHTS_MAX_COUNT * 4u );

  frame_data_upload_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_data_upload_pass_context );
  frame_data_upload_pass_context.ddgi_area = scene_renderer->ddgi_area;
  frame_data_upload_pass_context.ddgi_probe_update_offset = scene_renderer->ddgi_probe_update_offset;
  frame_data_upload_pass_context.ddgi_rotation_scaler = scene_renderer->ddgi_rotation_scaler;
  frame_data_upload_pass_context.world_environment = scene_renderer->world_environment;
  frame_data_upload_pass_context.scene = scene_context;
  frame_data_upload_pass_context.debug = debug_context;
  CRUDE_GFX_FGNC_INIT( "frame_data_upload", frame_data_upload_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_frame_data_upload_pass_queue );
  CRUDE_GFX_FGNC_OUT_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( ddgi_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( meshes_instances_draws_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( joint_matrices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( mesh_task_indirect_count_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( debug_line_vertices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( debug_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( culled_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( total_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( culled_lights_world_to_texture_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( culled_pointlight_spheres_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Point Light Shadow Pass
   *
   ********************************************************/
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( culled_pointshadow_meshletes_instances_hga, sizeof( XMUINT2 ) * CRUDE_MAX_MESHLETS_PER_LIGHT * CRUDE_LIGHTS_MAX_COUNT );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( culled_pointshadow_meshletes_instances_count_hga, sizeof( uint32 ) * ( CRUDE_LIGHTS_MAX_COUNT + 1u ) );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( culled_pointshadow_meshlet_draw_commands_hga, sizeof( XMUINT4 ) * ( CRUDE_LIGHTS_MAX_COUNT * 4u ) );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    culled_tetrahedron_shadow_texture,
    CRUDE_GFX_TETRAHEDRON_SHADOWMAP_SIZE, CRUDE_GFX_TETRAHEDRON_SHADOWMAP_SIZE,
    CRUDE_GFX_RHI_FORMAT_D32_SFLOAT,
    scene_renderer->tetrahedron_shadow_sampler );

  pointlight_shadow_culling_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_pointlight_shadow_culling_pass_context );
  pointlight_shadow_culling_pass_context.scene = scene_context;
  CRUDE_GFX_FGNC_INIT( "pointlight_shadow_culling", pointlight_shadow_culling_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_pointlight_shadow_culling_pass_queue );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( meshes_instances_draws_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_OUT_MEMORY( culled_pointshadow_meshletes_instances_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_OUT_MEMORY( culled_pointshadow_meshletes_instances_count_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_SUBMIT;

  pointlight_shadow_commands_generation_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_pointlight_shadow_commands_generation_pass_context );
  pointlight_shadow_commands_generation_pass_context.culled_lights_count = CRUDE_ARRAY_LENGTH( scene_renderer->culled_lights );
  CRUDE_GFX_FGNC_INIT( "pointlight_shadow_commands_generation", pointlight_shadow_commands_generation_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_pointlight_shadow_commands_generation_pass_queue );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_pointshadow_meshletes_instances_count_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_OUT_MEMORY( culled_pointshadow_meshlet_draw_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_SUBMIT;

  pointlight_shadow_draw_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_pointlight_shadow_draw_pass_context );
  pointlight_shadow_draw_pass_context.culled_lights = scene_renderer->culled_lights;
  pointlight_shadow_draw_pass_context.lights = scene_renderer->lights;
  CRUDE_GFX_FGNC_INIT( "pointlight_shadow_draw", pointlight_shadow_draw_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_pointlight_shadow_draw_pass_queue );
  CRUDE_GFX_FGNC_TYPE( CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( meshes_instances_draws_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( joint_matrices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_world_to_texture_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_pointlight_spheres_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_pointshadow_meshletes_instances_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_pointshadow_meshletes_instances_count_hga, CRUDE_GFX_RHI_RESOURCE_STATE_INDIRECT_ARGUMENT );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_pointshadow_meshlet_draw_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_INDIRECT_ARGUMENT );
  CRUDE_GFX_FGNC_OUT_IMAGE( culled_tetrahedron_shadow_texture, CRUDE_GFX_RHI_RESOURCE_STATE_DEPTH_WRITE, { 1.f, 1.f, 1.f, 1.f } );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Culling early pass
   *
   ******************************************s**************/
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( mesh_task_indirect_commands_hga, sizeof( crude_gfx_mesh_draw_command ) * CRUDE_ARRAY_LENGTH( scene_renderer->meshes_instances_draws ) );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( mesh_task_indirect_commands_culled_hga, sizeof( crude_gfx_mesh_draw_command ) * CRUDE_ARRAY_LENGTH( scene_renderer->meshes_instances_draws ) );

  culling_early_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_culling_pass_context );
  culling_early_pass_context.culling_type = CRUDE_GFX_CULLING_PASS_TYPE_EARLY;
  culling_early_pass_context.scene = scene_context;
  CRUDE_GFX_FGNC_INIT( "culling_early_pass", culling_early_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_culling_pass_queue );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( meshes_instances_draws_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( mesh_task_indirect_count_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_line_vertices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_OUT_MEMORY( mesh_task_indirect_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_OUT_MEMORY( mesh_task_indirect_commands_culled_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Opaque early pass
   *
   ********************************************************/
  CRUDE_GFX_FG_CREATE_TARGET_2D(
    gbuffer_radiance_texture,
    resolution.x, resolution.y,
    CRUDE_GFX_RHI_FORMAT_R16G16B16A16_SFLOAT,
    graph->gpu->default_sampler );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    gbuffer_normal_texture,
    resolution.x, resolution.y,
    CRUDE_GFX_RHI_FORMAT_R16G16_SFLOAT,
    graph->gpu->default_sampler );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    gbuffer_roughness_metalness_texture,
    resolution.x, resolution.y,
    CRUDE_GFX_RHI_FORMAT_R8G8B8A8_UNORM,
    graph->gpu->default_sampler );

  CRUDE_GFX_FG_CREATE_RENDER_TARGET_2D(
    gbuffer_albedo_texture,
    resolution.x, resolution.y,
    CRUDE_GFX_RHI_FORMAT_R8G8B8A8_SRGB,
    graph->gpu->default_sampler );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    depth_texture,
    resolution.x, resolution.y,
    CRUDE_GFX_RHI_FORMAT_D32_SFLOAT,
    graph->gpu->default_sampler );

  opaque_early_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_opaque_pass_context );
  opaque_early_pass_context.tlas_address = crude_gfx_rhi_get_acceleration_structure_device_address( &scene_renderer->gpu->rhi_device, scene_renderer->rhi_tlas );
  opaque_early_pass_context.geometry_type = CRUDE_GFX_OPAQUE_PASS_GEOMETRY_TYPE_MESHLET;
  opaque_early_pass_context.type = CRUDE_GFX_OPAQUE_PASS_TYPE_EARLY;
  opaque_early_pass_context.scene = scene_context;
  CRUDE_GFX_FGNC_INIT( "opaque_early_pass", opaque_early_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_opaque_pass_queue );
  CRUDE_GFX_FGNC_TYPE( CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( meshes_instances_draws_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( joint_matrices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_world_to_texture_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( mesh_task_indirect_count_hga, CRUDE_GFX_RHI_RESOURCE_STATE_INDIRECT_ARGUMENT );
  CRUDE_GFX_FGNC_IN_MEMORY( mesh_task_indirect_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_INDIRECT_ARGUMENT );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_line_vertices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_OUT_IMAGE( gbuffer_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( gbuffer_normal_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( gbuffer_roughness_metalness_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( gbuffer_albedo_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( depth_texture, CRUDE_GFX_RHI_RESOURCE_STATE_DEPTH_WRITE, { 1.f, 1.f, 1.f, 1.f } );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Depth pyramid farthest pass
   *
   ********************************************************/
  CRUDE_GFX_FG_CREATE_TARGET_2D_MIPS(
    depth_pyramid_farthest_texture,
    depth_hierarchy_resolution.x, depth_hierarchy_resolution.y,
    CRUDE_GFX_RHI_FORMAT_R32_SFLOAT,
    depth_pyramid_levels,
    scene_renderer->depth_pyramid_farthest_sampler );

  depth_pyramid_farthest_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_depth_pyramid_pass_context );
  depth_pyramid_farthest_pass_context.type = CRUDE_GFX_DEPTH_PYRAMID_PASS_TYPE_FARTHEST;
  CRUDE_GFX_FGNC_INIT( "depth_pyramid_farthest_pass", depth_pyramid_farthest_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_depth_pyramid_pass_queue );
  CRUDE_GFX_FGNC_IN_IMAGE( depth_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_OUT_IMAGE( depth_pyramid_farthest_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, { 1.f, 1.f, 1.f, 1.f } );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Culling late pass
   *
   ********************************************************/
  culling_late_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_culling_pass_context );
  culling_late_pass_context.culling_type = CRUDE_GFX_CULLING_PASS_TYPE_LATE;
  culling_late_pass_context.scene = scene_context;
  CRUDE_GFX_FGNC_INIT( "culling_late_pass", culling_late_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_culling_pass_queue );
  CRUDE_GFX_FGNC_IN_IMAGE( depth_pyramid_farthest_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( meshes_instances_draws_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( mesh_task_indirect_count_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_line_vertices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( mesh_task_indirect_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( mesh_task_indirect_commands_culled_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Opaque late pass
   *
   ********************************************************/
  opaque_late_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_opaque_pass_context );
  opaque_late_pass_context.tlas_address = crude_gfx_rhi_get_acceleration_structure_device_address( &scene_renderer->gpu->rhi_device, scene_renderer->rhi_tlas );
  opaque_late_pass_context.geometry_type = CRUDE_GFX_OPAQUE_PASS_GEOMETRY_TYPE_MESHLET;
  opaque_late_pass_context.type = CRUDE_GFX_OPAQUE_PASS_TYPE_LATE;
  opaque_late_pass_context.scene = scene_context;
  CRUDE_GFX_FGNC_INIT( "opaque_late_pass", opaque_late_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_opaque_pass_queue );
  CRUDE_GFX_FGNC_TYPE( CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS );
  CRUDE_GFX_FGNC_IN_MARKER( culling_late_pass );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( meshes_instances_draws_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( joint_matrices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_world_to_texture_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( mesh_task_indirect_count_hga, CRUDE_GFX_RHI_RESOURCE_STATE_INDIRECT_ARGUMENT );
  CRUDE_GFX_FGNC_IN_MEMORY( mesh_task_indirect_commands_culled_hga, CRUDE_GFX_RHI_RESOURCE_STATE_INDIRECT_ARGUMENT );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_line_vertices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_INOUT_IMAGE( gbuffer_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET );
  CRUDE_GFX_FGNC_INOUT_IMAGE( gbuffer_normal_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET );
  CRUDE_GFX_FGNC_INOUT_IMAGE( gbuffer_roughness_metalness_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET );
  CRUDE_GFX_FGNC_INOUT_IMAGE( gbuffer_albedo_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET );
  CRUDE_GFX_FGNC_INOUT_IMAGE( depth_texture, CRUDE_GFX_RHI_RESOURCE_STATE_DEPTH_WRITE );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Depth pyramid nearest pass
   *
   ********************************************************/
  CRUDE_GFX_FG_CREATE_TARGET_2D_MIPS(
    depth_pyramid_nearest_texture,
    depth_hierarchy_resolution.x, depth_hierarchy_resolution.y,
    CRUDE_GFX_RHI_FORMAT_R32_SFLOAT,
    depth_pyramid_levels,
    scene_renderer->depth_pyramid_nearest_sampler );

  depth_pyramid_nearest_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_depth_pyramid_pass_context );
  depth_pyramid_nearest_pass_context.type = CRUDE_GFX_DEPTH_PYRAMID_PASS_TYPE_NEAREST;
  CRUDE_GFX_FGNC_INIT( "depth_pyramid_nearest_pass", depth_pyramid_nearest_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_depth_pyramid_pass_queue );
  CRUDE_GFX_FGNC_IN_MARKER( opaque_late_pass );
  CRUDE_GFX_FGNC_IN_IMAGE( depth_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_OUT_IMAGE( depth_pyramid_nearest_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, { 1.f, 1.f, 1.f, 1.f } );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Screen space reflection pass
   *
   ********************************************************/
  CRUDE_GFX_FG_CREATE_TARGET_2D(
    ssr_hit_uv_depth_rdotv_texture,
    resolution.x * 0.5f, resolution.y * 0.5f,
    CRUDE_GFX_RHI_FORMAT_R8G8B8A8_UNORM,
    graph->gpu->default_sampler );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    ssr_radiance_texture,
    resolution.x * 0.5f, resolution.y * 0.5f,
    CRUDE_GFX_RHI_FORMAT_R8G8B8A8_UNORM,
    graph->gpu->default_sampler );

  CRUDE_GFX_FG_CREATE_TARGET_2D_MIPS(
    ssr_radiance_hierarchy_texture,
    radiance_hierarchy_resolution.x, radiance_hierarchy_resolution.y,
    CRUDE_GFX_RHI_FORMAT_R16G16B16A16_SFLOAT,
    radiance_hierarchy_levels,
    scene_renderer->radiance_hierarchy_sampler );

  CRUDE_GFX_FG_CREATE_TARGET_2D_MIPS(
    ssr_radiance_hierarchy_temporary_texture,
    radiance_hierarchy_resolution.x / 2, radiance_hierarchy_resolution.y / 2,
    CRUDE_GFX_RHI_FORMAT_R16G16B16A16_SFLOAT,
    radiance_hierarchy_levels - 1,
    scene_renderer->radiance_hierarchy_sampler );

  ssr_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_ssr_pass_context );
  CRUDE_GFX_FGNC_INIT( "ssr_pass", ssr_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_ssr_pass_queue );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( gbuffer_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_SOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( gbuffer_normal_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( gbuffer_roughness_metalness_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( depth_pyramid_nearest_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_OUT_IMAGE( ssr_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( ssr_radiance_hierarchy_texture, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( ssr_radiance_hierarchy_temporary_texture, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( ssr_hit_uv_depth_rdotv_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, { } );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Indirect light pass
   *
   ********************************************************/
  CRUDE_GFX_FG_CREATE_TARGET_2D(
    indirect_radiance_texture,
    resolution.x, resolution.y,
    CRUDE_GFX_RHI_FORMAT_R16G16B16A16_SFLOAT,
    graph->gpu->default_sampler );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    probe_raytrace_radiance_texture,
    scene_renderer->ddgi_area.probe_rays, crude_gfx_ddgi_area_cpu_calculate_probes_count( &scene_renderer->ddgi_area ),
    CRUDE_GFX_RHI_FORMAT_R16G16B16A16_SFLOAT,
    graph->gpu->default_sampler );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    probe_grid_irradiance_texture,
    crude_gfx_ddgi_area_cpu_calculate_irradiance_atlas_width( &scene_renderer->ddgi_area ), crude_gfx_ddgi_area_cpu_calculate_irradiance_atlas_height( &scene_renderer->ddgi_area ),
    CRUDE_GFX_RHI_FORMAT_R16G16B16A16_SFLOAT,
    scene_renderer->probe_grid_sampler_handle );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    probe_grid_visibility_texture,
    crude_gfx_ddgi_area_cpu_calculate_visibility_atlas_width( &scene_renderer->ddgi_area ), crude_gfx_ddgi_area_cpu_calculate_visibility_atlas_height( &scene_renderer->ddgi_area ),
    CRUDE_GFX_RHI_FORMAT_R16G16B16A16_SFLOAT,
    scene_renderer->probe_grid_sampler_handle );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    probe_offsets_texture,
    scene_renderer->ddgi_area.probe_count.x * scene_renderer->ddgi_area.probe_count.y, scene_renderer->ddgi_area.probe_count.z,
    CRUDE_GFX_RHI_FORMAT_R16G16B16A16_SFLOAT,
    graph->gpu->default_sampler );

  indirect_light_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_indirect_light_pass_context );
  indirect_light_pass_context.ddgi_area = scene_renderer->ddgi_area;
  indirect_light_pass_context.ddgi_offsets_calculations_count = scene_renderer->ddgi_offsets_calculations_count;
  indirect_light_pass_context.tlas_address = crude_gfx_rhi_get_acceleration_structure_device_address( &scene_renderer->gpu->rhi_device, scene_renderer->rhi_tlas );
  CRUDE_GFX_FGNC_INIT( "indirect_light_pass", indirect_light_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_indirect_light_pass_queue );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( ddgi_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( total_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( meshes_instances_draws_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_line_vertices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_IMAGE( depth_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( gbuffer_normal_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_OUT_IMAGE( probe_raytrace_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( probe_offsets_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( probe_grid_irradiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( probe_grid_visibility_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, { } );
  CRUDE_GFX_FGNC_OUT_IMAGE( indirect_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, { } );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Compose pass
   *
   ********************************************************/
  CRUDE_GFX_FG_CREATE_TARGET_2D(
    radiance_texture,
    resolution.x, resolution.y,
    CRUDE_GFX_RHI_FORMAT_R16G16B16A16_SFLOAT,
    graph->gpu->default_sampler );

  compose_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_compose_pass_context );
  CRUDE_GFX_FGNC_INIT( "compose_pass", compose_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_compose_pass_queue );
  CRUDE_GFX_FGNC_IN_IMAGE( gbuffer_albedo_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( gbuffer_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( gbuffer_roughness_metalness_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( indirect_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( ssr_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( indirect_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_OUT_IMAGE( radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, { } );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Translucent pass
   *
   ********************************************************/
  translucent_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_translucent_pass_context );
  translucent_pass_context.geometry_type = CRUDE_GFX_TRANSLUCENT_PASS_GEOMETRY_TYPE_MESHLET;
  translucent_pass_context.scene = scene_context;
  translucent_pass_context.tlas_address = crude_gfx_rhi_get_acceleration_structure_device_address( &scene_renderer->gpu->rhi_device, scene_renderer->rhi_tlas );
  CRUDE_GFX_FGNC_INIT( "translucent_pass", translucent_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_translucent_pass_queue );
  CRUDE_GFX_FGNC_TYPE( CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS );
  CRUDE_GFX_FGNC_IN_MEMORY( meshes_instances_draws_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( mesh_task_indirect_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_INDIRECT_ARGUMENT );
  CRUDE_GFX_FGNC_IN_MEMORY( mesh_task_indirect_count_hga, CRUDE_GFX_RHI_RESOURCE_STATE_INDIRECT_ARGUMENT );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_line_vertices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_IN_MEMORY( joint_matrices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( culled_lights_world_to_texture_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_INOUT_IMAGE( radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET );
  CRUDE_GFX_FGNC_INOUT_IMAGE( depth_texture, CRUDE_GFX_RHI_RESOURCE_STATE_DEPTH_WRITE );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Indirect light debug pass
   *
   ********************************************************/
  indirect_light_debug_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_indirect_light_debug_pass_context );
  CRUDE_GFX_FGNC_INIT( "indirect_light_debug_pass", indirect_light_debug_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_indirect_light_debug_pass_queue );
  CRUDE_GFX_FGNC_TYPE( CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS );
  CRUDE_GFX_FGNC_IN_MARKER( translucent_pass );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( ddgi_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_INOUT_IMAGE( radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FGNC_INOUT_IMAGE( depth_texture, CRUDE_GFX_RHI_RESOURCE_STATE_DEPTH_WRITE );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Post processing pass
   *
   ********************************************************/
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( luminance_histogram_hga, sizeof( uint32 ) * 256u );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    luminance_average_texture,
    1u, 1u,
    CRUDE_GFX_RHI_FORMAT_R32_SFLOAT,
    graph->gpu->default_sampler );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    postprocessed_texture,
    resolution.x, resolution.y,
    CRUDE_GFX_RHI_FORMAT_R8G8B8A8_UNORM,
    graph->gpu->default_sampler );

  luminance_calculation_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_luminance_calculation_pass_context );
  luminance_calculation_pass_context.min_log_lum = -8.0f;
  luminance_calculation_pass_context.max_log_lum = 3.5f;
  luminance_calculation_pass_context.luminance_avarge_last_update_delta_time = crude_time_delta_seconds( scene_renderer->luminance_avarge_last_update_time, crude_time_now( ) );
  scene_renderer->luminance_avarge_last_update_time = crude_time_now( );
  CRUDE_GFX_FGNC_INIT( "luminance_calculation_pass", luminance_calculation_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_luminance_calculation_pass_queue );
  CRUDE_GFX_FGNC_IN_MARKER( translucent_pass );
  CRUDE_GFX_FGNC_IN_IMAGE( radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_OUT_MEMORY( luminance_histogram_hga, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS);
  CRUDE_GFX_FGNC_OUT_IMAGE( luminance_average_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET, { } );
  CRUDE_GFX_FGNC_SUBMIT;

  postprocessing_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_postprocessing_pass_context );
  postprocessing_pass_context.gamma = scene_renderer->options.gamma;
  CRUDE_GFX_FGNC_INIT( "postprocessing_pass", postprocessing_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_postprocessing_pass_queue );
  CRUDE_GFX_FGNC_TYPE( CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS );
  CRUDE_GFX_FGNC_IN_IMAGE( radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_IMAGE( luminance_average_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_OUT_IMAGE( postprocessed_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET, { } );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Debug pass
   *
   ********************************************************/
  debug_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_debug_pass_context );
  CRUDE_GFX_FGNC_INIT( "debug_pass", debug_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_debug_pass_queue );
  CRUDE_GFX_FGNC_TYPE( CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS );
  CRUDE_GFX_FGNC_IN_MEMORY( scene_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_line_vertices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( debug_commands_hga, CRUDE_GFX_RHI_RESOURCE_STATE_INDIRECT_ARGUMENT );
  CRUDE_GFX_FGNC_INOUT_IMAGE( postprocessed_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET );
  CRUDE_GFX_FGNC_SUBMIT;

  /********************************************************
   *
   * Imgui pass
   *
   ********************************************************/
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( imgui_vertices_hga, 665536 );
  CRUDE_GFX_FG_CREATE_MEMORY_GPU( imgui_indices_hga, 665536 );

  CRUDE_GFX_FG_CREATE_TARGET_2D(
    imgui_texture,
    resolution.x, resolution.y,
    CRUDE_GFX_RHI_FORMAT_R8G8B8A8_UNORM,
    graph->gpu->default_sampler );

  imgui_generate_buffers_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_imgui_generate_buffers_pass_context );
  imgui_generate_buffers_pass_context.imgui_font_texture_handle = scene_renderer->imgui_font_texture_handle;
  imgui_generate_buffers_pass_context.imgui_context = scene_renderer->imgui_context;
  CRUDE_GFX_FGNC_INIT( "imgui_generate_buffers_pass", imgui_generate_buffers_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_imgui_generate_buffers_pass_queue );
  CRUDE_GFX_FGNC_OUT_MEMORY( imgui_vertices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_OUT_MEMORY( imgui_indices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST );
  CRUDE_GFX_FGNC_SUBMIT;

  imgui_pass_context = CRUDE_COMPOUNT_EMPTY( crude_gfx_imgui_pass_context );
  imgui_pass_context.imgui_context = scene_renderer->imgui_context;
  CRUDE_GFX_FGNC_INIT( "imgui_pass", imgui_pass_context );
  CRUDE_GFX_FGNC_QUEUE( crude_gfx_imgui_pass_queue );
  CRUDE_GFX_FGNC_TYPE( CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS );
  CRUDE_GFX_FGNC_IN_MARKER( debug_pass );
  CRUDE_GFX_FGNC_IN_IMAGE( postprocessed_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( imgui_vertices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_IN_MEMORY( imgui_indices_hga, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGNC_OUT_IMAGE( imgui_texture, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET, { } );
  CRUDE_GFX_FGNC_SUBMIT;

  crude_gfx_frame_graph_build( scene_renderer->frame_graph );

  crude_gfx_frame_graph_render( scene_renderer->frame_graph, scene_renderer->primary_cmd );

  CRUDE_PROFILER_ZONE_END;
}

void
crude_gfx_scene_renderer_queue
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
)
{
  CRUDE_PROFILER_ZONE_NAME( "crude_gfx_scene_renderer_queue" );
  crude_gfx_queue_cmd( scene_renderer->primary_cmd );
  CRUDE_PROFILER_ZONE_END;
}

void
crude_gfx_scene_renderer_on_resize
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
)
{
  CRUDE_PROFILER_ZONE_NAME( "crude_gfx_scene_renderer_on_resize" );

  CRUDE_PROFILER_ZONE_END;
}

#if CRUDE_DEVELOP
void
crude_gfx_scene_renderer_debug_queue_clear
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
)
{
  CRUDE_ARRAY_SET_LENGTH( scene_renderer->debug_model_renderer_resoruces_instances, 0 );
  CRUDE_ARRAY_SET_LENGTH( scene_renderer->debug_line_vertices_3d, 0 );
}

void
crude_gfx_scene_renderer_debug_queue_draw_line3d
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ XMVECTOR                                            start,
  _In_ XMVECTOR                                            end,
  _In_ XMVECTOR                                            color
)
{
  crude_gfx_debug_line_vertex                              vertex_start, vertex_end;

  if ( !scene_renderer->options.debug.show_debug_gltf )
  {
    return;
  }
  
  XMStoreFloat3( &vertex_start.position, start );
  vertex_start.color = crude_color_set( XMVectorGetX( color ), XMVectorGetY( color ), XMVectorGetZ( color ), XMVectorGetW( color ) );
  
  XMStoreFloat3( &vertex_end.position, end );
  vertex_end.color = crude_color_set( XMVectorGetX( color ), XMVectorGetY( color ), XMVectorGetZ( color ), XMVectorGetW( color ) );

  CRUDE_ARRAY_PUSH( scene_renderer->debug_line_vertices_3d, vertex_start );
  CRUDE_ARRAY_PUSH( scene_renderer->debug_line_vertices_3d, vertex_end );
}

void
crude_gfx_scene_renderer_debug_queue_draw_capsule
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ XMMATRIX                                            model_to_world,
  _In_ float32                                             height,
  _In_ float32                                             radius
)
{
  XMMATRIX                                                 model_to_custom_model;

  if ( !scene_renderer->options.debug.show_debug_gltf )
  {
    return;
  }

  model_to_custom_model = XMMatrixTranslation( 0.f, height, 0.f );
  model_to_custom_model = XMMatrixMultiply( XMMatrixScaling( radius, height, radius ), model_to_custom_model );
  XMStoreFloat4x4( &scene_renderer->capsule_model_renderer_resources_instance.model_to_world, XMMatrixMultiply( model_to_custom_model, model_to_world ) );
  CRUDE_ARRAY_PUSH( scene_renderer->debug_model_renderer_resoruces_instances, scene_renderer->capsule_model_renderer_resources_instance );
}
#endif /* CRUDE_DEVELOP */

void
crude_scene_renderer_register_nodes_instances_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ crude_ecs                                          *world,
  _In_ crude_entity                                        node
)
{
  uint32                                                   model_renderer_resoruces_instances_count;
  ecs_iter_t                                               children_it;
  XMMATRIX                                                 model_to_custom_model;

  children_it = crude_ecs_children( world, node );

  model_to_custom_model = XMMatrixIdentity( );
  
  model_renderer_resoruces_instances_count = CRUDE_ARRAY_LENGTH( scene_renderer->model_renderer_resoruces_instances );

  if ( CRUDE_ENTITY_HAS_COMPONENT( world, node, crude_gltf ) )
  {
    crude_gltf                                            *child_gltf;
    bool                                                   should_render;

    child_gltf = CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, node, crude_gltf );
  
    should_render = !child_gltf->hidden && ( child_gltf->model_renderer_resources_instance.model_renderer_resources_handle.index != -1 );

#if CRUDE_DEVELOP
    if ( child_gltf->debug_only )
    {
      should_render = should_render && scene_renderer->options.debug.show_debug_gltf;
    }
#endif /* CRUDE_DEVELOP */

    if ( should_render )
    {
      XMStoreFloat4x4( &child_gltf->model_renderer_resources_instance.model_to_world, XMMatrixMultiply( model_to_custom_model, crude_transform_node_to_world( world, node, CRUDE_ENTITY_GET_IMMUTABLE_COMPONENT( world, node, crude_transform ) ) ) );
      child_gltf->model_renderer_resources_instance.unique_id = node;
      CRUDE_ARRAY_PUSH( scene_renderer->model_renderer_resoruces_instances, child_gltf->model_renderer_resources_instance );
    }
  }
  
  if ( CRUDE_ENTITY_HAS_COMPONENT( world, node, crude_light ) )
  {
    crude_gfx_light_cpu light_gpu = CRUDE_COMPOUNT_EMPTY( crude_gfx_light_cpu );
    light_gpu.light = *CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, node, crude_light );
    XMStoreFloat3( &light_gpu.translation, crude_transform_node_to_world( world, node, CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, node, crude_transform ) ).r[ 3 ] );
    CRUDE_ARRAY_PUSH( scene_renderer->lights, light_gpu ); 
  }
  
  if ( CRUDE_ENTITY_HAS_COMPONENT( world, node, crude_world_environment ) )
  {
    crude_world_environment *world_environment = CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, node, crude_world_environment );
    scene_renderer->world_environment = *world_environment;
  }
  
  if ( CRUDE_ENTITY_HAS_COMPONENT( world, node, crude_ddgi_area ) )
  {
    crude_ddgi_area *ddgi_area = CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, node, crude_ddgi_area );
    
    XMStoreFloat3( &scene_renderer->ddgi_area.probe_grid_position, crude_transform_node_to_world( world, node, NULL ).r[ 3 ] );
    scene_renderer->ddgi_area.probe_spacing = ddgi_area->probe_spacing;
    scene_renderer->ddgi_area.hysteresis = ddgi_area->hysteresis;
    scene_renderer->ddgi_area.self_shadow_bias = ddgi_area->self_shadow_bias;
    scene_renderer->ddgi_area.infinite_bounces_multiplier = ddgi_area->infinite_bounces_multiplier;
    scene_renderer->ddgi_area.max_probe_offset = ddgi_area->max_probe_offset;
    scene_renderer->ddgi_area.shadow_weight_power = ddgi_area->shadow_weight_power;
    scene_renderer->ddgi_area.probe_update_per_frame = ddgi_area->probe_update_per_frame;
    scene_renderer->ddgi_area.probe_count = ddgi_area->probe_count;
    scene_renderer->ddgi_area.probe_rays = ddgi_area->probe_rays;
    scene_renderer->ddgi_area.offsets_calculations_count = ddgi_area->offsets_calculations_count;
    scene_renderer->ddgi_enabled = true;
  }
  
#if CRUDE_EDITOR
  if ( scene_renderer->options.debug.show_debug_gltf )
  {
    if ( CRUDE_ENTITY_HAS_COMPONENT( world, node, crude_light ) )
    {
      crude_gfx_light_cpu light_gpu = CRUDE_COMPOUNT_EMPTY( crude_gfx_light_cpu );
      XMStoreFloat3( &light_gpu.translation, crude_transform_node_to_world( world, node, CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, node, crude_transform ) ).r[ 3 ] );
      
      XMStoreFloat4x4( &scene_renderer->light_model_renderer_resources_instance.model_to_world, XMMatrixTranslation( light_gpu.translation.x, light_gpu.translation.y, light_gpu.translation.z ) );
      CRUDE_ARRAY_PUSH( scene_renderer->model_renderer_resoruces_instances, scene_renderer->light_model_renderer_resources_instance );
    }
  
    if ( CRUDE_ENTITY_HAS_COMPONENT( world, node, crude_camera ) )
    {
      model_to_custom_model = XMMatrixScaling( 0.25, 0.25, 0.25 );
      XMStoreFloat4x4( &scene_renderer->camera_model_renderer_resources_instance.model_to_world, XMMatrixMultiply( model_to_custom_model, crude_transform_node_to_world( world, node, CRUDE_ENTITY_GET_IMMUTABLE_COMPONENT( world, node, crude_transform ) ) ) );
      CRUDE_ARRAY_PUSH( scene_renderer->model_renderer_resoruces_instances, scene_renderer->camera_model_renderer_resources_instance );
    }
  
    if ( CRUDE_ENTITY_HAS_COMPONENT( world, node, crude_audio_player ) )
    {
      XMStoreFloat4x4( &scene_renderer->audo_player_model_renderer_resources_instance.model_to_world, XMMatrixMultiply( model_to_custom_model, crude_transform_node_to_world( world, node, CRUDE_ENTITY_GET_IMMUTABLE_COMPONENT( world, node, crude_transform ) ) ) );
      CRUDE_ARRAY_PUSH( scene_renderer->model_renderer_resoruces_instances, scene_renderer->audo_player_model_renderer_resources_instance );
    }
  
    if ( CRUDE_ENTITY_HAS_COMPONENT( world, node, crude_audio_listener ) )
    {
      XMStoreFloat4x4( &scene_renderer->audio_listener_model_renderer_resources_instance.model_to_world, XMMatrixMultiply( model_to_custom_model, crude_transform_node_to_world( world, node, CRUDE_ENTITY_GET_IMMUTABLE_COMPONENT( world, node, crude_transform ) ) ) );
      CRUDE_ARRAY_PUSH( scene_renderer->model_renderer_resoruces_instances, scene_renderer->audio_listener_model_renderer_resources_instance );
    }
  
    if ( CRUDE_ENTITY_HAS_COMPONENT( world, node, crude_ray ) )
    {
      float32 ray_distance = CRUDE_ENTITY_GET_IMMUTABLE_COMPONENT( world, node, crude_ray )->distance;
      XMMATRIX node_to_world = crude_transform_node_to_world( world, node, CRUDE_ENTITY_GET_IMMUTABLE_COMPONENT( world, node, crude_transform ) );
      XMMATRIX scale_matrix = XMMatrixScaling( 1, 1, ray_distance );
      XMMATRIX model_to_world = model_to_custom_model;

      model_to_world = XMMatrixMultiply( model_to_world, scale_matrix );
      model_to_world = XMMatrixMultiply( model_to_world, node_to_world );
      XMStoreFloat4x4( &scene_renderer->ray_model_renderer_resources_instance.model_to_world, model_to_world );
      CRUDE_ARRAY_PUSH( scene_renderer->model_renderer_resoruces_instances, scene_renderer->ray_model_renderer_resources_instance );
    }
  }
  
  if ( scene_renderer->options.debug.show_debug_gltf )
  {
    if ( CRUDE_ENTITY_HAS_COMPONENT( world, node, crude_physics_body ) )
    {
      crude_physics_body                                  *physics_body;
    
      physics_body = CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, node, crude_physics_body );
    
      switch ( physics_body->shape.type )
      {
      case CRUDE_PHYSICS_SHAPE_TYPE_CAPSULE:
      {
        crude_gfx_scene_renderer_debug_queue_draw_capsule( scene_renderer, crude_transform_node_to_world( world, node, NULL ), physics_body->shape.capsule.height, physics_body->shape.capsule.radius );
        break;
      }
      case CRUDE_PHYSICS_SHAPE_TYPE_BOX:
      {
        model_to_custom_model = XMMatrixMultiply( XMMatrixScaling( physics_body->shape.box.extent.x, physics_body->shape.box.extent.y, physics_body->shape.box.extent.z ), model_to_custom_model );
        XMStoreFloat4x4( &scene_renderer->physics_box_collision_model_renderer_resources_instance.model_to_world, XMMatrixMultiply( model_to_custom_model, crude_transform_node_to_world( world, node, CRUDE_ENTITY_GET_IMMUTABLE_COMPONENT( world, node, crude_transform ) ) ) );
        CRUDE_ARRAY_PUSH( scene_renderer->model_renderer_resoruces_instances, scene_renderer->physics_box_collision_model_renderer_resources_instance );
        break;
      }
      case CRUDE_PHYSICS_SHAPE_TYPE_MESH:
      {
        if ( physics_body->shape.mesh.mesh_handle.index != -1 )
        {
          crude_physics_shape_mesh_container              *shape_mesh_container;

          shape_mesh_container = crude_physics_shapes_manager_access_mesh( scene_renderer->physics_shapes_manager, physics_body->shape.mesh.mesh_handle );
          XMStoreFloat4x4( &shape_mesh_container->debug_model_renderer_resource_instance.model_to_world, XMMatrixMultiply( model_to_custom_model, crude_transform_node_to_world( world, node, CRUDE_ENTITY_GET_IMMUTABLE_COMPONENT( world, node, crude_transform ) ) ) );
          CRUDE_ARRAY_PUSH( scene_renderer->model_renderer_resoruces_instances, shape_mesh_container->debug_model_renderer_resource_instance );
        }
        break;
      }
      case CRUDE_PHYSICS_SHAPE_TYPE_NONE:
      {
        break;
      }
      default:
      {
        CRUDE_ASSERT( false );
        break;
      }
      }
    }
  }
#endif /* CRUDE_DEVELOP */

  if ( model_renderer_resoruces_instances_count != CRUDE_ARRAY_LENGTH( scene_renderer->model_renderer_resoruces_instances ) )
  {
    for ( uint32 i = model_renderer_resoruces_instances_count; i < CRUDE_ARRAY_LENGTH( scene_renderer->model_renderer_resoruces_instances ); ++i )
    {
      scene_renderer->model_renderer_resoruces_instances[ i ].unique_id = node;
    }
  }

  while ( ecs_children_next( &children_it ) )
  {
    for ( size_t i = 0; i < children_it.count; ++i )
    {
      crude_entity child = crude_entity_from_iterator( &children_it, i );
      crude_scene_renderer_register_nodes_instances_( scene_renderer, world, child );
    }
  }
}

void
crude_scene_renderer_update_lights_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
)
{
  crude_gfx_device                                        *gpu;
  crude_camera const                                      *camera;
  crude_transform const                                   *camera_transform;
  XMMATRIX                                                 view_to_world, world_to_view, view_to_clip, clip_to_view, world_to_clip;
  float32                                                  tile_size, tile_position_x, tile_position_y;
  
  gpu = scene_renderer->gpu;

  camera = &scene_renderer->options.camera;
  view_to_world = XMLoadFloat4x4( &scene_renderer->options.camera_view_to_world );

  world_to_view = XMMatrixInverse( NULL, view_to_world );
  view_to_clip = crude_camera_view_to_clip( camera );
  clip_to_view = XMMatrixInverse( NULL, view_to_clip );
  world_to_clip = XMMatrixMultiply( world_to_view, view_to_clip );
  
  CRUDE_ARRAY_SET_LENGTH( scene_renderer->culled_lights, 0 );
  
  tile_size = 2048.f / CRUDE_GFX_TETRAHEDRON_SHADOWMAP_SIZE;
  
  tile_position_x = 0.f;
  tile_position_y = 0.f;
  
  for ( uint32 light_index = 0; light_index < CRUDE_ARRAY_LENGTH( scene_renderer->lights ); ++light_index )
  {
    crude_gfx_light_cpu                               *light;
    XMVECTOR                                           light_world_position, light_view_position;
    XMVECTOR                                           aabb, aabb_screen;
    float32                                            aabb_screen_min_x, aabb_screen_max_x, aabb_screen_min_y, aabb_screen_max_y;
    float32                                            aabb_screen_width, aabb_screen_height, aabb_screen_width_cropped, aabb_screen_height_cropped;
    float32                                            light_view_position_length, light_radius;
    bool                                               camera_inside, camera_visible, ty_camera_inside, tx_camera_inside;
  
    light = &scene_renderer->lights[ light_index ];
  
    /* Transform light in camera space */
    light_world_position = XMVectorSet( light->translation.x, light->translation.y, light->translation.z, 1.0f );
    light_radius = light->light.radius;
  
    light_view_position = XMVector4Transform( light_world_position, world_to_view );
#if CRUDE_RIGHT_HAND
    light_view_position = XMVectorSetZ( light_view_position, -1.f * XMVectorGetZ( light_view_position ) );
#endif
    camera_visible = -XMVectorGetZ( light_view_position ) - light_radius < camera->near_z;
  
    if ( !camera_visible )
    {
      continue;
    }
  
    aabb = crude_compute_projected_sphere_aabb( light_world_position, light_radius, world_to_view, view_to_clip, camera->near_z );
  
    light_view_position_length = XMVectorGetX( XMVector3Length( light_view_position ) );
    camera_inside = ( light_view_position_length - light_radius ) < camera->near_z;
  
    if ( camera_inside )
    {
      aabb = { -1,-1, 1, 1 };
    }

    aabb_screen = XMVectorSet(
      ( XMVectorGetX( aabb ) * 0.5f + 0.5f ) * ( gpu->renderer_size.x - 1 ),
      ( XMVectorGetY( aabb ) * 0.5f + 0.5f ) * ( gpu->renderer_size.y - 1 ),
      ( XMVectorGetZ( aabb ) * 0.5f + 0.5f ) * ( gpu->renderer_size.x - 1 ),
      ( XMVectorGetW( aabb ) * 0.5f + 0.5f ) * ( gpu->renderer_size.y - 1 )
    );
  
    aabb_screen_width = XMVectorGetZ( aabb_screen ) - XMVectorGetX( aabb_screen );
    aabb_screen_height = XMVectorGetW( aabb_screen ) - XMVectorGetY( aabb_screen );
  
    if ( aabb_screen_width < 0.0001f || aabb_screen_height < 0.0001f )
    {
      continue;
    }
  
    aabb_screen_min_x = XMVectorGetX( aabb_screen );
    aabb_screen_min_y = XMVectorGetY( aabb_screen );
    
    aabb_screen_max_x = aabb_screen_min_x + aabb_screen_width;
    aabb_screen_max_y = aabb_screen_min_y + aabb_screen_height;
  
    if ( aabb_screen_min_x > gpu->renderer_size.x || aabb_screen_min_y > gpu->renderer_size.y )
    {
      continue;
    }
  
    if ( aabb_screen_max_x < 0.0f || aabb_screen_max_y < 0.0f )
    {
      continue;
    }
  
    aabb_screen_min_x = CRUDE_MAX( aabb_screen_min_x, 0.0f );
    aabb_screen_min_y = CRUDE_MAX( aabb_screen_min_y, 0.0f );
  
    aabb_screen_max_x = CRUDE_MIN( aabb_screen_max_x, gpu->renderer_size.x );
    aabb_screen_max_y = CRUDE_MIN( aabb_screen_max_y, gpu->renderer_size.y );

    aabb_screen_width_cropped = aabb_screen_max_x - aabb_screen_min_x;
    aabb_screen_height_cropped = aabb_screen_max_y - aabb_screen_min_y;

    crude_gfx_culled_light_cpu culled_light_cpu;
    culled_light_cpu.light_index = light_index;
    culled_light_cpu.screen_area = aabb_screen_width_cropped * aabb_screen_height_cropped;
    culled_light_cpu.tile_position.x = tile_position_x;
    culled_light_cpu.tile_position.y = tile_position_y;
    culled_light_cpu.tile_size = tile_size;
    CRUDE_ARRAY_PUSH( scene_renderer->culled_lights, culled_light_cpu );

    tile_position_x += tile_size;

    if ( tile_position_x > 0.99 )
    {
      tile_position_x = 0;
      tile_position_y += tile_size;
    }
  }
}

void
crude_scene_renderer_update_meshes_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer
)
{
  CRUDE_ARRAY_SET_LENGTH( scene_renderer->joint_matrices, 1 );
  XMStoreFloat4x4( &scene_renderer->joint_matrices[ 0 ], XMMatrixIdentity( ) );

  CRUDE_ARRAY_SET_LENGTH( scene_renderer->meshes_instances_draws, 0 );

  for ( uint32 model_instance_index = 0u; model_instance_index < CRUDE_ARRAY_LENGTH( scene_renderer->model_renderer_resoruces_instances ); ++model_instance_index )
  {
    crude_gfx_model_renderer_resources_instance const   *model_renderer_resources_instance;
    crude_gfx_model_renderer_resources const            *model_renderer_resources;

    model_renderer_resources_instance = &scene_renderer->model_renderer_resoruces_instances[ model_instance_index ];
    model_renderer_resources = crude_gfx_model_renderer_resources_manager_access( scene_renderer->model_renderer_resources_manager, model_renderer_resources_instance->model_renderer_resources_handle );

    for ( uint32 node_index = 0u; node_index < CRUDE_ARRAY_LENGTH( model_renderer_resources->nodes ); ++node_index )
    {
      crude_gfx_node                                    *node;
      XMMATRIX                                           mesh_to_model, model_to_world, mesh_to_world;

      node = &model_renderer_resources->nodes[ node_index ];

      if ( node->meshes || node->skin != -1 )
      {
        mesh_to_model = crude_gfx_node_to_model( model_renderer_resources->nodes, model_renderer_resources_instance->nodes_transforms, node_index );
        model_to_world = XMLoadFloat4x4( &model_renderer_resources_instance->model_to_world );
        mesh_to_world = XMMatrixMultiply( mesh_to_model, model_to_world );
      }

      if ( node->meshes )
      {
        for ( uint32 mesh_index = 0; mesh_index < CRUDE_ARRAY_LENGTH( node->meshes ); ++mesh_index )
        {
          crude_gfx_mesh_cpu                            *cpu_mesh;
          crude_gfx_mesh_instance_draw                   mesh_instance_draw;

          cpu_mesh = &model_renderer_resources->meshes[ node->meshes[ mesh_index ] ];

          mesh_instance_draw = CRUDE_COMPOUNT_EMPTY( crude_gfx_mesh_instance_draw );
          mesh_instance_draw.mesh_draw = model_renderer_resources->meshes_draws_hga[ node->meshes[ mesh_index ] ].gpu_address;

          XMStoreFloat4x4( &mesh_instance_draw.mesh_to_world, mesh_to_world );
          XMStoreFloat4x4( &mesh_instance_draw.world_to_mesh, XMMatrixInverse( NULL, mesh_to_world ) );

          if ( node->skin != -1 )
          {
            if ( CRUDE_ARRAY_LENGTH( node->affected_joints[ mesh_index ] ) )
            {
              crude_gfx_skin                            *skin;
              XMMATRIX                                   model_to_mesh;
              XMVECTOR                                   position_max, position_min;
              XMVECTOR                                   bounding_center;
              float32                                    bounding_radius;

              skin = &model_renderer_resources->skins[ node->skin ];
              model_to_mesh = XMMatrixInverse( NULL, mesh_to_model );

              position_max = XMLoadFloat3( &node->affected_joints_local_aabb[ mesh_index ][ 0 ].max );
              position_min = XMLoadFloat3( &node->affected_joints_local_aabb[ mesh_index ][ 0 ].min );

              for ( uint32 affected_joint = 0; affected_joint < CRUDE_ARRAY_LENGTH( node->affected_joints[ mesh_index ] ); ++affected_joint )
              {
                crude_gfx_aabb_cpu                      *local_joint_aabb;
                XMVECTOR                                 animated_joint_aabb_max;
                XMVECTOR                                 animated_joint_aabb_min;
                XMMATRIX                                 joint_matrix, inverse_bind_matrix;

                local_joint_aabb = &node->affected_joints_local_aabb[ mesh_index ][ affected_joint ];

                inverse_bind_matrix = XMLoadFloat4x4( &skin->inverse_bind_matrices[ node->affected_joints[ mesh_index ][ affected_joint ] ] );
                joint_matrix = crude_gfx_node_to_model( model_renderer_resources->nodes, model_renderer_resources_instance->nodes_transforms, skin->joints[ node->affected_joints[ mesh_index ][ affected_joint ] ] );
                joint_matrix = XMMatrixMultiply( XMMatrixMultiply( inverse_bind_matrix, joint_matrix ), model_to_mesh );

                animated_joint_aabb_max = XMVector3Transform( XMLoadFloat3( &local_joint_aabb->max ), joint_matrix );
                animated_joint_aabb_min = XMVector3Transform( XMLoadFloat3( &local_joint_aabb->min ), joint_matrix );
                position_max = XMVectorMax( position_max, animated_joint_aabb_max );
                position_min = XMVectorMin( position_min, animated_joint_aabb_min );
              }

              bounding_center = XMVectorAdd( position_max, position_min );
              bounding_center = XMVectorScale( bounding_center, 0.5f );
              bounding_radius = XMVectorGetX( XMVectorMax( XMVector3Length( position_max - bounding_center ), XMVector3Length( position_min - bounding_center ) ) );

              XMStoreFloat4( &mesh_instance_draw.bounding_sphere, XMVectorSet( XMVectorGetX( bounding_center ), XMVectorGetY( bounding_center ), XMVectorGetZ( bounding_center ), bounding_radius ) );
            }
            else
            {
              mesh_instance_draw.bounding_sphere = cpu_mesh->default_bounding_sphere;
            }

            mesh_instance_draw.joints_matrices_offset = CRUDE_ARRAY_LENGTH( scene_renderer->joint_matrices );
          }
          else
          {
            mesh_instance_draw.bounding_sphere = cpu_mesh->default_bounding_sphere;
            mesh_instance_draw.joints_matrices_offset = 0;
          }

          mesh_instance_draw.flags = 0;

          if ( model_renderer_resources_instance->cast_shadow )
          {
            mesh_instance_draw.flags |= CRUDE_GFX_MESH_INSTANCE_DRAW_FLAG_CAST_SHADOW;
          }

          if ( node->skin != -1 )
          {
            mesh_instance_draw.flags |= CRUDE_GFX_MESH_INSTANCE_DRAW_FLAG_ANIMATED;
          }

          CRUDE_ARRAY_PUSH( scene_renderer->meshes_instances_draws, mesh_instance_draw );
        }
      }

      if ( node->skin != -1 )
      {
        crude_gfx_skin                                  *skin;
        XMMATRIX                                         model_to_mesh;

        skin = &model_renderer_resources->skins[ node->skin ];

        model_to_mesh = XMMatrixInverse( NULL, mesh_to_model );

        for ( uint64 i = 0; i < CRUDE_ARRAY_LENGTH( skin->joints ); ++i )
        {
          XMMATRIX                                           joint_matrix, inverse_bind_matrix;
          XMFLOAT4X4                                         joint_matrix_f4;

          inverse_bind_matrix = XMLoadFloat4x4( &skin->inverse_bind_matrices[ i ] );
          joint_matrix = crude_gfx_node_to_model( model_renderer_resources->nodes, model_renderer_resources_instance->nodes_transforms, skin->joints[ i ] );

          XMStoreFloat4x4( &joint_matrix_f4, XMMatrixMultiply( XMMatrixMultiply( inverse_bind_matrix, joint_matrix ), model_to_mesh ) );
          CRUDE_ARRAY_PUSH( scene_renderer->joint_matrices, joint_matrix_f4 );
        }
      }
    }
  }
}

#if CRUDE_GFX_RAY_TRACING_ENABLED

#if CRUDE_GFX_VULKAN_AVAILABLE
void
crude_gfx_scene_renderer_fill_top_level_acceleration_structures_instances_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _Inout_ VkAccelerationStructureInstanceKHR             **vk_acceleration_structure_instances
)
{
  uint32                                                   instance_custom_index;

  instance_custom_index = 0u;
  for ( uint32 model_instance_index = 0u; model_instance_index < CRUDE_ARRAY_LENGTH( scene_renderer->model_renderer_resoruces_instances ); ++model_instance_index )
  {
    crude_gfx_model_renderer_resources_instance const     *model_renderer_resources_instance;
    crude_gfx_model_renderer_resources const              *model_renderer_resources;
  
    model_renderer_resources_instance = &scene_renderer->model_renderer_resoruces_instances[ model_instance_index ];
    model_renderer_resources = crude_gfx_model_renderer_resources_manager_access( scene_renderer->model_renderer_resources_manager, model_renderer_resources_instance->model_renderer_resources_handle );
  
    if ( !model_renderer_resources_instance->rtx_affected || !model_renderer_resources->rtx_affected )
    {
      for ( uint32 node_index = 0u; node_index < CRUDE_ARRAY_LENGTH( model_renderer_resources->nodes ); ++node_index )
      {
        crude_gfx_node                                      *node;

        node = &model_renderer_resources->nodes[ node_index ];
          
        if ( node->meshes )
        {
          instance_custom_index += CRUDE_ARRAY_LENGTH( node->meshes );
        }
      }
      continue;
    }

    for ( uint32 node_index = 0u; node_index < CRUDE_ARRAY_LENGTH( model_renderer_resources->nodes ); ++node_index )
    {
      crude_gfx_node                                      *node;
      XMMATRIX                                             mesh_to_model, model_to_world, mesh_to_world;

      node = &model_renderer_resources->nodes[ node_index ];
        
      if ( !node->meshes )
      {
        continue;
      }

      CRUDE_ASSERT( node->skin == -1 );
      
      mesh_to_model = crude_gfx_node_to_model( model_renderer_resources->nodes, model_renderer_resources_instance->nodes_transforms, node_index );
      model_to_world = XMLoadFloat4x4( &model_renderer_resources_instance->model_to_world );
      mesh_to_world = XMMatrixMultiply( mesh_to_model, model_to_world );
      
      for ( uint32 mesh_index = 0; mesh_index < CRUDE_ARRAY_LENGTH( node->meshes ); ++mesh_index )
      {
        crude_gfx_mesh_cpu                             *cpu_mesh;
        crude_gfx_mesh_instance_draw                   *gpu_mesh_instance_draw;
        VkAccelerationStructureInstanceKHR              vk_acceleration_structure_instance;
        VkAccelerationStructureDeviceAddressInfoKHR     vk_acceleration_structure_address_info;
        VkDeviceAddress                                 vk_blas_address;
        VkTransformMatrixKHR                            vk_transform; 
      
        cpu_mesh = &model_renderer_resources->meshes[ node->meshes[ mesh_index ] ];
        
        for ( int32 y = 0; y < 3; ++y )
        {
          for ( int32 x = 0; x < 4; ++x )
          {
            vk_transform.matrix[ y ][ x ] = XMVectorGetByIndex( mesh_to_world.r[ x ], y );
          }
        }
      
        vk_acceleration_structure_address_info = CRUDE_COMPOUNT_EMPTY( VkAccelerationStructureDeviceAddressInfoKHR );
        vk_acceleration_structure_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        vk_acceleration_structure_address_info.accelerationStructure = model_renderer_resources->rhi_blases[ node->meshes[ mesh_index ] ].vk.acceleration_structure;
      
        vk_blas_address = scene_renderer->gpu->rhi_device.vk.vkGetAccelerationStructureDeviceAddressKHR( scene_renderer->gpu->rhi_device.vk.device, &vk_acceleration_structure_address_info );
      
        vk_acceleration_structure_instance = CRUDE_COMPOUNT_EMPTY( VkAccelerationStructureInstanceKHR );
        vk_acceleration_structure_instance.mask = 0xff;
        vk_acceleration_structure_instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR; /* TODO ??? */
        vk_acceleration_structure_instance.accelerationStructureReference = vk_blas_address;
        vk_acceleration_structure_instance.transform = vk_transform;
        vk_acceleration_structure_instance.instanceCustomIndex = instance_custom_index;
      
        CRUDE_ARRAY_PUSH( *vk_acceleration_structure_instances, vk_acceleration_structure_instance );
      
        ++instance_custom_index;
      }
    }
  }
}
#endif /* CRUDE_GFX_VULKAN_AVAILABLE */

void
crude_gfx_scene_renderer_create_top_level_acceleration_structure_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
#if CRUDE_GFX_VULKAN_AVAILABLE
  VkAccelerationStructureInstanceKHR                      *vk_acceleration_structure_instances;
#endif
  crude_gfx_rhi_acceleration_structure_create_info         acceleration_structure_create_info;
  crude_gfx_rhi_acceleration_structure_build_geometry_info acceleration_build_geometry_info;
  crude_gfx_rhi_acceleration_structure_geometry            acceleration_structure_geometry;
  crude_gfx_rhi_acceleration_structure_build_sizes_info    acceleration_structure_build_sizes_info;
  crude_gfx_rhi_acceleration_structure_build_range_info    acceleration_structure_build_range_info;
  uint32                                                   max_instance_count;
  uint32                                                   instance_custom_index;
  
  if ( crude_gfx_rhi_get_type( ) == CRUDE_GFX_RHI_TYPE_VULKAN )
  {
#if CRUDE_GFX_VULKAN_AVAILABLE
     CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( vk_acceleration_structure_instances, CRUDE_ARRAY_LENGTH( scene_renderer->meshes_instances_draws ), crude_heap_allocator_pack( scene_renderer->allocator ) );
    crude_gfx_scene_renderer_fill_top_level_acceleration_structures_instances_( scene_renderer, &vk_acceleration_structure_instances );

    max_instance_count = CRUDE_ARRAY_LENGTH( vk_acceleration_structure_instances );

    scene_renderer->tlas_instances_hga = crude_gfx_memory_allocate_with_name(
      scene_renderer->gpu,
      max_instance_count * sizeof( VkAccelerationStructureInstanceKHR ),
      CRUDE_GFX_MEMORY_TYPE_CPU_GPU, /* !TODO try gpu only and check if it affect perfomance*/
      CRUDE_STRING_VIEW( "tlas_instances_hga" ),
      CRUDE_GFX_RHI_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR );

    crude_memory_copy( scene_renderer->tlas_instances_hga.cpu_address, vk_acceleration_structure_instances, max_instance_count * sizeof( VkAccelerationStructureInstanceKHR ) );
    
    CRUDE_ARRAY_DEINITIALIZE( vk_acceleration_structure_instances );
#else
    CRUDE_ASSERT( false );
#endif /* CRUDE_GFX_VULKAN_AVAILABLE */
  }
  else if ( crude_gfx_rhi_get_type( ) == CRUDE_GFX_RHI_TYPE_NULL )
  {
    scene_renderer->tlas_instances_hga = crude_gfx_memory_allocate_with_name(
      scene_renderer->gpu,
      0,
      CRUDE_GFX_MEMORY_TYPE_CPU_GPU,
      CRUDE_STRING_VIEW( "tlas_instances_hga" ),
      CRUDE_GFX_RHI_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR );
  }
  else
  {
    CRUDE_ASSERT( false );
  }

  acceleration_structure_geometry = CRUDE_COMPOUNT_EMPTY( crude_gfx_rhi_acceleration_structure_geometry );
  acceleration_structure_geometry.geometry_type = CRUDE_GFX_RHI_GEOMETRY_TYPE_INSTANCES_KHR;
  acceleration_structure_geometry.flags = CRUDE_GFX_RHI_GEOMETRY_OPAQUE_BIT_KHR;
  acceleration_structure_geometry.geometry.instances.array_of_pointers = false;
  acceleration_structure_geometry.geometry.instances.data.device_address = scene_renderer->tlas_instances_hga.gpu_address;

  acceleration_build_geometry_info = CRUDE_COMPOUNT_EMPTY( crude_gfx_rhi_acceleration_structure_build_geometry_info );
  acceleration_build_geometry_info.type = CRUDE_GFX_RHI_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  acceleration_build_geometry_info.flags = CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_BIT_KHR | CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
  acceleration_build_geometry_info.geometry_count = 1;
  acceleration_build_geometry_info.geometries = &acceleration_structure_geometry;
  
  acceleration_structure_build_sizes_info = CRUDE_COMPOUNT_EMPTY( crude_gfx_rhi_acceleration_structure_build_sizes_info );  
  
  crude_gfx_cmd_buffer_barrier_explicit( primary_cmd, scene_renderer->tlas_instances_hga.buffer_handle, CRUDE_GFX_RHI_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, CRUDE_GFX_RHI_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE );

  crude_gfx_rhi_get_acceleration_structure_build_sizes(
    &scene_renderer->gpu->rhi_device, scene_renderer->allocator,
    CRUDE_GFX_RHI_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
    &acceleration_build_geometry_info,
    &max_instance_count,
    &acceleration_structure_build_sizes_info );
  
  scene_renderer->tlas_hga = crude_gfx_memory_allocate_with_name(
    scene_renderer->gpu,
    acceleration_structure_build_sizes_info.acceleration_structure_size,
    CRUDE_GFX_MEMORY_TYPE_GPU,
    CRUDE_STRING_VIEW( "tlas_hga" ),
    CRUDE_GFX_RHI_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR );
  
  acceleration_structure_create_info = CRUDE_COMPOUNT_EMPTY( crude_gfx_rhi_acceleration_structure_create_info );
  acceleration_structure_create_info.buffer = crude_gfx_access_buffer( scene_renderer->gpu, scene_renderer->tlas_hga.buffer_handle )->rhi_buffer;
  acceleration_structure_create_info.size = acceleration_structure_build_sizes_info.acceleration_structure_size;
  acceleration_structure_create_info.type = CRUDE_GFX_RHI_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

  crude_gfx_rhi_create_acceleration_structure( &scene_renderer->gpu->rhi_device, &acceleration_structure_create_info, &scene_renderer->rhi_tlas );
  crude_gfx_rhi_set_acceleration_structure_debug_name( &scene_renderer->gpu->rhi_device, scene_renderer->rhi_tlas, CRUDE_STRING_VIEW( "rhi_tlas" ) );

  scene_renderer->tlas_scratch_hga = crude_gfx_memory_allocate_with_name(
    scene_renderer->gpu, 
    acceleration_structure_build_sizes_info.build_scratch_size,
    CRUDE_GFX_MEMORY_TYPE_GPU,
    CRUDE_STRING_VIEW( "tlas_scratch_hga" ),
    CRUDE_GFX_RHI_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR );

  acceleration_build_geometry_info = CRUDE_COMPOUNT_EMPTY( crude_gfx_rhi_acceleration_structure_build_geometry_info );
  acceleration_build_geometry_info.type = CRUDE_GFX_RHI_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  acceleration_build_geometry_info.flags = CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_BIT_KHR | CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
  acceleration_build_geometry_info.mode = CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  acceleration_build_geometry_info.dst_acceleration_structure = scene_renderer->rhi_tlas;
  acceleration_build_geometry_info.geometry_count = 1;
  acceleration_build_geometry_info.geometries = &acceleration_structure_geometry;
  acceleration_build_geometry_info.scratch_data.device_address = scene_renderer->tlas_scratch_hga.gpu_address;
  
  acceleration_structure_build_range_info = CRUDE_COMPOUNT_EMPTY( crude_gfx_rhi_acceleration_structure_build_range_info );
  acceleration_structure_build_range_info.primitive_count = max_instance_count;
  
  crude_gfx_cmd_buffer_barrier_explicit( primary_cmd, scene_renderer->tlas_instances_hga.buffer_handle, CRUDE_GFX_RHI_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, CRUDE_GFX_RHI_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE );

  crude_gfx_rhi_command_buffer_build_acceleration_structures(
    &scene_renderer->gpu->rhi_device, scene_renderer->allocator,
    primary_cmd->rhi_cmd_buffer,
    1u,
    &acceleration_build_geometry_info,
    &acceleration_structure_build_range_info );

  crude_gfx_cmd_buffer_barrier_explicit( primary_cmd, scene_renderer->tlas_instances_hga.buffer_handle, CRUDE_GFX_RHI_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, CRUDE_GFX_RHI_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE );
}

void
crude_gfx_scene_renderer_update_top_level_acceleration_structure_
(
  _In_ crude_gfx_scene_renderer                           *scene_renderer,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
#if CRUDE_GFX_VULKAN_AVAILABLE
  VkAccelerationStructureInstanceKHR                      *vk_acceleration_structure_instances;
#endif
  crude_gfx_buffer                                        *tlas_buffer;
  crude_gfx_rhi_acceleration_structure_build_geometry_info acceleration_build_geometry_info;
  crude_gfx_rhi_acceleration_structure_geometry            acceleration_structure_geometry;
  crude_gfx_rhi_acceleration_structure_build_sizes_info    acceleration_structure_build_sizes_info;
  crude_gfx_rhi_acceleration_structure_build_range_info    acceleration_structure_build_range_info;
  uint32                                                   max_instance_count;
  
  if ( CRUDE_RESOURCE_HANDLE_IS_INVALID( scene_renderer->tlas_hga.buffer_handle ) )
  {
    return;
  }
  
  max_instance_count = 0u;

  if ( crude_gfx_rhi_get_type( ) == CRUDE_GFX_RHI_TYPE_VULKAN )
  {
    CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( vk_acceleration_structure_instances, CRUDE_ARRAY_LENGTH( scene_renderer->meshes_instances_draws ), crude_heap_allocator_pack( scene_renderer->allocator ) );
    crude_gfx_scene_renderer_fill_top_level_acceleration_structures_instances_( scene_renderer, &vk_acceleration_structure_instances );
  
    max_instance_count = CRUDE_ARRAY_LENGTH( vk_acceleration_structure_instances );

    crude_memory_copy( scene_renderer->tlas_instances_hga.cpu_address, vk_acceleration_structure_instances, sizeof( VkAccelerationStructureInstanceKHR ) * max_instance_count );
    CRUDE_ARRAY_DEINITIALIZE( vk_acceleration_structure_instances );
  }
  else if ( crude_gfx_rhi_get_type( ) != CRUDE_GFX_RHI_TYPE_NULL )
  {
    CRUDE_ASSERT( false );
  }

  acceleration_structure_geometry = CRUDE_COMPOUNT_EMPTY( crude_gfx_rhi_acceleration_structure_geometry );
  acceleration_structure_geometry.geometry_type = CRUDE_GFX_RHI_GEOMETRY_TYPE_INSTANCES_KHR;
  acceleration_structure_geometry.flags = CRUDE_GFX_RHI_GEOMETRY_OPAQUE_BIT_KHR;
  acceleration_structure_geometry.geometry.instances.array_of_pointers = false;
  acceleration_structure_geometry.geometry.instances.data.device_address = scene_renderer->tlas_instances_hga.gpu_address;

  acceleration_build_geometry_info = CRUDE_COMPOUNT_EMPTY( crude_gfx_rhi_acceleration_structure_build_geometry_info );
  acceleration_build_geometry_info.type = CRUDE_GFX_RHI_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  acceleration_build_geometry_info.flags = CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_BIT_KHR | CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
  acceleration_build_geometry_info.mode = CRUDE_GFX_RHI_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
  acceleration_build_geometry_info.dst_acceleration_structure = scene_renderer->rhi_tlas;
  acceleration_build_geometry_info.src_acceleration_structure = scene_renderer->rhi_tlas;
  acceleration_build_geometry_info.geometry_count = 1;
  acceleration_build_geometry_info.geometries = &acceleration_structure_geometry;
  acceleration_build_geometry_info.scratch_data.device_address = scene_renderer->tlas_scratch_hga.gpu_address;

  acceleration_structure_build_range_info = CRUDE_COMPOUNT_EMPTY( crude_gfx_rhi_acceleration_structure_build_range_info );
  acceleration_structure_build_range_info.primitive_count = max_instance_count;

  crude_gfx_cmd_buffer_barrier_explicit( primary_cmd, scene_renderer->tlas_instances_hga.buffer_handle, CRUDE_GFX_RHI_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, CRUDE_GFX_RHI_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE );

  crude_gfx_rhi_command_buffer_build_acceleration_structures(
    &scene_renderer->gpu->rhi_device, scene_renderer->allocator,
    primary_cmd->rhi_cmd_buffer,
    1u,
    &acceleration_build_geometry_info,
    &acceleration_structure_build_range_info );

  crude_gfx_cmd_buffer_barrier_explicit( primary_cmd, scene_renderer->tlas_instances_hga.buffer_handle, CRUDE_GFX_RHI_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, CRUDE_GFX_RHI_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE );
}

#endif /* CRUDE_GFX_RAY_TRACING_ENABLED */
