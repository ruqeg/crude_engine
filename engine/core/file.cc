#include <stdio.h>
#include <string.h>
#if defined( _WIN32 )
#include <windows.h>
#include <direct.h>
#else /* #if defined( _WIN32 ) */
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#if defined( FNM_CASEFOLD )
#define CRUDE_FNMATCH_FLAGS_ FNM_CASEFOLD
#else
#define CRUDE_FNMATCH_FLAGS_ 0
#endif
#endif /* #if defined( _WIN32 ) #else */

#include <engine/core/string.h>
#include <engine/core/assert.h>

#include <engine/core/file.h>

#if defined( _WIN32 )
static_assert( sizeof( crude_file_iterator::founded_data ) == sizeof( WIN32_FIND_DATA ) );
static_assert( sizeof( crude_file_iterator::handle ) == sizeof( HANDLE ) );
#else /* #if defined( _WIN32 ) */
typedef struct crude_file_iterator_linux_data_
{
  struct dirent                                           *entry;
  char                                                     pattern[ NAME_MAX + 1 ];
} crude_file_iterator_linux_data_;
static_assert( sizeof( crude_file_iterator::founded_data ) >= sizeof( crude_file_iterator_linux_data_ ) );
static_assert( sizeof( crude_file_iterator::handle ) >= sizeof( DIR* ) );
#endif /* #if defined( _WIN32 ) #else */

static long
crude_file_get_size_
(
  _In_ FILE                                               *f
)
{
  long                                                     file_size_signed;

  fseek( f, 0, SEEK_END );
  file_size_signed = ftell( f );
  fseek( f, 0, SEEK_SET );

  return file_size_signed;
}

#if !defined( _WIN32 )
static bool
crude_file_iterator_advance_
(
  _In_ crude_file_iterator                                *iterator
)
{
  crude_file_iterator_linux_data_                         *data;
  DIR                                                     *dir;
  struct dirent                                           *entry;

  data = CRUDE_CAST( crude_file_iterator_linux_data_*, iterator->founded_data );
  dir = CRUDE_CAST( DIR*, iterator->handle );

  while ( ( entry = readdir( dir ) ) != NULL )
  {
    if ( fnmatch( data->pattern, entry->d_name, CRUDE_FNMATCH_FLAGS_ ) == 0 )
    {
      data->entry = entry;
      return true;
    }
  }

  data->entry = NULL;
  return false;
}
#endif /* !defined( _WIN32 ) */

bool
crude_file_iterator_initialize
(
  _In_ crude_file_iterator                                *iterator,
  _In_ crude_string_view const                             search_filter
)
{
#if defined( _WIN32 )
  iterator->handle = CRUDE_CAST( crude_file_iterator_handle, FindFirstFile( search_filter.data, CRUDE_CAST( LPWIN32_FIND_DATAA, iterator->founded_data ) ), sizeof( iterator->handle ) );
  return iterator->handle != CRUDE_CAST( crude_file_iterator_handle, INVALID_HANDLE_VALUE );
#else /* defined( _WIN32 ) */
  crude_file_iterator_linux_data_                         *data;
  char                                                     directory[ PATH_MAX ];
  char const                                              *filter;
  char const                                              *separator;
  char const                                              *back_separator;
  char const                                              *pattern;
  DIR                                                     *dir;

  data = CRUDE_CAST( crude_file_iterator_linux_data_*, iterator->founded_data );
  crude_memory_set( data, 0, sizeof( *data ) );
  iterator->handle = CRUDE_CAST( crude_file_iterator_handle, 0 );

  filter = search_filter.data;
  separator = strrchr( filter, '/' );
  back_separator = strrchr( filter, '\\' );
  if ( back_separator && ( !separator || back_separator > separator ) )
  {
    separator = back_separator;
  }

  if ( separator )
  {
    sizet directory_length = separator - filter;
    if ( directory_length == 0 )
    {
      directory_length = 1; /* filesystem root */
    }
    if ( directory_length >= sizeof( directory ) )
    {
      CRUDE_LOG_ERROR( CRUDE_CHANNEL_FILEIO, "Search directory is too long: %s", filter );
      return false;
    }
    crude_memory_copy( directory, filter, directory_length );
    directory[ directory_length ] = 0;
    pattern = separator + 1;
  }
  else
  {
    crude_string_raw_copy( directory, ".", sizeof( directory ) );
    pattern = filter;
  }

  if ( *pattern == 0 )
  {
    pattern = "*";
  }
  if ( crude_string_raw_calculate_length( pattern ) >= sizeof( data->pattern ) )
  {
    CRUDE_LOG_ERROR( CRUDE_CHANNEL_FILEIO, "Search pattern is too long: %s", pattern );
    return false;
  }
  crude_string_raw_copy( data->pattern, pattern, sizeof( data->pattern ) );

  dir = opendir( directory );
  if ( !dir )
  {
    return false;
  }

  iterator->handle = CRUDE_CAST( crude_file_iterator_handle, dir );
  if ( !crude_file_iterator_advance_( iterator ) )
  {
    closedir( dir );
    iterator->handle = CRUDE_CAST( crude_file_iterator_handle, 0 );
    return false;
  }
  return true;
#endif /* defined( _WIN32 ) #else */
}

