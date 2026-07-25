#include <thirdparty/cr/cr.h>

#include <engine/engine.h>
#include <editor/editor.h>

CR_EXPORT int
cr_main
(
  _In_ struct cr_plugin                                   *ctx,
  _In_ enum cr_op                                          operation
)
{
  crude_cr_user_data                                      *user_data;

  user_data = CRUDE_CAST( crude_cr_user_data*, ctx->userdata );

  if ( ( user_data->flags & CRUDE_CR_USER_DATA_EDITOR_INITIALIZED_FLAG ) == 0u  )
  {
    crude_editor_instance_intialize( );
    crude_editor_initialize( crude_editor_instance( ), user_data->engine );
    CRUDE_LOG_INFO( CRUDE_CHANNEL_GENERAL, "Editor initialized" );
    user_data->flags |= CRUDE_CR_USER_DATA_EDITOR_INITIALIZED_FLAG;
    return 0;
  }

  switch ( operation )
  {
  case CR_LOAD:
  {
    user_data->engine->editor_gui_queue_draws_container = crude_engine_gui_queue_draw_function_container_pack( crude_editor_queue_draw, crude_editor_instance( ) );
    break;
  }
  case CR_UNLOAD:
  {
    break;
  }
  case CR_STEP:
  {
    crude_editor_update( crude_editor_instance( ) );
    break;
  }
  case CR_CLOSE:
  {
    crude_editor_deinitialize( crude_editor_instance( ) );
    crude_editor_instance_deintialize( );
    CRUDE_LOG_INFO( CRUDE_CHANNEL_GENERAL, "Editor deinitialized" );
    break;
  }
  }
  assert( ctx );
  return 0;
}
