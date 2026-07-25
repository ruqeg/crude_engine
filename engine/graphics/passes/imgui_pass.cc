#include <engine/graphics/imgui.h>

#define IMGUI 1
#include <engine/graphics/shaders/imgui.crude_shader>

#include <engine/graphics/passes/imgui_pass.h>

void
crude_gfx_imgui_generate_buffers_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  ImDrawData                                              *imgui_draw_data;
  ImDrawVert                                              *im_draw_vertices;
  ImDrawIdx                                               *im_draw_indices;
  crude_gfx_memory_allocation                              im_draw_indices_tca;
  crude_gfx_memory_allocation                              im_draw_vertices_tca;
  uint64                                                   draw_counts, vertex_size, index_size;

  CRUDE_GFX_FGN_UTILS( );
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_imgui_generate_buffers_pass_context );
  CRUDE_GFX_FGN_MEMORY( imgui_vertices_hga );
  CRUDE_GFX_FGN_MEMORY( imgui_indices_hga );

  if ( crude_gfx_rhi_get_type( ) == CRUDE_GFX_RHI_TYPE_NULL )
  {
    return;
  }

  ImGui::SetCurrentContext( CRUDE_CAST( ImGuiContext*, ctx->imgui_context ) );

  ImGui::Render();
  imgui_draw_data = ImGui::GetDrawData();
  
  draw_counts = imgui_draw_data->CmdListsCount;
  for ( int32 n = 0; n < draw_counts; ++n )
  {
    ImDrawList const *cmd_list = imgui_draw_data->CmdLists[ n ];
    for ( int32 cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i )
    {
      const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[ cmd_i ];
      crude_gfx_texture_handle texture_handle = *( crude_gfx_texture_handle* )( pcmd->TexRef.GetTexID() );
      if ( texture_handle.index != ctx->imgui_font_texture_handle.index )
      {
        //crude_gfx_texture *texture = crude_gfx_access_texture( gpu, texture_handle );
        //crude_gfx_cmd_add_image_barrier( primary_cmd, texture, CRUDE_GFX_RHI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0u, 1u, crude_gfx_rhi_format_has_depth( texture->vk_format ) );
      }
    }
  }

  /* Vulkan backend has a different origin than OpenGL. */
  vertex_size = imgui_draw_data->TotalVtxCount * sizeof( ImDrawVert );
  index_size = imgui_draw_data->TotalIdxCount * sizeof( ImDrawIdx );
  
  if ( vertex_size >= imgui_vertices_hga.size || index_size >= imgui_indices_hga.size )
  {
    CRUDE_LOG_ERROR( CRUDE_CHANNEL_GRAPHICS, "ImGui Backend Error: vertex/index overflow!" );
    return;
  }
  
  if ( vertex_size == 0 && index_size == 0 )
  {
    return;
  }

  im_draw_vertices_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, vertex_size );
  im_draw_indices_tca = crude_gfx_linear_allocator_allocate( &gpu->frame_linear_allocator, index_size );

  im_draw_vertices = CRUDE_CAST( ImDrawVert*, im_draw_vertices_tca.cpu_address );
  im_draw_indices = CRUDE_CAST( ImDrawIdx*, im_draw_indices_tca.cpu_address );
  
  for ( uint64 i = 0; i < imgui_draw_data->CmdListsCount; i++ )
  {
    ImDrawList const* cmd_list = imgui_draw_data->CmdLists[ i ];
    
    memcpy( im_draw_vertices, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) );
    im_draw_vertices += cmd_list->VtxBuffer.Size;

    memcpy( im_draw_indices, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ) );
    im_draw_indices += cmd_list->IdxBuffer.Size;
  }
  
  crude_gfx_cmd_memory_copy( primary_cmd, im_draw_vertices_tca, imgui_vertices_hga, 0, 0 );
  crude_gfx_cmd_memory_copy( primary_cmd, im_draw_indices_tca, imgui_indices_hga, 0, 0 );
}

