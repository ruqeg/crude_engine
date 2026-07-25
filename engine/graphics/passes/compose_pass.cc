#include "engine/graphics/rhi/rhi.h"
#include <engine/core/profiler.h>
#include <engine/core/hashmapstr.h>
#include <engine/scene/scene_ecs.h>

#define COMPOSE_LIGHT
#include <engine/graphics/shaders/compose.crude_shader>

#include <engine/graphics/passes/compose_pass.h>

void
crude_gfx_compose_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_pipeline_handle                                pipeline;
  crude_gfx_compose_light_pass_push_constant_              pust_constant;
  uint32                                                   flags;

  CRUDE_GFX_FGN_UTILS();
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_compose_pass_context );
  CRUDE_GFX_FGN_TEXTURE( gbuffer_albedo_texture );
  CRUDE_GFX_FGN_TEXTURE( gbuffer_radiance_texture );
  CRUDE_GFX_FGN_TEXTURE( gbuffer_roughness_metalness_texture );
  CRUDE_GFX_FGN_TEXTURE( radiance_texture );
  CRUDE_GFX_FGN_GLOBAL_TEXTURE_OPTIONAL( indirect_radiance_texture, CRUDE_GFX_RHI_RESOURCE_STATE_SHADER_RESOURCE );
  CRUDE_GFX_FGN_TEXTURE_OPTIONAL( ssr_radiance_texture );

  flags = 0u;
  if ( ssr_radiance_texture )
  {
    flags |= COMPOSE_LIGHT_FLAG_SSR;
  }
  if ( indirect_radiance_texture )
  {
    flags |= COMPOSE_LIGHT_FLAG_INDIRECT;
  }

  pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME_WITH_FLAGS( "compose_light", flags );
  crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
  
  pust_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_compose_light_pass_push_constant_ );
  pust_constant.direct_radiance_texture_index = gbuffer_radiance_texture->handle.index;
  pust_constant.radiance_texture_index = radiance_texture->handle.index;
  pust_constant.albedo_texture_index = gbuffer_albedo_texture->handle.index;
  pust_constant.roughness_metalness_texture_index = gbuffer_roughness_metalness_texture->handle.index;
  pust_constant.ssr_radiance_texture_index = ssr_radiance_texture ? ssr_radiance_texture->handle.index : -1;
  pust_constant.indirect_radiance_texture_index = indirect_radiance_texture ? indirect_radiance_texture->handle.index : -1;
  pust_constant.inv_resolution.x = 1.f / radiance_texture->width;
  pust_constant.inv_resolution.y = 1.f / radiance_texture->height;
  crude_gfx_cmd_push_constant( primary_cmd, &pust_constant, sizeof( pust_constant ) );

  crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

  crude_gfx_cmd_dispatch( primary_cmd, ( radiance_texture->width + 7 ) / 8, ( radiance_texture->height + 7 ) / 8, 1u );
}