bool
crude_file_iterator_is_directory
(
  _In_ crude_file_iterator                                *iterator
)
{

#if defined( _WIN32 )
  return CRUDE_CAST( WIN32_FIND_DATA*, iterator->founded_data )->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
#else /* #if defined( _WIN32 ) */
  crude_file_iterator_linux_data_                         *data;
  struct stat                                              file_stat;

  data = CRUDE_CAST( crude_file_iterator_linux_data_*, iterator->founded_data );
  if ( !data->entry )
  {
    return false;
  }

#if defined( _DIRENT_HAVE_D_TYPE )
  if ( data->entry->d_type == DT_DIR )
  {
    return true;
  }
  if ( data->entry->d_type != DT_UNKNOWN && data->entry->d_type != DT_LNK )
  {
    return false;
  }
#endif /* #if defined( _DIRENT_HAVE_D_TYPE ) */

  return fstatat( dirfd( CRUDE_CAST( DIR*, iterator->handle ) ), data->entry->d_name, &file_stat, 0 ) == 0 && S_ISDIR( file_stat.st_mode );
#endif /* #if defined( _WIN32 ) #else */
}

crude_string_view
crude_file_iterator_name
(
  _In_ crude_file_iterator                                *iterator
)
{
  // !TODO
#if defined( _WIN32 )
  return crude_string_view_make( CRUDE_CAST( WIN32_FIND_DATA*, iterator->founded_data )->cFileName );
#else /* defined( _WIN32 ) */
  crude_file_iterator_linux_data_                         *data;

  data = CRUDE_CAST( crude_file_iterator_linux_data_*, iterator->founded_data );
  return crude_string_view_make( data->entry ? data->entry->d_name : "" );
#endif /* defined( _WIN32 ) #else */

}

bool
crude_file_iterator_next
(
  _In_ crude_file_iterator                                *iterator
)
{
#if defined( _WIN32 )
  return FindNextFile( CRUDE_CAST( HANDLE, iterator->handle ), CRUDE_CAST( LPWIN32_FIND_DATAA, iterator->founded_data ) );
#else /* defined( _WIN32 ) */
  if ( iterator->handle == CRUDE_CAST( crude_file_iterator_handle, 0 ) )
  {
    return false;
  }
  return crude_file_iterator_advance_( iterator );
#endif /* defined( _WIN32 ) #else */

}

void
crude_file_iterator_deinitialize
(
  _In_ crude_file_iterator                                *iterator
)
{
#if defined( _WIN32 )
  FindClose( CRUDE_CAST( HANDLE, iterator->handle ) );
#else /* #if defined( _WIN32 ) */
  if ( iterator->handle != CRUDE_CAST( crude_file_iterator_handle, 0 ) )
  {
    closedir( CRUDE_CAST( DIR*, iterator->handle ) );
    iterator->handle = CRUDE_CAST( crude_file_iterator_handle, 0 );
  }
#endif /* #if defined( _WIN32 ) #else */
}

void
crude_get_executable_directory
(
  _Out_ char                                              *buffer,
  _In_ uint32                                              buffer_size
)
{
#if defined( _WIN32 )
  GetModuleFileName( NULL, buffer, buffer_size );

  char *last_back_slash = strrchr( buffer, '\\');
  if ( last_back_slash )
  {
    buffer[ last_back_slash - buffer + 1 ] = 0u;
  }
#else /* #if defined( _WIN32 ) */
  char                                                    *last_slash;
  ssize_t                                                  length;

  length = readlink( "/proc/self/exe", buffer, buffer_size - 1 );
  if ( length <= 0 )
  {
    buffer[ 0 ] = 0;
    CRUDE_LOG_ERROR( CRUDE_CHANNEL_FILEIO, "Failed to get executable path!" );
    return;
  }
  buffer[ length ] = 0;

  last_slash = strrchr( buffer, '/' );
  if ( last_slash )
  {
    buffer[ last_slash - buffer + 1 ] = 0u;
  }
#endif /* #if defined( _WIN32 ) #else */
}