void
crude_gfx_imgui_pass_queue
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
)
{
  ImDrawData                                              *imgui_draw_data;
  crude_gfx_pipeline_handle                                imgui_pipeline;
  crude_gfx_imgui_pass_push_constant_                      push_constant;
  crude_gfx_viewport                                       dev_viewport;
  XMFLOAT4X4                                               ortho_projection;
  ImVec2                                                   clip_off, clip_scale;
  uint64                                                   vtx_buffer_offset, index_buffer_offset;
  int32                                                    draw_counts, framebuffer_width, framebuffer_height;
  bool                                                     clip_origin_lower_left;

  CRUDE_GFX_FGN_UTILS( );
  CRUDE_GFX_FGN_CONTEXT( crude_gfx_imgui_pass_context );
  CRUDE_GFX_FGN_MEMORY( imgui_vertices_hga );
  CRUDE_GFX_FGN_MEMORY( imgui_indices_hga );
  CRUDE_GFX_FGN_TEXTURE( postprocessed_texture );

  if ( crude_gfx_rhi_get_type( ) == CRUDE_GFX_RHI_TYPE_NULL )
  {
    return;
  }

  ImGui::SetCurrentContext( CRUDE_CAST( ImGuiContext*, ctx->imgui_context ) );
  imgui_draw_data = ImGui::GetDrawData();
  
  /* Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates) */
  framebuffer_width = (int)( imgui_draw_data->DisplaySize.x * imgui_draw_data->FramebufferScale.x );
  framebuffer_height = (int)( imgui_draw_data->DisplaySize.y * imgui_draw_data->FramebufferScale.y );
  if ( framebuffer_width <= 0 || framebuffer_height <= 0 )
  {
    return;
  }
  
  /* Vulkan backend has a different origin than OpenGL. */
  clip_origin_lower_left = false;
  
  if ( imgui_draw_data->TotalVtxCount == 0 && imgui_draw_data->TotalIdxCount == 0 )
  {
    return;
  }
  
  // !TODO add the sorting
  imgui_pipeline = CRUDE_GFX_ACCESS_TECHNIQUE_PIPELINE_BY_NAME( "imgui" );
  crude_gfx_cmd_bind_pipeline( primary_cmd, imgui_pipeline );
  
  dev_viewport = { 0, 0, ( uint16 )framebuffer_width, ( uint16 )framebuffer_height, 0.0f, 1.0f };
  crude_gfx_cmd_set_viewport( primary_cmd, &dev_viewport ); 
  
  /* Setup dev_viewport, orthographic projection matrix */
  /* Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayMin is typically (0,0) for single dev_viewport apps. */
  {
    float32 l = imgui_draw_data->DisplayPos.x;
    float32 r = imgui_draw_data->DisplayPos.x + imgui_draw_data->DisplaySize.x;
    float32 t = imgui_draw_data->DisplayPos.y;
    float32 b = imgui_draw_data->DisplayPos.y + imgui_draw_data->DisplaySize.y;
    XMStoreFloat4x4( &ortho_projection, XMMatrixSet(
      2.0f / ( r - l ), 0.0f, 0.0f, 0.0f,
      0.0f, 2.0f / ( t - b ), 0.0f, 0.0f,
      0.0f, 0.0f, -1.0f, 0.0f,
      ( r + l ) / ( l - r ), ( t + b ) / ( b - t ), 0.0f, 1.0f
    ) );
  }

  crude_gfx_cmd_bind_bindless_descriptor_set( primary_cmd );
  
  /* Will project scissor/clipping rectangles into framebuffer space */
  clip_off = imgui_draw_data->DisplayPos;         /* (0,0) unless using multi-viewports */
  clip_scale = imgui_draw_data->FramebufferScale; /* (1,1) unless using retina display which are often (2,2) */
  
  /* Render command lists */
  draw_counts = imgui_draw_data->CmdListsCount;
  
  vtx_buffer_offset = 0;
  index_buffer_offset = 0;
  for ( int32 n = 0; n < draw_counts; ++n )
  {
    ImDrawList const *cmd_list = imgui_draw_data->CmdLists[ n ];
    
    for ( int32 cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i )
    {
      const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[ cmd_i ];
      if ( pcmd->UserCallback )
      {
        /* User callback (registered via ImDrawList::AddCallback) */
        pcmd->UserCallback( cmd_list, pcmd );
      }
      else
      {
        /* Project scissor/clipping rectangles into framebuffer space */
        ImVec4 clip_rect;
        clip_rect.x = CRUDE_MAX( ( pcmd->ClipRect.x - clip_off.x ) * clip_scale.x, 0 );
        clip_rect.y = CRUDE_MAX( ( pcmd->ClipRect.y - clip_off.y ) * clip_scale.y, 0 );
        clip_rect.z = CRUDE_MAX( ( pcmd->ClipRect.z - clip_off.x ) * clip_scale.x, 0 );
        clip_rect.w = CRUDE_MAX( ( pcmd->ClipRect.w - clip_off.y ) * clip_scale.y, 0 );

        if ( clip_rect.x < framebuffer_width && clip_rect.y < framebuffer_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f )
        {
          /* Apply scissor/clipping rectangle */
          if ( clip_origin_lower_left )
          {
            crude_gfx_rect2d_int scissor_rect = { (int16_t)clip_rect.x, (int16_t)( framebuffer_height - clip_rect.w ), (uint16_t)( clip_rect.z - clip_rect.x ), (uint16_t)( clip_rect.w - clip_rect.y ) };
            crude_gfx_cmd_set_scissor( primary_cmd, &scissor_rect );
          }
          else
          {
            crude_gfx_rect2d_int scissor_rect = { (int16_t)clip_rect.x, (int16_t)clip_rect.y, (uint16_t)( clip_rect.z - clip_rect.x ), (uint16_t)( clip_rect.w - clip_rect.y ) };
            crude_gfx_cmd_set_scissor( primary_cmd, &scissor_rect );
          }
          
          /* Retrieve */
          crude_gfx_texture_handle new_texture = *( crude_gfx_texture_handle* )( pcmd->TexRef.GetTexID() );
          if ( new_texture.index == CRUDE_GFX_FRAME_GRAPH_GAME_FINAL_TEXTURE_HANDLE )
          {
            new_texture = postprocessed_texture->handle;
          }

          push_constant = CRUDE_COMPOUNT_EMPTY( crude_gfx_imgui_pass_push_constant_ );
          push_constant.vertices = imgui_vertices_hga.gpu_address;
          push_constant.indices = imgui_indices_hga.gpu_address;
          push_constant.projection = ortho_projection;
          push_constant.index_offset = index_buffer_offset + pcmd->IdxOffset;
          push_constant.vertex_offset = vtx_buffer_offset + pcmd->VtxOffset;
          push_constant.texture_index = new_texture.index;
          crude_gfx_cmd_push_constant( primary_cmd, &push_constant, sizeof( push_constant ) );

          crude_gfx_cmd_draw( primary_cmd, 0, pcmd->ElemCount, 0, 1 );
        }
      }
    }
    index_buffer_offset += cmd_list->IdxBuffer.Size;
    vtx_buffer_offset += cmd_list->VtxBuffer.Size;
  }
}
