#include "engine/graphics/rhi/rhi.h"
#include <engine/core/profiler.h>
#include <engine/scene/scene_ecs.h>

#define SSR_COMPOSE
#define SSR_CONVOLVE_VERTICAL
#define SSR_HIT_CALCULATION_HIZ
#include <engine/graphics/shaders/ssr.crude_shader>

#include <engine/graphics/passes/ssr_pass.h>

void
crude_gfx_ssr_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  CRUDE_GFX_FGN_UTILS();
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_ssr_pass_context );
  CRUDE_GFX_FGN_MEMORY( scene_hga );
  CRUDE_GFX_FGN_TEXTURE( ssr_radiance_texture );
  CRUDE_GFX_FGN_TEXTURE( gbuffer_radiance_texture );
  CRUDE_GFX_FGN_TEXTURE( gbuffer_normal_texture );
  CRUDE_GFX_FGN_TEXTURE( gbuffer_roughness_metalness_texture );
  CRUDE_GFX_FGN_TEXTURE( ssr_radiance_hierarchy_texture );
  CRUDE_GFX_FGN_TEXTURE( ssr_radiance_hierarchy_temporary_texture );
  CRUDE_GFX_FGN_TEXTURE( ssr_hit_uv_depth_rdotv_texture );
  CRUDE_GFX_FGN_TEXTURE( depth_pyramid_nearest_texture );

  CRUDE_GFX_FG_CREATE_TEXTURE_MIPS_VIEWS(
    ssr_radiance_hierarchy_temporary_texture_mips_views,
    ssr_radiance_hierarchy_temporary_texture );

  CRUDE_GFX_FG_CREATE_TEXTURE_MIPS_VIEWS(
    ssr_radiance_hierarchy_texture_mips_views,
    ssr_radiance_hierarchy_texture );

  /* SSR Hit Calculation Pass  */
  {
    crude_gfx_pipeline_handle                              pipeline;
    crude_gfx_ssr_hit_calculation_pass_push_constant_      pust_constant;

    crude_gfx_cmd_push_marker( primary_cmd, CRUDE_STRING_VIEW( "ssr_hit_calculation_hiz" ) );

    pipeline = crude_gfx_access_technique_pipeline_by_name( gpu, CRUDE_STRING_VIEW( "ssr_hit_calculation_hiz" ) , 0u );
    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );

    pust_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_ssr_hit_calculation_pass_push_constant_ );
    pust_constant.scene = scene_hga.gpu_address;
    pust_constant.ssr_max_steps = ctx->ssr.max_steps;
    pust_constant.normal_texture_index = gbuffer_normal_texture->handle.index;
    pust_constant.ssr_hit_uv_depth_rdotv_texture_index = ssr_hit_uv_depth_rdotv_texture->handle.index;
    pust_constant.depth_pyramid_nearest_texture_index = depth_pyramid_nearest_texture->handle.index;

    crude_gfx_cmd_push_constant( primary_cmd, &pust_constant, sizeof( pust_constant ) );

    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

    crude_gfx_cmd_dispatch( primary_cmd, ( depth_pyramid_nearest_texture->width + 7 ) / 8, ( depth_pyramid_nearest_texture->height + 7 ) / 8, 1u );

    crude_gfx_cmd_pop_marker( primary_cmd );
  }

  /* SSR Blurring Pass  */
  {
    /* First Mip */
    {
      crude_gfx_rhi_image_blit                               blit_region;

      blit_region = CRUDE_COMPOUNT_EMPTY( crude_gfx_rhi_image_blit );

      blit_region.src_subresource.aspect_mask = CRUDE_GFX_RHI_IMAGE_ASPECT_COLOR_BIT;
      blit_region.src_subresource.mip_level = 0;
      blit_region.src_subresource.base_array_layer = 0;
      blit_region.src_subresource.layer_count = 1;
      
      blit_region.src_offsets[ 0 ].x = 0;
      blit_region.src_offsets[ 0 ].y = 0;
      blit_region.src_offsets[ 0 ].z = 0;
      blit_region.src_offsets[ 1 ].x = gbuffer_radiance_texture->width;
      blit_region.src_offsets[ 1 ].y = gbuffer_radiance_texture->height;
      blit_region.src_offsets[ 1 ].z = 1;

      blit_region.dst_subresource.aspect_mask = CRUDE_GFX_RHI_IMAGE_ASPECT_COLOR_BIT;
      blit_region.dst_subresource.mip_level = 0;
      blit_region.dst_subresource.base_array_layer = 0;
      blit_region.dst_subresource.layer_count = 1;

      blit_region.dst_offsets[ 0 ].x = 0;
      blit_region.dst_offsets[ 0 ].y = 0;
      blit_region.dst_offsets[ 0 ].z = 0;
      blit_region.dst_offsets[ 1 ].x = ssr_radiance_hierarchy_texture->width;
      blit_region.dst_offsets[ 1 ].y = ssr_radiance_hierarchy_texture->height;
      blit_region.dst_offsets[ 1 ].z = 1;

      crude_gfx_cmd_add_image_barrier_ext2( primary_cmd, ssr_radiance_hierarchy_texture->rhi_image, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST, 0, 1u, false );
      crude_gfx_rhi_command_buffer_blit_image(
        primary_cmd->rhi_cmd_buffer,
        gbuffer_radiance_texture->rhi_image,
        CRUDE_GFX_RHI_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        ssr_radiance_hierarchy_texture->rhi_image,
        CRUDE_GFX_RHI_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &blit_region, 
        CRUDE_GFX_RHI_FILTER_LINEAR );
      crude_gfx_cmd_add_image_barrier_ext2( primary_cmd, ssr_radiance_hierarchy_texture->rhi_image, CRUDE_GFX_RHI_RESOURCE_STATE_COPY_DEST, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1u, false );
    }

    /* Convolve Vertical */
    {
      crude_gfx_pipeline_handle                            pipeline;
      crude_gfx_convolve_pass_push_constant_               pust_constant;
      uint64                                               mip_width, mip_height;
      
      mip_width = ssr_radiance_hierarchy_texture->width;
      mip_height = ssr_radiance_hierarchy_texture->height;

      CRUDE_GFX_CMD_PUSH_MARKER( primary_cmd, "ssr_convolve_vertical" );
      
      pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "ssr_convolve_vertical" );
      crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
      
      crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
      
      crude_gfx_cmd_add_image_barrier_ext2( primary_cmd, ssr_radiance_hierarchy_texture->rhi_image, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 1u, false );

      for ( uint32 mip_index = 0; mip_index < ssr_radiance_hierarchy_texture->subresource.mip_level_count - 1; ++mip_index )
      {
        uint64                                               prev_mip_width, prev_mip_height;
      
        prev_mip_width = mip_width;
        prev_mip_height = mip_height;
      
        mip_width = mip_width / 2;
        mip_height = mip_height / 2;
      
        crude_gfx_cmd_add_image_barrier_ext2( primary_cmd, ssr_radiance_hierarchy_texture->rhi_image, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, mip_index, 1u, false );
        
        pust_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_convolve_pass_push_constant_ );
        pust_constant.dst_texture_index = ssr_radiance_hierarchy_temporary_texture_mips_views[ mip_index ]->handle.index;
        pust_constant.src_texture_index = ( mip_index == 0 ) ? ssr_radiance_hierarchy_texture_mips_views[ 0 ]->handle.index : ssr_radiance_hierarchy_temporary_texture_mips_views[ mip_index - 1 ]->handle.index;
        pust_constant.src_div_dst_texture_size.x = prev_mip_width / CRUDE_CAST( float32, mip_width );
        pust_constant.src_div_dst_texture_size.y = prev_mip_height / CRUDE_CAST( float32, mip_height );

        crude_gfx_cmd_push_constant( primary_cmd, &pust_constant, sizeof( pust_constant ) );
        
        crude_gfx_cmd_dispatch( primary_cmd, ( mip_width + 7 ) / 8, ( mip_height + 7 ) / 8, 1 );
        
        crude_gfx_cmd_add_image_barrier_ext2( primary_cmd, ssr_radiance_hierarchy_texture->rhi_image, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mip_index, 1u, false );
      }

      ssr_radiance_hierarchy_texture->state = CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

      CRUDE_GFX_CMD_POP_MARKER( primary_cmd  );
    }

    /* Convolve Horizontal */
    {
      crude_gfx_pipeline_handle                            pipeline;
      crude_gfx_convolve_pass_push_constant_               pust_constant;
      uint64                                               mip_width, mip_height;
      
      mip_width = ssr_radiance_hierarchy_texture->width;
      mip_height = ssr_radiance_hierarchy_texture->height;

      CRUDE_GFX_CMD_PUSH_MARKER( primary_cmd, "ssr_convolve_vertical" );
      
      pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "ssr_convolve_vertical" );
      crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
      
      crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

      for ( uint32 mip_index = 1; mip_index < ssr_radiance_hierarchy_texture->subresource.mip_level_count; ++mip_index )
      {
        uint64                                               prev_mip_width, prev_mip_height;
      
        prev_mip_width = mip_width;
        prev_mip_height = mip_height;
      
        mip_width = mip_width / 2;
        mip_height = mip_height / 2;
      
        crude_gfx_cmd_add_image_barrier_ext2( primary_cmd, ssr_radiance_hierarchy_texture->rhi_image, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, mip_index, 1u, false );
        
        pust_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_convolve_pass_push_constant_ );
        pust_constant.dst_texture_index = ssr_radiance_hierarchy_texture_mips_views[ mip_index ]->handle.index;
        pust_constant.src_texture_index = ( mip_index == 1 ) ? ssr_radiance_hierarchy_texture_mips_views[ 0 ]->handle.index : ssr_radiance_hierarchy_temporary_texture_mips_views[ mip_index - 2 ]->handle.index;
        pust_constant.src_div_dst_texture_size.x = prev_mip_width / CRUDE_CAST( float32, mip_width );
        pust_constant.src_div_dst_texture_size.y = prev_mip_height / CRUDE_CAST( float32, mip_height );

        crude_gfx_cmd_push_constant( primary_cmd, &pust_constant, sizeof( pust_constant ) );
        
        crude_gfx_cmd_dispatch( primary_cmd, ( mip_width + 7 ) / 8, ( mip_height + 7 ) / 8, 1 );
        
        crude_gfx_cmd_add_image_barrier_ext2( primary_cmd, ssr_radiance_hierarchy_texture->rhi_image, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mip_index, 1u, false );
      }

      ssr_radiance_hierarchy_texture->state = CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

      CRUDE_GFX_CMD_POP_MARKER( primary_cmd  );
    }
  }

  /* SSR Resolve Pass (Cone Tracing ) */
  {
    crude_gfx_pipeline_handle                            pipeline;
    crude_gfx_ssr_compose_pass_push_constant_            pust_constant;
    
    CRUDE_GFX_CMD_PUSH_MARKER( primary_cmd, "ssr_compose" );

    pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "ssr_compose" );
    
    crude_gfx_cmd_bind_pipeline( primary_cmd, pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

    pust_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_ssr_compose_pass_push_constant_ );
    pust_constant.scene = scene_hga.gpu_address;
    pust_constant.ssr_hit_uv_depth_rdotv_texture_index = ssr_hit_uv_depth_rdotv_texture->handle.index;
    pust_constant.output_texture_index = ssr_radiance_texture->handle.index;
    pust_constant.normal_texture_index = gbuffer_normal_texture->handle.index;
    pust_constant.radiance_hierarchy_texture_index = ssr_radiance_hierarchy_texture->handle.index;
    pust_constant.packed_roughness_metalness_texture_index = gbuffer_roughness_metalness_texture->handle.index;
    pust_constant.radiance_hierarchy_mips_count = ssr_radiance_hierarchy_texture->handle.index;
    pust_constant.fade_end = ctx->ssr.fade_end;
    pust_constant.fade_start = ctx->ssr.fade_start;
    crude_gfx_cmd_push_constant( primary_cmd, &pust_constant, sizeof( pust_constant ) );

    crude_gfx_cmd_dispatch( primary_cmd, ( ssr_radiance_hierarchy_texture->width + 7 ) / 8, ( ssr_radiance_hierarchy_texture->height + 7 ) / 8, 1u );

    CRUDE_GFX_CMD_POP_MARKER( primary_cmd );
  }
}
