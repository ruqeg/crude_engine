#include <engine/core/cvar.h>
#include <engine/engine.h>
#include <editor/editor.h>

#include <editor/gui/log_viewer.h>

CRUDE_GUI_BASE_DEFINE_CVAR( log_viewer )

CRUDE_GUI_BASE_INITIALIZE_START( log_viewer )
  log_viewer->prev_log_buffer_length = 0;
CRUDE_GUI_BASE_INITIALIZE_END( log_viewer )

CRUDE_GUI_BASE_DEINITIALIZE_START( log_viewer )
CRUDE_GUI_BASE_DEINITIALIZE_END( log_viewer )

CRUDE_GUI_BASE_UPDATE_START( log_viewer )
CRUDE_GUI_BASE_UPDATE_END( log_viewer )

CRUDE_GUI_BASE_QUEUE_DRAW_START( log_viewer )
  ImGui::TextUnformatted( crude_log_buffer( ) );
  if ( log_viewer->prev_log_buffer_length != crude_log_buffer_length( ) )
  {
    log_viewer->prev_log_buffer_length = crude_log_buffer_length( );
    ImGui::SetScrollHereY( 1.0f );
  }
CRUDE_GUI_BASE_QUEUE_DRAW_END( log_viewer )
