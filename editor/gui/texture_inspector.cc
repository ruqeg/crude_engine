#include <engine/core/cvar.h>
#include <engine/engine.h>
#include <editor/editor.h>

#include <editor/gui/texture_inspector.h>

CRUDE_GUI_BASE_DEFINE_CVAR( texture_inspector )

CRUDE_GUI_BASE_INITIALIZE_START( texture_inspector )
  texture_inspector->texture_handle.index = -1;
CRUDE_GUI_BASE_INITIALIZE_END( texture_inspector )

CRUDE_GUI_BASE_DEINITIALIZE_START( texture_inspector )
CRUDE_GUI_BASE_DEINITIALIZE_END( texture_inspector )

CRUDE_GUI_BASE_UPDATE_START( texture_inspector )
CRUDE_GUI_BASE_UPDATE_END( texture_inspector )

CRUDE_GUI_BASE_QUEUE_DRAW_START( texture_inspector )
  crude_gui_texture_inspector                             *inspector;
  char const                                              *preview_texture_name;
  uint32                                                   id;

  inspector = texture_inspector;
  
  if ( CRUDE_RESOURCE_HANDLE_IS_VALID( inspector->texture_handle ) )
  {
    ImGui::Image( CRUDE_CAST( ImTextureRef, &inspector->texture_handle.index ), ImGui::GetContentRegionAvail( ) );
  }

  ImGui::SetCursorPos( ImGui::GetWindowContentRegionMin( ) );
  
  preview_texture_name = "Unknown";
  id = 0;

  if ( CRUDE_RESOURCE_HANDLE_IS_VALID( inspector->texture_handle ) )
  {
    crude_gfx_texture *selected_texture = crude_gfx_access_texture( &engine->gpu, inspector->texture_handle );
    if ( selected_texture && selected_texture->name[ 0 ] )
    {
      preview_texture_name = selected_texture->name;
    };
  }
  
  if ( ImGui::BeginCombo( "Texture ID", preview_texture_name ) )
  {
    for ( uint32 t = 0; t < engine->gpu.textures.pool_size; ++t )
    {
      crude_gfx_texture                                   *texture;
      crude_gfx_texture_handle                             texture_handle;
      bool                                                 is_selected;
  
      texture_handle = CRUDE_CAST( crude_gfx_texture_handle, t );
      if ( CRUDE_RESOURCE_HANDLE_IS_INVALID( texture_handle ) )
      {
        continue;
      }
      
      texture = crude_gfx_access_texture( &engine->gpu, texture_handle );
      if ( !texture || !texture->name )
      {
        continue;
      }
      
      ImGui::PushID( id++ );
  
      is_selected = ( inspector->texture_handle.index == texture_handle.index );
      if ( ImGui::Selectable( texture->name ) )
      {
        inspector->texture_handle = texture_handle;
      }
      
      ImGui::PopID( );
      if ( is_selected )
      {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
CRUDE_GUI_BASE_QUEUE_DRAW_END( texture_inspector )
