#include "engine/core/string.h"
#include "engine/graphics/frame_graph.h"
#include "engine/graphics/rhi/rhi.h"
#include "engine/graphics/scene_renderer_resources.h"
#include <engine/core/profiler.h>
#include <engine/core/hashmapstr.h>
#include <engine/scene/scene_ecs.h>

#define SAMPLE_IRRADIANCE
#define PROBE_UPDATE_IRRADIANCE
#define PROBE_UPDATE_VISIBILITY
#define CALCULATE_PROBE_OFFSETS
#define PROBE_RAYTRACER
#define PROBE_DEBUG
#include <engine/graphics/shaders/ddgi.crude_shader>

#include <engine/graphics/passes/indirect_light_pass.h>

void
crude_gfx_indirect_light_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  CRUDE_GFX_FGN_UTILS();
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_indirect_light_pass_context );
  CRUDE_GFX_FGN_MEMORY( scene_hga );
  CRUDE_GFX_FGN_MEMORY( ddgi_hga );
  CRUDE_GFX_FGN_MEMORY( total_lights_hga );
  CRUDE_GFX_FGN_MEMORY( meshes_instances_draws_hga );
  CRUDE_GFX_FGN_MEMORY( debug_commands_hga );
  CRUDE_GFX_FGN_MEMORY( debug_line_vertices_hga );
  CRUDE_GFX_FGN_TEXTURE( depth_texture );
  CRUDE_GFX_FGN_TEXTURE( gbuffer_normal_texture );
  CRUDE_GFX_FG_GLOBAL_TEXTURE( probe_raytrace_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FG_GLOBAL_TEXTURE( probe_offsets_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FG_GLOBAL_TEXTURE( probe_grid_irradiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FG_GLOBAL_TEXTURE( probe_grid_visibility_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );
  CRUDE_GFX_FG_GLOBAL_TEXTURE( indirect_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS );

  /* Probe Raytracer */
  {
    crude_gfx_probe_raytracer_push_constant_               push_constant;
    crude_gfx_pipeline_handle                              pipeline;
        
    pipeline = crude_gfx_access_technique_pipeline_by_name( gpu, CRUDE_STRING_VIEW( "probe_raytracer" ), 0u );

    crude_gfx_cmd_push_marker( primary_cmd, CRUDE_STRING_VIEW( "probe_raytracer" ) );
    
    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_probe_raytracer_push_constant_ );
    push_constant.ddgi = ddgi_hga.gpu_address;
    push_constant.scene = scene_hga.gpu_address;
    push_constant.mesh_instance_draws = meshes_instances_draws_hga.gpu_address;
    push_constant.total_lights = total_lights_hga.gpu_address;
    push_constant.acceleration_structure = ctx->tlas_address;
    push_constant.probe_raytrace_radiance_texture = probe_raytrace_radiance_texture->handle.index;
    push_constant.probe_offset_texture_index = probe_offsets_texture->handle.index;
    push_constant.grid_irradiance_texture_index = probe_grid_irradiance_texture->handle.index;
    push_constant.grid_visibility_texture_index = probe_grid_visibility_texture->handle.index;
    push_constant.debug_counts = debug_commands_hga.gpu_address;
    push_constant.debug_line_vertices = debug_line_vertices_hga.gpu_address;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );
     
    crude_gfx_cmd_add_image_barrier( primary_cmd, probe_raytrace_radiance_texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, 0, 1, false );
    crude_gfx_cmd_trace_rays( primary_cmd, pipeline, ctx->ddgi_area.probe_rays, ctx->ddgi_area.probe_update_per_frame, 1u );
    crude_gfx_cmd_add_image_barrier( primary_cmd, probe_raytrace_radiance_texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, 0, 1, false );
    crude_gfx_cmd_pop_marker( primary_cmd );
  }
  
  /* Calculate Probe Offsets */
  if ( ctx->ddgi_offsets_calculations_count >= 0 )
  {
    crude_gfx_calculate_probe_offsets_push_constant_       push_constant;
    crude_gfx_pipeline_handle                              pipeline;
    uint32                                                 first_frame;
    
    first_frame = ctx->ddgi_offsets_calculations_count == ctx->ddgi_area.offsets_calculations_count ? 1 : 0;
    
    pipeline = crude_gfx_access_technique_pipeline_by_name( gpu, CRUDE_STRING_VIEW( "calculate_probe_offsets" ), 0u );

    crude_gfx_cmd_push_marker( primary_cmd, CRUDE_STRING_VIEW( "calculate_probe_offsets" ) );
    
    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
    
    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_calculate_probe_offsets_push_constant_ );
    push_constant.ddgi = ddgi_hga.gpu_address;
    push_constant.first_frame = first_frame;
    push_constant.probe_raytrace_radiance_texture = probe_raytrace_radiance_texture->handle.index;
    push_constant.probe_offset_texture_index = probe_offsets_texture->handle.index;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

    crude_gfx_cmd_dispatch( primary_cmd, ( crude_gfx_ddgi_area_cpu_calculate_probes_count( &ctx->ddgi_area ) + 31 ) / 32, 1u, 1 );

    crude_gfx_cmd_pop_marker( primary_cmd );
  }

  /* Probe Update Irradiance */
  {
    crude_gfx_probe_update_push_constant_                  push_constant;
    crude_gfx_pipeline_handle                              pipeline;
    
    pipeline = crude_gfx_access_technique_pipeline_by_name( gpu, CRUDE_STRING_VIEW( "probe_update_irradiance" ), 0u );
    
    crude_gfx_cmd_push_marker( primary_cmd, CRUDE_STRING_VIEW( "probe_update_irradiance" ) );

    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
    
    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_probe_update_push_constant_ );
    push_constant.ddgi = ddgi_hga.gpu_address;
    push_constant.irradiance_image_index = probe_grid_irradiance_texture->handle.index;
    push_constant.visibility_image_index = probe_grid_visibility_texture->handle.index;
    push_constant.probe_raytrace_radiance_texture = probe_raytrace_radiance_texture->handle.index;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

    crude_gfx_cmd_dispatch( primary_cmd, 
      ( crude_gfx_ddgi_area_cpu_calculate_irradiance_side_length_with_borders( &ctx->ddgi_area ) + 7 ) / 8,
      ( crude_gfx_ddgi_area_cpu_calculate_irradiance_side_length_with_borders( &ctx->ddgi_area ) + 7 ) / 8,
      ctx->ddgi_area.probe_update_per_frame );

    crude_gfx_cmd_add_image_barrier( primary_cmd, probe_grid_irradiance_texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, 0, 1, false );

    crude_gfx_cmd_pop_marker( primary_cmd );
  }
  
  /* Probe Update Visibility */
  {
    crude_gfx_probe_update_push_constant_                  push_constant;
    crude_gfx_pipeline_handle                              pipeline;
    
    pipeline = crude_gfx_access_technique_pipeline_by_name( gpu, CRUDE_STRING_VIEW( "probe_update_visibility" ), 0u );
    
    crude_gfx_cmd_push_marker( primary_cmd, CRUDE_STRING_VIEW( "probe_update_visibility" ) );
    
    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
    
    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_probe_update_push_constant_ );
    push_constant.ddgi = ddgi_hga.gpu_address;
    push_constant.irradiance_image_index = probe_grid_irradiance_texture->handle.index;
    push_constant.visibility_image_index = probe_grid_visibility_texture->handle.index;
    push_constant.probe_raytrace_radiance_texture = probe_raytrace_radiance_texture->handle.index;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

    crude_gfx_cmd_dispatch( primary_cmd,
      ( crude_gfx_ddgi_area_cpu_calculate_visibility_side_length_with_borders( &ctx->ddgi_area ) + 7 ) / 8,
      ( crude_gfx_ddgi_area_cpu_calculate_visibility_side_length_with_borders( &ctx->ddgi_area ) + 7 ) / 8,
      ctx->ddgi_area.probe_update_per_frame );

    crude_gfx_cmd_add_image_barrier( primary_cmd, probe_grid_visibility_texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, 0, 1, false );

    crude_gfx_cmd_pop_marker( primary_cmd );
  }

  /* Sample Irradiance */
  {
    crude_gfx_sample_irradiance_push_constant_             push_constant;
    crude_gfx_pipeline_handle                              pipeline;
    uint32                                                 width, height;
    
    width = gpu->renderer_size.x;
    height = gpu->renderer_size.y;

    pipeline = crude_gfx_access_technique_pipeline_by_name( gpu, CRUDE_STRING_VIEW( "sample_irradiance" ), 0u );
    
    crude_gfx_cmd_push_marker( primary_cmd, CRUDE_STRING_VIEW( "sample_irradiance" ) );

    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
    
    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_sample_irradiance_push_constant_ );
    push_constant.ddgi = ddgi_hga.gpu_address;
    push_constant.scene = scene_hga.gpu_address;
    push_constant.depth_texture_index = depth_texture->handle.index;
    push_constant.indirect_radiance_index = indirect_radiance_texture->handle.index;
    push_constant.gbuffer_normal_texture_index = gbuffer_normal_texture->handle.index;
    push_constant.probe_offset_texture_index = probe_offsets_texture->handle.index;
    push_constant.grid_irradiance_texture_index = probe_grid_irradiance_texture->handle.index;
    push_constant.grid_visibility_texture_index = probe_grid_visibility_texture->handle.index;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

    crude_gfx_cmd_dispatch( primary_cmd, ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
    
    crude_gfx_cmd_pop_marker( primary_cmd );
  }
}
