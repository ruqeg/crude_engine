#include <engine/graphics/scene_renderer.h>
#include <engine/graphics/scene_renderer_resources.h>

#define POINTSHADOW_CULLING
#define POINTSHADOW_COMMANDS_GENERATION
#define POINTSHADOW
#include <engine/graphics/shaders/pointshadow_meshlet.crude_shader>

#include <engine/graphics/passes/pointlight_shadow_pass.h>

void
crude_gfx_pointlight_shadow_culling_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_pipeline_handle                                pointshadow_culling_pipeline;
  crude_gfx_pointshadow_culling_pass_push_constant_        push_constant;
  
  CRUDE_GFX_FGN_UTILS( );
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_pointlight_shadow_culling_pass_context );
  CRUDE_GFX_FGN_MEMORY( scene_hga );
  CRUDE_GFX_FGN_MEMORY( meshes_instances_draws_hga );
  CRUDE_GFX_FGN_MEMORY( culled_lights_hga );
  CRUDE_GFX_FGN_MEMORY( culled_pointshadow_meshletes_instances_hga );
  CRUDE_GFX_FGN_MEMORY( culled_pointshadow_meshletes_instances_count_hga );
  
  pointshadow_culling_pipeline = crude_gfx_access_technique_pipeline_by_name( node->graph->gpu, CRUDE_STRING_VIEW( "pointshadow_culling" ), 0u );

  crude_gfx_cmd_fill_buffer( primary_cmd, culled_pointshadow_meshletes_instances_count_hga.buffer_handle, 0u );
  
  crude_gfx_cmd_buffer_barrier( primary_cmd, culled_pointshadow_meshletes_instances_count_hga.buffer_handle, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );

  crude_gfx_cmd_bind_pipeline( primary_cmd, pointshadow_culling_pipeline );
    
  push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_pointshadow_culling_pass_push_constant_ );
  push_constant.scene = scene_hga.gpu_address;
  push_constant.mesh_instance_draws = meshes_instances_draws_hga.gpu_address;
  push_constant.culled_lights = culled_lights_hga.gpu_address;
  push_constant.culled_pointshadow_meshletes_instances = culled_pointshadow_meshletes_instances_hga.gpu_address;
  push_constant.culled_pointshadow_meshletes_instances_count = culled_pointshadow_meshletes_instances_count_hga.gpu_address;
  crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

  crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

  crude_gfx_cmd_dispatch( primary_cmd, ( CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws ) * CRUDE_ARRAY_LENGTH( ctx->scene.culled_lights ) + 31 ) / 32, 1u, 1u );
}

void
crude_gfx_pointlight_shadow_commands_generation_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_pointshadow_commands_generation_pass_push_constant_      push_constant;
  crude_gfx_pipeline_handle                                          pointshadow_commands_generation_pipeline;
  
  CRUDE_GFX_FGN_UTILS( );
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_pointlight_shadow_commands_generation_pass_context );
  CRUDE_GFX_FGN_MEMORY( scene_hga );
  CRUDE_GFX_FGN_MEMORY( culled_pointshadow_meshletes_instances_count_hga );
  CRUDE_GFX_FGN_MEMORY( culled_pointshadow_meshlet_draw_commands_hga );

  pointshadow_commands_generation_pipeline = crude_gfx_access_technique_pipeline_by_name( node->graph->gpu, CRUDE_STRING_VIEW( "pointshadow_commands_generation" ), 0u );
  
  crude_gfx_cmd_bind_pipeline( primary_cmd, pointshadow_commands_generation_pipeline );
  
  push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_pointshadow_commands_generation_pass_push_constant_ );
  push_constant.scene = scene_hga.gpu_address;
  push_constant.culled_pointshadow_meshletes_instances_count = culled_pointshadow_meshletes_instances_count_hga.gpu_address;
  push_constant.culled_pointshadow_meshlet_draw_commands = culled_pointshadow_meshlet_draw_commands_hga.gpu_address;
  crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );
  
  crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
  
  crude_gfx_cmd_dispatch( primary_cmd, ( ctx->culled_lights_count + 31 ) / 32, 1u, 1u );
}

/* GPU Pro 6 Tile-Based Omnidirectional Shadows Hawar Doghramachi, mixed with http://www.hd-prg.com/tileBasedShadows.html */
void
crude_gfx_pointlight_shadow_draw_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_pointshadow_pass_push_constant_                push_constant;
  crude_gfx_pipeline_handle                                pointshadow_pipeline;
  
  CRUDE_GFX_FGN_UTILS( );
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_pointlight_shadow_draw_pass_context );
  CRUDE_GFX_FGN_MEMORY( scene_hga );
  CRUDE_GFX_FGN_MEMORY( meshes_instances_draws_hga );
  CRUDE_GFX_FGN_MEMORY( debug_commands_hga );
  CRUDE_GFX_FGN_MEMORY( joint_matrices_hga );
  CRUDE_GFX_FGN_MEMORY( culled_lights_hga );
  CRUDE_GFX_FGN_MEMORY( culled_pointshadow_meshletes_instances_hga );
  CRUDE_GFX_FGN_MEMORY( culled_pointshadow_meshletes_instances_count_hga );
  CRUDE_GFX_FGN_MEMORY( culled_lights_world_to_texture_hga );
  CRUDE_GFX_FGN_MEMORY( culled_pointlight_spheres_hga );
  CRUDE_GFX_FGN_MEMORY( culled_pointshadow_meshlet_draw_commands_hga );
  CRUDE_GFX_FGN_TEXTURE( culled_tetrahedron_shadow_texture );

  pointshadow_pipeline = crude_gfx_access_technique_pipeline_by_name( node->graph->gpu, CRUDE_STRING_VIEW( "pointshadow" ), 0u );

  crude_gfx_cmd_bind_pipeline( primary_cmd, pointshadow_pipeline );
  
  push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_pointshadow_pass_push_constant_ );
  push_constant.scene = scene_hga.gpu_address;
  push_constant.mesh_instance_draws = meshes_instances_draws_hga.gpu_address;
  push_constant.debug_counts = debug_commands_hga.gpu_address;
  push_constant.joint_matrices = joint_matrices_hga.gpu_address;
  push_constant.culled_lights = culled_lights_hga.gpu_address;
  push_constant.culled_pointshadow_meshletes_instances = culled_pointshadow_meshletes_instances_hga.gpu_address;
  push_constant.culled_pointshadow_meshletes_instances_count = culled_pointshadow_meshletes_instances_count_hga.gpu_address;
  push_constant.culled_pointlight_spheres = culled_pointlight_spheres_hga.gpu_address;
  push_constant.culled_pointshadow_meshlet_draw_commands = culled_pointshadow_meshlet_draw_commands_hga.gpu_address;
  push_constant.culled_lights_world_to_texture = culled_lights_world_to_texture_hga.gpu_address;
  crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

  crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
  
  crude_gfx_cmd_draw_mesh_task_indirect_count( primary_cmd,
    culled_pointshadow_meshlet_draw_commands_hga.buffer_handle, 0u,
    culled_pointshadow_meshletes_instances_count_hga.buffer_handle, sizeof( uint32 ) * CRUDE_LIGHTS_MAX_COUNT,
    4u * CRUDE_LIGHTS_MAX_COUNT, sizeof( XMUINT4 ) );
}
