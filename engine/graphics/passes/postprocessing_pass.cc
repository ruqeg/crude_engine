#include <engine/core/hashmapstr.h>
#include <engine/core/time.h>

#define LUMINANCE_AVERAGE_CALCULATION 
#define LUMINANCE_HISTOGRAM_GENERATION
#define POSTPROCESSING
#include <engine/graphics/shaders/postprocessing.crude_shader>
#include <engine/graphics/shaders/luminance.crude_shader>

#include <engine/graphics/passes/postprocessing_pass.h>

void
crude_gfx_luminance_calculation_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_pipeline_handle                                luminance_average_calculation_pipeline;
  crude_gfx_pipeline_handle                                luminance_histogram_generation_pipeline;

  CRUDE_GFX_FGN_UTILS();
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_luminance_calculation_pass_context );
  CRUDE_GFX_FGN_MEMORY( luminance_histogram_hga );
  CRUDE_GFX_FGN_TEXTURE( radiance_texture );
  CRUDE_GFX_FGN_TEXTURE( luminance_average_texture );

  
  CRUDE_GFX_CMD_PUSH_MARKER( primary_cmd, "luminance_histogram_generation_pass" );
  {
    crude_gfx_luminance_histogram_generation_pass_push_constant_ push_constant;

    luminance_histogram_generation_pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "luminance_histogram_generation" );
  
    crude_gfx_cmd_bind_pipeline( primary_cmd, luminance_histogram_generation_pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
  
    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_luminance_histogram_generation_pass_push_constant_ );
    push_constant.histogram = luminance_histogram_hga.gpu_address;
    push_constant.hdr_color_texture_index = radiance_texture->handle.index;
    push_constant.inverse_log_lum_range = 1.f / ( ctx->max_log_lum - ctx->min_log_lum );
    push_constant.min_log_lum = ctx->min_log_lum;
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );
  
    crude_gfx_cmd_dispatch( primary_cmd, ( radiance_texture->width + 15u ) / 16u, ( radiance_texture->height + 15u ) / 16u, 1u );
  }
  CRUDE_GFX_CMD_POP_MARKER( primary_cmd );
  
  CRUDE_GFX_CMD_PUSH_MARKER( primary_cmd, "luminance_average_calculation_pass" );
  {
    crude_gfx_luminance_average_calculation_pass_push_constant_ push_constant;

    luminance_average_calculation_pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "luminance_average_calculation" );
    crude_gfx_cmd_bind_pipeline( primary_cmd, luminance_average_calculation_pipeline );
    crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
 
    push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_luminance_average_calculation_pass_push_constant_ );
    push_constant.histogram = luminance_histogram_hga.gpu_address;
    push_constant.luminance_avarage_texture = luminance_average_texture->handle.index;
    push_constant.log_lum_range = ( ctx->max_log_lum - ctx->min_log_lum );
    push_constant.min_log_lum = ctx->min_log_lum;
    push_constant.num_pixels = radiance_texture->width * radiance_texture->height;
    push_constant.time_coeff = CRUDE_CLAMP( ( 1 - exp( -ctx->luminance_avarge_last_update_delta_time * 1.1f ) ), 1.0, 0.001 );
    crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );
  
    crude_gfx_cmd_add_image_barrier( primary_cmd, luminance_average_texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_UNORDERED_ACCESS, 0u, 1u, false );
    crude_gfx_cmd_dispatch( primary_cmd, 1u, 1u, 1u );
    crude_gfx_cmd_add_image_barrier( primary_cmd, luminance_average_texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0u, 1u, false );
  }
  CRUDE_GFX_CMD_POP_MARKER( primary_cmd );
}

void
crude_gfx_postprocessing_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  crude_gfx_postprocessing_pass_push_constant_             push_constant;
  crude_gfx_pipeline_handle                                postprocessing_pipeline;

  CRUDE_GFX_FGN_UTILS();
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_postprocessing_pass_context );
  CRUDE_GFX_FGN_TEXTURE( radiance_texture );
  CRUDE_GFX_FGN_TEXTURE( luminance_average_texture );

  postprocessing_pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "postprocessing" );
  
  CRUDE_GFX_CMD_PUSH_MARKER( primary_cmd, "postprocessing_main_pass" );

  crude_gfx_cmd_bind_pipeline( primary_cmd, postprocessing_pipeline );
  crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );

  push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_postprocessing_pass_push_constant_ );
  push_constant.luminance_average_texture_index = luminance_average_texture->handle.index;
  push_constant.pbr_texture_index = radiance_texture->handle.index;
  push_constant.inv_gamma = 1.f / ctx->gamma;
  crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

  crude_gfx_cmd_draw( primary_cmd, 0u, 3u, 0u, 1u );

  CRUDE_GFX_CMD_POP_MARKER( primary_cmd );
}