uint32
crude_get_working_directory_length
(
)
{
#if defined( _WIN32 )
  return GetCurrentDirectoryA( 0, NULL );
#else /* defined( _WIN32 ) */
  char                                                    *working_directory;
  uint32                                                   length;

  working_directory = getcwd( NULL, 0 );
  if ( !working_directory )
  {
    return 0;
  }

  length = CRUDE_CAST( uint32, strlen( working_directory ) + 1 );
  ( free )( working_directory );
  return length;
#endif /* defined( _WIN32 ) #else */
}

void
crude_get_working_directory
(
  _Out_ char                                              *buffer,
  _In_ uint32                                              buffer_size
)
{
  char* result = getcwd( buffer, buffer_size );
  CRUDE_ASSERTM( CRUDE_CHANNEL_FILEIO, result, "Failed to get current directory!" );
}

crude_string_view
crude_string_view_directory_from_filepath
(
  _In_ crude_string_view const                             filepath
)
{
  char const                                              *last_point;
  char const                                              *last_separator;

  last_point = strrchr( filepath.data, '.' );
  last_separator = strrchr( filepath.data, '/' );
  if ( last_separator != NULL && last_point > last_separator )
  {
    return crude_string_view_make_with_length( filepath.data, last_separator - filepath.data );
  }
  else
  {
    last_separator = strrchr( filepath.data, '\\' );
    if ( last_separator != NULL && last_point > last_separator )
    {
      return crude_string_view_make_with_length( filepath.data, last_separator - filepath.data );
    }
    else
    {
      CRUDE_LOG_ERROR( CRUDE_CHANNEL_FILEIO, "Malformed path %s", filepath.data );
    }
  }
  return filepath;
}

bool
crude_file_exist
(
  _In_ crude_string_view const                             absolute_filepath
)
{
#if defined( _WIN32 )
  WIN32_FILE_ATTRIBUTE_DATA unused;
  return GetFileAttributesExA( absolute_filepath.data, GetFileExInfoStandard, &unused );
#else /* #if defined( _WIN32 ) */
  int result = access( absolute_filepath.data, F_OK );
  return ( result == 0 );
#endif /* #if defined( _WIN32 ) #else */
}

bool
crude_read_file
(
  _In_ crude_string_view const                             absolute_filename,
  _In_ crude_allocator_container                           allocator_container,
  _Out_ uint8                                            **buffer,
  _Out_ uint32                                            *buffer_size
)
{
  FILE* file = fopen( absolute_filename.data, "r" );
  
  if ( !file )
  {
    CRUDE_LOG_ERROR( CRUDE_CHANNEL_FILEIO, "Cannot read file \"%s\"", absolute_filename.data );
    return false;
  }

  sizet filesize = crude_file_get_size_( file );
  *buffer = CRUDE_CAST( uint8*, CRUDE_ALLOCATE( allocator_container, filesize + 1 ) );
  *buffer_size = fread( *buffer, 1, filesize, file );
  (*buffer)[ *buffer_size ] = 0;
  fclose( file );
  return true;
}

bool
crude_read_file_binary
(
  _In_ crude_string_view const                             absolute_filename,
  _Out_opt_ uint8                                         *buffer,
  _Out_ uint32                                            *buffer_size
)
{
  FILE* file = fopen( absolute_filename.data, "rb" );
  
  if ( !file )
  {
    CRUDE_LOG_ERROR( CRUDE_CHANNEL_FILEIO, "Cannot read file \"%s\"", absolute_filename.data );
    return false;
  }

  sizet filesize = crude_file_get_size_( file );
  if ( buffer )
  {
    *buffer_size = fread( buffer, 1, filesize, file );
    buffer[ *buffer_size ] = 0;
    fclose( file );
    
    CRUDE_ASSERT( *buffer_size < filesize + 1 );
  }
  else
  {
    *buffer_size = filesize + 1;
  }
  return true;
}

void
crude_write_file
(
  _In_ crude_string_view const                             absolute_filename,
  _In_ crude_string_view const                             buffer
)
{
  FILE* file = fopen( absolute_filename.data, "w" );
  if ( !file )
  {
    CRUDE_LOG_ERROR( CRUDE_CHANNEL_FILEIO, "Cannot write file \"%s\"", absolute_filename.data ? absolute_filename.data : "" );
    return;
  }
  fwrite( buffer.data, buffer.length, 1, file );
  fclose( file );
}

bool
crude_file_delete
(
  _In_ char const                                         *path
)
{
#if defined( _WIN64 )
  int result = remove( path );
  return result != 0;
#else /* #if defined( _WIN64 ) */
  int result = remove( path );
  return ( result == 0 );
#endif /* #if defined( _WIN64 ) #else */
}