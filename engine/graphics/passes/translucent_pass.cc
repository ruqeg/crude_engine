#define TRANSLUCENT_NO_CULL_MESHLET
#define TRANSLUCENT_NO_CULL_CLASSIC
#include <engine/graphics/shaders/geometry.crude_shader>

#include <engine/graphics/passes/translucent_pass.h>

void
crude_gfx_translucent_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_pipeline_handle                                pipeline;

  CRUDE_GFX_FGN_UTILS();
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_translucent_pass_context );
  CRUDE_GFX_FGN_MEMORY( meshes_instances_draws_hga );
  CRUDE_GFX_FGN_MEMORY( scene_hga );
  CRUDE_GFX_FGN_MEMORY( mesh_task_indirect_commands_hga );
  CRUDE_GFX_FGN_MEMORY( mesh_task_indirect_count_hga );
  CRUDE_GFX_FGN_MEMORY( culled_lights_hga );
  CRUDE_GFX_FGN_MEMORY( debug_line_vertices_hga );
  CRUDE_GFX_FGN_MEMORY( debug_commands_hga );
  CRUDE_GFX_FGN_MEMORY( joint_matrices_hga );
  CRUDE_GFX_FGN_MEMORY( culled_lights_world_to_texture_hga );

  if ( !ctx->tlas_address )
  {
    return;
  }

  switch ( ctx->geometry_type )
  {
  case CRUDE_GFX_TRANSLUCENT_PASS_GEOMETRY_TYPE_MESHLET:
  {
    crude_gfx_translucent_no_cull_pass_push_constant_ push_constant;
  
    pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "translucent_no_cull_meshlet" );

    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );

    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_translucent_no_cull_pass_push_constant_ );
    push_constant.mesh_instance_draws = meshes_instances_draws_hga.gpu_address;
    push_constant.scene = scene_hga.gpu_address;
    push_constant.mesh_draw_commands = mesh_task_indirect_commands_hga.gpu_address;
    push_constant.visible_mesh_count = mesh_task_indirect_count_hga.gpu_address;
    push_constant.culled_lights = culled_lights_hga.gpu_address;
    push_constant.debug_line_vertices = debug_line_vertices_hga.gpu_address;
    push_constant.debug_counts = debug_commands_hga.gpu_address;
    push_constant.joint_matrices = joint_matrices_hga.gpu_address;
    push_constant.culled_lights_world_to_texture = culled_lights_world_to_texture_hga.gpu_address;
    push_constant.acceleration_structure = ctx->tlas_address;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );
    
    crude_gfx_cmd_draw_mesh_task_indirect_count(
      primary_cmd,
      mesh_task_indirect_commands_hga.buffer_handle,
      CRUDE_OFFSETOF( crude_gfx_mesh_draw_command, indirect_meshlet_group_count_x ),
      mesh_task_indirect_count_hga.buffer_handle,
      CRUDE_OFFSETOF( crude_gfx_mesh_draw_count, transparent_mesh_visible_count ),
      CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws ),
      sizeof( crude_gfx_mesh_draw_command ) );

    break;
  }
  case CRUDE_GFX_TRANSLUCENT_PASS_GEOMETRY_TYPE_CLASSIC:
  {
    crude_gfx_translucent_no_cull_classic_pass_push_constant_ push_constant;

    pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "translucent_no_cull_classic" );

    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );

    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_translucent_no_cull_classic_pass_push_constant_ );
    push_constant.mesh_instance_draws = meshes_instances_draws_hga.gpu_address;
    push_constant.scene = scene_hga.gpu_address;
    push_constant.mesh_draw_commands = mesh_task_indirect_commands_hga.gpu_address;
    push_constant.visible_mesh_count = mesh_task_indirect_count_hga.gpu_address;
    push_constant.culled_lights = culled_lights_hga.gpu_address;
    push_constant.inv_radiance_texture_width = 1.f / gpu->renderer_size.x;
    push_constant.inv_radiance_texture_height = 1.f / gpu->renderer_size.y;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );
    
    crude_gfx_cmd_draw_indirect_count(
      primary_cmd,
      mesh_task_indirect_commands_hga.buffer_handle,
      CRUDE_OFFSETOF( crude_gfx_mesh_draw_command, indirect_mesh_vertex_count ),
      mesh_task_indirect_count_hga.buffer_handle,
      CRUDE_OFFSETOF( crude_gfx_mesh_draw_count, transparent_mesh_visible_count ),
      CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws ),
      sizeof( crude_gfx_mesh_draw_command ) );

    break;
  }
  }
}
