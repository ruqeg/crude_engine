#define CGLTF_IMPLEMENTATION
#include <thirdparty/cgltf/cgltf.h>

#define STB_SPRINTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <thirdparty/stb/stb_sprintf.h>
#include <thirdparty/stb/stb_image.h>

#define VMA_IMPLEMENTATION
#include <thirdparty/vma/include/vk_mem_alloc.h>

#include <engine/core/file.h>

#include <game/game.h>

#if CRUDE_GFX_GPU_AV_ENABLE
#include <sys/resource.h>
#include <cstdio>
#endif /* CRUDE_GFX_GPU_AV_ENABLE */

int
main
(
)
{
  crude_engine                                             engine;
  crude_environment                                        environment;
  
  /* I'm too lazy, move this to better place when I will be in..... the mood for it? */
#if CRUDE_GFX_GPU_AV_ENABLE
  rlimit                                                   rl;

  rl = CRUDE_COMPOUNT_EMPTY( rlimit );
  if ( getrlimit( RLIMIT_NOFILE, &rl ) == 0 )
  {
    rl.rlim_cur = rl.rlim_max;
    setrlimit( RLIMIT_NOFILE, &rl );
    getrlimit( RLIMIT_NOFILE, &rl );
    printf( "fd limit: %llu\n", CRUDE_CAST( unsigned long long, rl.rlim_cur ) );
  }
#endif /* CRUDE_GFX_GPU_AV_ENABLE */
  
  /* Initialization */
  {
    char                                                  *working_absolute_directory;
    crude_engine_creation                                  engine_creation;
    uint32                                                 working_absolute_directory_length;

    working_absolute_directory_length = crude_get_working_directory_length( );
    working_absolute_directory = CRUDE_CAST( char*, malloc( working_absolute_directory_length + 1 ) );
    crude_get_working_directory( working_absolute_directory, working_absolute_directory_length + 1 );

    engine_creation = CRUDE_COMPOUNT_EMPTY( crude_engine_creation );
    engine_creation.environment_creation.working_absolute_directory = crude_string_view_make_with_length( working_absolute_directory, working_absolute_directory_length );
#if defined( _WIN32 )
    engine_creation.environment_creation.render_graph_relative_directory = CRUDE_STRING_VIEW( "/../../engine/graphics/render_graphs/" );
    engine_creation.environment_creation.resources_relative_directory = CRUDE_STRING_VIEW( "/../../resources/" );
    engine_creation.environment_creation.shaders_relative_directory = CRUDE_STRING_VIEW( "/../../engine/graphics/shaders/" );
    engine_creation.environment_creation.compiled_shaders_relative_directory = CRUDE_STRING_VIEW( "/../../compiled_shaders/" );
#else
    engine_creation.environment_creation.resources_relative_directory = CRUDE_STRING_VIEW( "/../../../resources/" );
    engine_creation.environment_creation.shaders_relative_directory = CRUDE_STRING_VIEW( "/../../../engine/graphics/shaders/" );
    engine_creation.environment_creation.compiled_shaders_relative_directory = CRUDE_STRING_VIEW( "/../../../compiled_shaders/" );
#endif
    engine_creation.environment_creation.temporary_relative_directory = CRUDE_STRING_VIEW( "" );
    engine_creation.environment_creation.window_title = CRUDE_STRING_VIEW( "CE2" );
    engine_creation.environment_creation.window_width = 800;
    engine_creation.environment_creation.window_height = 600;

    crude_engine_initialize( &engine, &engine_creation );

    crude_game_instance_intialize( );
    
    crude_game_initialize( crude_game_instance( ), &engine );

    free( working_absolute_directory );
  }

  while ( engine.running )
  {
    crude_engine_update( &engine );
    crude_game_update( crude_game_instance( ) );
  }
  
  /* Deinitialization */
  {
    crude_game_deinitialize( crude_game_instance( ) );
    crude_engine_deinitialize( &engine );
    crude_game_instance_deintialize( );
  }

  return 0;
}
