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

#include <engine/graphics/passes/indirect_light_debug_pass.h>

void
crude_gfx_indirect_light_debug_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_probe_debug_push_constant_                     push_constant;
  crude_gfx_pipeline_handle                                pipeline;

  CRUDE_GFX_FGN_UTILS();
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_indirect_light_debug_pass_context );
  CRUDE_GFX_FGN_MEMORY( scene_hga );
  CRUDE_GFX_FGN_MEMORY( ddgi_hga );
  
  pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "probe_debug" );

  crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
  crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
  
  push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_probe_debug_push_constant_ );
  push_constant.ddgi = ddgi_hga.gpu_address;
  push_constant.scene = scene_hga.gpu_address;
  crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

  crude_gfx_cmd_draw_mesh_task( primary_cmd, 1u, 1u, 1u );
}
