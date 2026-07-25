#include <thirdparty/cJSON/cJSON.h>

#include <engine/core/file.h>
#include <engine/core/log.h>

#include <engine/core/environment.h>

void
crude_environment_initialize
(
  _In_ crude_environment                                  *environment,
  _In_ crude_environment_creation const                   *creation,
  _In_ crude_heap_allocator                               *allocator
)
{
  char                                                    *working_absolute_directory;
  uint64                                                   resources_absolute_directory_length;
  uint64                                                   shaders_absolute_directory_length;
  uint64                                                   compiled_shaders_absolute_directory_length;
  uint64                                                   temporary_absolute_directory_length;
  uint64                                                   working_absolute_directory_length, constant_string_buffer_size;
  
  constant_string_buffer_size = 0u;


  working_absolute_directory_length = crude_get_working_directory_length( );
  working_absolute_directory = CRUDE_CAST( char*, malloc( working_absolute_directory_length + 1 ) );
  crude_get_working_directory( working_absolute_directory, working_absolute_directory_length + 1 );

  resources_absolute_directory_length = working_absolute_directory_length + creation->resources_relative_directory.length;
  shaders_absolute_directory_length = working_absolute_directory_length + creation->shaders_relative_directory.length;
  compiled_shaders_absolute_directory_length = working_absolute_directory_length + creation->compiled_shaders_relative_directory.length;
  temporary_absolute_directory_length = working_absolute_directory_length + creation->temporary_relative_directory.length;

  constant_string_buffer_size += resources_absolute_directory_length;
  constant_string_buffer_size += shaders_absolute_directory_length;
  constant_string_buffer_size += compiled_shaders_absolute_directory_length;
  constant_string_buffer_size += temporary_absolute_directory_length;
  constant_string_buffer_size += working_absolute_directory_length;

  crude_string_buffer_initialize( &environment->constant_string_buffer, constant_string_buffer_size, crude_heap_allocator_pack( allocator ) );
  environment->resources_absolute_directory = crude_string_buffer_append_use_f( &environment->constant_string_buffer,
    "%s%s", working_absolute_directory, creation->resources_relative_directory.data );
  environment->shaders_absolute_directory = crude_string_buffer_append_use_f( &environment->constant_string_buffer,
    "%s%s", working_absolute_directory, creation->shaders_relative_directory.data );
  environment->compiled_shaders_absolute_directory = crude_string_buffer_append_use_f( &environment->constant_string_buffer,
    "%s%s", working_absolute_directory, creation->compiled_shaders_relative_directory.data );
  environment->temporary_absolute_directory = crude_string_buffer_append_use_f( &environment->constant_string_buffer,
    "%s", working_absolute_directory, creation->temporary_relative_directory.data );
  
  environment->window_title = creation->window_title;
  environment->window_width = creation->window_width;
  environment->window_height = creation->window_height;

  free( working_absolute_directory );
}

void
crude_environment_deinitialize
(
  _In_ crude_environment                                  *environment
)
{
  crude_string_buffer_deinitialize( &environment->constant_string_buffer );
}
