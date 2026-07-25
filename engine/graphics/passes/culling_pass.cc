#include <engine/core/hashmapstr.h>
#include <engine/graphics/scene_renderer_resources.h>

#define CULLING_LATE
#include <engine/graphics/shaders/culling.crude_shader>

#include <engine/graphics/passes/culling_pass.h>

void
crude_gfx_culling_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_pipeline_handle                                culling_pipeline;
  crude_gfx_culling_pass_push_constant_                    push_constant;

  CRUDE_GFX_FGN_CONTEXT( crude_gfx_culling_pass_context );
  CRUDE_GFX_FGN_MEMORY( scene_hga );
  CRUDE_GFX_FGN_MEMORY( meshes_instances_draws_hga );
  CRUDE_GFX_FGN_MEMORY( mesh_task_indirect_commands_hga );
  CRUDE_GFX_FGN_MEMORY( mesh_task_indirect_commands_culled_hga );
  CRUDE_GFX_FGN_MEMORY( mesh_task_indirect_count_hga );
  CRUDE_GFX_FGN_MEMORY( debug_line_vertices_hga );
  CRUDE_GFX_FGN_MEMORY( debug_commands_hga );

  if ( CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws ) == 0 )
  {
    return;
  }

  if ( ctx->culling_type == CRUDE_GFX_CULLING_PASS_TYPE_EARLY )
  {
    culling_pipeline = crude_gfx_access_technique_pipeline_by_name( node->graph->gpu, CRUDE_STRING_VIEW( "culling_early" ), 0u );
  }
  else if ( ctx->culling_type == CRUDE_GFX_CULLING_PASS_TYPE_LATE )
  {
    culling_pipeline = crude_gfx_access_technique_pipeline_by_name( node->graph->gpu, CRUDE_STRING_VIEW( "culling_late" ), 0u );
  }
  else
  {
    CRUDE_ASSERT( false );
  }

  crude_gfx_cmd_bind_pipeline( primary_cmd, culling_pipeline );

  push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_culling_pass_push_constant_ );
  push_constant.scene = scene_hga.gpu_address;
  push_constant.mesh_instance_draws = meshes_instances_draws_hga.gpu_address;
  push_constant.mesh_draw_commands = mesh_task_indirect_commands_hga.gpu_address;
  push_constant.mesh_draw_commands_culled = mesh_task_indirect_commands_culled_hga.gpu_address;
  push_constant.mesh_draw_count = mesh_task_indirect_count_hga.gpu_address;
  push_constant.debug_line_vertices = debug_line_vertices_hga.gpu_address;
  push_constant.debug_counts = debug_commands_hga.gpu_address;
  crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

  crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

  crude_gfx_cmd_dispatch( primary_cmd, ( CRUDE_ARRAY_LENGTH( ctx->scene.meshes_instances_draws ) + 63u ) / 64u, 1u, 1u );
}
