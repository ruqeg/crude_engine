#define CR_HOST
#include <thirdparty/cr/cr.h>

#define CGLTF_IMPLEMENTATION
#include <thirdparty/cgltf/cgltf.h>

#define STB_SPRINTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <thirdparty/stb/stb_sprintf.h>
#include <thirdparty/stb/stb_image.h>

#define VMA_IMPLEMENTATION
#include <thirdparty/vma/include/vk_mem_alloc.h>

#include <engine/core/file.h>

#include <engine/engine.h>

#if CRUDE_GFX_GPU_AV_ENABLE
#include <sys/resource.h>
#include <cstdio>
#endif /* CRUDE_GFX_GPU_AV_ENABLE */

static void
load_cr_plugin_
(
  _Out_ cr_plugin                                          *cr_plugin,
  _In_ char const                                          *lib_name
);

int
main
(
)
{
  crude_engine                                            *engine;
  crude_cr_user_data                                       user_data;
  cr_plugin                                                framework_ctx;
  cr_plugin                                                editor_ctx;
  cr_plugin                                                game_ctx;
  crude_engine_creation                                    engine_creation;

  engine_creation = CRUDE_COMPOUNT_EMPTY( crude_engine_creation );
#if defined( _WIN32 )
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

  engine = CRUDE_CAST( crude_engine*, malloc( sizeof( crude_engine ) ) );
  crude_engine_initialize( engine, &engine_creation );

  load_cr_plugin_( &framework_ctx, "libcrude_framework.so" );
  load_cr_plugin_( &game_ctx, "libcrude_game.so" );
  load_cr_plugin_( &editor_ctx, "libcrude_editor.so" );

  user_data.flags = 0;
  user_data.engine = engine;

  framework_ctx.userdata = &user_data;
  game_ctx.userdata = &user_data;
  editor_ctx.userdata = &user_data;

  cr_plugin_update( framework_ctx );
  cr_plugin_update( game_ctx );
  cr_plugin_update( editor_ctx );

  while ( engine->running )
  {
    crude_engine_update( engine );
    cr_plugin_update( game_ctx );
    cr_plugin_update( framework_ctx );
    cr_plugin_update( editor_ctx );
  }

  cr_plugin_close( game_ctx );
  cr_plugin_close( editor_ctx );
  cr_plugin_close( framework_ctx );

  crude_engine_deinitialize( engine );
  free( engine );

  return 0;
}

void
load_cr_plugin_
(
  _Out_ cr_plugin                                          *cr_plugin,
  _In_ char const                                          *lib_name
)
{
  char                                                    *executable_absoule_filepath;
  char                                                    *executable_directory;
  uint32                                                   executable_directory_length;

  executable_directory_length = 0u;
  crude_get_executable_directory( NULL, &executable_directory_length );

  executable_directory = CRUDE_CAST( char*, malloc( executable_directory_length + 1 ) );
  executable_absoule_filepath = CRUDE_CAST( char*, malloc( executable_directory_length + 512 ) );

  crude_get_executable_directory( executable_directory, &executable_directory_length );
  snprintf( executable_absoule_filepath, executable_directory_length + 512, "%s%s", executable_directory, lib_name );

  CRUDE_ASSERT( cr_plugin_open( *cr_plugin, executable_absoule_filepath ) );

  free( executable_directory );
  free( executable_absoule_filepath );
}
