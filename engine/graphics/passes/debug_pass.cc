#define DEBUG_LINE2D
#define DEBUG_LINE3D
#define DEBUG_CUBE
#include <engine/graphics/shaders/debug.crude_shader>

#include <engine/graphics/passes/debug_pass.h>

void
crude_gfx_debug_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  CRUDE_GFX_FGN_UTILS();
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_debug_pass_context );
  CRUDE_GFX_FGN_MEMORY( scene_hga );
  CRUDE_GFX_FGN_MEMORY( debug_line_vertices_hga );
  CRUDE_GFX_FGN_MEMORY( debug_commands_hga );
  CRUDE_GFX_FGN_MEMORY_OPTIONAL( debug_cubes_instances_hga );

  {
    crude_gfx_pipeline_handle                              pipeline;
    crude_gfx_debug_line3d_pass_push_constant_             push_constant;
    
    pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "debug_line3d" );

    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_debug_line3d_pass_push_constant_ );
    push_constant.debug_lines_vertices = debug_line_vertices_hga.gpu_address;
    push_constant.scene = scene_hga.gpu_address;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

    crude_gfx_cmd_draw_inderect( primary_cmd, debug_commands_hga.buffer_handle, 0u, 1u, sizeof( crude_gfx_debug_counts ) );
  }
  
  {
    crude_gfx_pipeline_handle                              pipeline;
    crude_gfx_debug_line2d_pass_push_constant_             push_constant;
    
    pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "debug_line2d" );

    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_debug_line2d_pass_push_constant_ );
    push_constant.debug_lines_vertices = debug_line_vertices_hga.gpu_address;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

    crude_gfx_cmd_draw_inderect(
      primary_cmd,
      debug_commands_hga.buffer_handle,
      CRUDE_OFFSETOF( crude_gfx_debug_counts, debug_lines_2d_vertices_count ),
      1u, sizeof( crude_gfx_debug_counts ) );
  }

  if ( graph_debug_cubes_instances_hga )
  {
    crude_gfx_pipeline_handle                              pipeline;
    crude_gfx_debug_cube_pass_push_constant_               push_constant;
    
    pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "debug_cube" );

    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_debug_cube_pass_push_constant_ );
    push_constant.scene = scene_hga.gpu_address;
    push_constant.debug_cube_instances = debug_cubes_instances_hga.gpu_address;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );
    
    crude_gfx_cmd_draw_inderect(
      primary_cmd,
      debug_commands_hga.buffer_handle,
      CRUDE_OFFSETOF( crude_gfx_debug_counts, debug_cubes_vertices_count ),
      1u, sizeof( crude_gfx_debug_counts ) );
  }
}
