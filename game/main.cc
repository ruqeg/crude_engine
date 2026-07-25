#include "engine/core/log.h"
#include "engine/engine.h"
#include <thirdparty/cr/cr.h>

#include <game/game.h>

CR_EXPORT int
cr_main
(
  _In_ struct cr_plugin                                   *ctx,
  _In_ enum cr_op                                          operation
)
{
  crude_cr_user_data                                      *user_data;

  user_data = CRUDE_CAST( crude_cr_user_data*, ctx->userdata );

  if ( ( user_data->flags & CRUDE_CR_USER_DATA_GAME_INITIALIZED_FLAG ) == 0u  )
  {
    crude_game_instance_intialize( );
    crude_game_initialize( crude_game_instance( ), user_data->engine );
    CRUDE_LOG_INFO( CRUDE_CHANNEL_GENERAL, "Game initialized" );
    user_data->flags |= CRUDE_CR_USER_DATA_GAME_INITIALIZED_FLAG;
    return 0;
  }

  switch ( operation )
  {
  case CR_LOAD:
  case CR_UNLOAD:
  {
    break;
  }
  case CR_STEP:
  {
    crude_game_update( crude_game_instance( ) );
    break;
  }
  case CR_CLOSE:
  {
    crude_game_deinitialize( crude_game_instance( ) );
    crude_game_instance_deintialize( );
    CRUDE_LOG_INFO( CRUDE_CHANNEL_GENERAL, "Game deinitialized" );
    break;
  }
  }
  assert( ctx );
  return 0;
}
