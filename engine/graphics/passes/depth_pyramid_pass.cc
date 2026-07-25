#include "engine/graphics/gpu_resources.h"
#include "engine/graphics/rhi/rhi.h"
#include <engine/core/hashmapstr.h>

#define DEPTH_PYRAMID_FARTHEST
#include <engine/graphics/shaders/depth_pyramid.crude_shader>

#include <engine/graphics/passes/depth_pyramid_pass.h>

void
crude_gfx_depth_pyramid_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_texture                                       *depth_pyramid_texture;
  crude_gfx_texture                                      **depth_pyramid_texture_mips_views;
  crude_gfx_pipeline_handle                                pipeline;
  crude_gfx_depth_pyramid_pass_push_constant_              push_constant;
  int32                                                    width, height;

  CRUDE_GFX_FGN_UTILS( );
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_depth_pyramid_pass_context );
  CRUDE_GFX_FGN_TEXTURE( depth_texture );
  CRUDE_GFX_FGN_TEXTURE_OPTIONAL( depth_pyramid_farthest_texture );
  CRUDE_GFX_FGN_TEXTURE_OPTIONAL( depth_pyramid_nearest_texture );

  CRUDE_GFX_FG_CREATE_TEXTURE_MIPS_VIEWS(
    depth_pyramid_nearest_texture_mips_views,
    depth_pyramid_nearest_texture );

  CRUDE_GFX_FG_CREATE_TEXTURE_MIPS_VIEWS(
    depth_pyramid_farthest_texture_mips_views,
    depth_pyramid_farthest_texture );
 
  depth_pyramid_texture = NULL;
  depth_pyramid_texture_mips_views = NULL;

  switch ( ctx->type )
  {
  case CRUDE_GFX_DEPTH_PYRAMID_PASS_TYPE_FARTHEST:
  {
    pipeline = crude_gfx_access_technique_pipeline_by_name( node->graph->gpu, CRUDE_STRING_VIEW( "depth_pyramid_farthest" ), 0u );
    depth_pyramid_texture = depth_pyramid_farthest_texture;
    depth_pyramid_texture_mips_views = depth_pyramid_farthest_texture_mips_views;
    break;
  }
  case CRUDE_GFX_DEPTH_PYRAMID_PASS_TYPE_NEAREST:
  {
    pipeline = crude_gfx_access_technique_pipeline_by_name( node->graph->gpu, CRUDE_STRING_VIEW( "depth_pyramid_nearest" ), 0u );
    depth_pyramid_texture = depth_pyramid_nearest_texture;
    depth_pyramid_texture_mips_views = depth_pyramid_nearest_texture_mips_views;
    break;
  }
  }

  CRUDE_ASSERT( depth_pyramid_texture );
  CRUDE_ASSERT( depth_pyramid_texture_mips_views );

  crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );

  width = depth_pyramid_texture->width;
  height = depth_pyramid_texture->height;

  crude_gfx_cmd_add_image_barrier( primary_cmd, depth_pyramid_texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0u, depth_pyramid_texture->subresource.mip_level_count, false );

  crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

  for ( uint32 mip_index = 0; mip_index < depth_pyramid_texture->subresource.mip_level_count; ++mip_index )
  {
    uint32                                                 group_x, group_y;

    crude_gfx_cmd_add_image_barrier_ext2( primary_cmd, depth_pyramid_texture->rhi_image, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, mip_index, 1u, false );

    if ( mip_index == 0 )
    {
      push_constant.src_image_index = depth_texture->handle.index;
      push_constant.dst_image_index = depth_pyramid_texture_mips_views[ mip_index ]->handle.index;
    }
    else
    {
      push_constant.src_image_index = depth_pyramid_texture_mips_views[ mip_index - 1 ]->handle.index;
      push_constant.dst_image_index = depth_pyramid_texture_mips_views[ mip_index ]->handle.index;
    }
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

    group_x = ( width + 7 ) / 8;
    group_y = ( height + 7 ) / 8;
    crude_gfx_cmd_dispatch( primary_cmd, group_x, group_y, 1 );

    crude_gfx_cmd_add_image_barrier_ext2( primary_cmd, depth_pyramid_texture->rhi_image, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mip_index, 1u, false );

    width = width / 2;
    height = height / 2;
  }

  depth_pyramid_texture->state = CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}
