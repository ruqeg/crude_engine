#include <engine/core/profiler.h>
#include <engine/core/hashmapstr.h>

#define RAY_TRACING_SOLID
#include <engine/graphics/shaders/ray_tracing_solid.crude_shader>

#include <engine/graphics/passes/ray_tracing_solid_pass.h>

void
crude_gfx_ray_tracing_solid_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_ray_tracing_solid_pass_push_constant_          push_constant;
  crude_gfx_pipeline_handle                                pipeline;

  CRUDE_GFX_FGN_UTILS();
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_ray_tracing_solid_pass_context );
  CRUDE_GFX_FGN_MEMORY( scene_hga );
  CRUDE_GFX_FGN_MEMORY( meshes_instances_draws_hga );
  CRUDE_GFX_FGN_TEXTURE( ray_tracing_solid_texture );

  if ( !ctx->tlas_address )
  {
    return;
  }

  pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "ray_tracing_solid" );

  crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
  crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

  push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_ray_tracing_solid_pass_push_constant_ );
  push_constant.scene = scene_hga.gpu_address;
  push_constant.mesh_instance_draws = meshes_instances_draws_hga.gpu_address;
  push_constant.sbt_offset = 0u;
  push_constant.sbt_stride = node->graph->gpu->ray_tracing_pipeline_properties.shader_group_handle_alignment;
  push_constant.miss_index = 0u;
  push_constant.out_image_index = ray_tracing_solid_texture->handle.index;
  push_constant.acceleration_structure = ctx->tlas_address;
  crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );
   
  crude_gfx_cmd_add_image_barrier( primary_cmd, ray_tracing_solid_texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, 0, 1, false );
  crude_gfx_cmd_trace_rays( primary_cmd, pipeline, ray_tracing_solid_texture->width, ray_tracing_solid_texture->height, 1u );
  crude_gfx_cmd_add_image_barrier( primary_cmd, ray_tracing_solid_texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1, false );
}
