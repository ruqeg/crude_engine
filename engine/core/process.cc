#if defined( _WIN32 )
#include <Windows.h>
#else /* defined( _WIN32 ) */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <spawn.h>
#include <sys/wait.h>
#endif /* #if defined( _WIN32 ) #else */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <engine/core/string.h>
#include <engine/core/core_config.h>
#include <engine/core/log.h>

#include <engine/core/process.h>

char                s_process_log_buffer[ CRUDE_CORE_PROCESS_OUTPUT_BUFFER_SIZE + 1 ];
static char         k_process_output_buffer[ CRUDE_CORE_PROCESS_OUTPUT_BUFFER_SIZE + 1 ];

#if defined( _WIN32 )
void
crude_process_win32_get_error_
(
  _In_ char                                               *buffer,
  _In_ uint32                                              size
)
{
  DWORD errorCode = GetLastError();
  
  char *error_string;
  if ( !FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, errorCode, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (LPSTR)&error_string, 0, NULL ) )
  {
    return;
  }

  sprintf_s( buffer, size, "%s", error_string );
  LocalFree( error_string );
}
#endif /* #if defined( _WIN32 ) */

/* From the post in https://stackoverflow.com/questions/35969730/how-to-read-output-from-cmd-exe-using-createprocess-and-createpipe/55718264#55718264 */
bool
crude_process_execute
(
  _In_ char const                                         *working_directory,
  _In_ char const                                         *process_fullpath,
  _In_ char const                                         *arguments,
  _In_ char const                                         *search_error_string
)
{
#if defined( _WIN32 )
  PROCESS_INFORMATION                                      process_info;
  HANDLE                                                   handle_stdin_pipe_read, handle_stdin_pipe_write, handle_stdout_pipe_read, handle_std_pipe_write;
  BOOL                                                     ok;
  bool                                                     execution_success;

  {
    SECURITY_ATTRIBUTES                                    security_attributes;
    
    security_attributes = CRUDE_COMPOUNT( SECURITY_ATTRIBUTES, { sizeof( SECURITY_ATTRIBUTES ), NULL, TRUE } );
    ok = CreatePipe( &handle_stdin_pipe_read, &handle_stdin_pipe_write, &security_attributes, 0 );
    if ( ok == FALSE )
    {
      return false;
    }
    ok = CreatePipe( &handle_stdout_pipe_read, &handle_std_pipe_write, &security_attributes, 0 );
    if ( ok == FALSE )
    {
      return false;
    }
  }

  {
    STARTUPINFOA                                           startup_info;
    BOOL                                                   inherit_handles;

    startup_info = CRUDE_COMPOUNT_EMPTY( STARTUPINFOA );
    startup_info.cb = sizeof( startup_info );
    startup_info.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startup_info.hStdInput = handle_stdin_pipe_read;
    startup_info.hStdError = handle_std_pipe_write;
    startup_info.hStdOutput = handle_std_pipe_write;
    startup_info.wShowWindow = SW_SHOW;
    
    execution_success = false;
    process_info = CRUDE_COMPOUNT_EMPTY( PROCESS_INFORMATION );
    inherit_handles = TRUE;
    if ( CreateProcessA( process_fullpath, (char*)arguments, 0, 0, inherit_handles, 0, 0, working_directory, &startup_info, &process_info ) )
    {
      CloseHandle( process_info.hThread );
      CloseHandle( process_info.hProcess );
      
      execution_success = true;
    }
    else
    {
      crude_process_win32_get_error_( &s_process_log_buffer[ 0 ], sizeof( s_process_log_buffer ) );
      
      CRUDE_LOG_INFO( CRUDE_CHANNEL_GRAPHICS, "Execute process error.\n Exe: \"%s\" - Args: \"%s\" - Work_dir: \"%s\"", process_fullpath, arguments, working_directory );
      CRUDE_LOG_INFO( CRUDE_CHANNEL_GRAPHICS, "Message: %s\n", s_process_log_buffer );
    }
    CloseHandle( handle_stdin_pipe_read );
    CloseHandle( handle_std_pipe_write );
  }

  /* Output */
  DWORD bytes_read;
  ok = ReadFile( handle_stdout_pipe_read, k_process_output_buffer, CRUDE_CORE_PROCESS_OUTPUT_BUFFER_SIZE, &bytes_read, NULL );
  
  /* Consume all outputs.
   * Terminate current read and initialize the next. */
  CRUDE_LOG_INFO( CRUDE_CHANNEL_SYSTEM, "proc called: %s %s", process_fullpath, (char*)arguments );
  while ( ok == TRUE )
  {
    k_process_output_buffer[bytes_read] = 0;
    CRUDE_LOG_INFO( CRUDE_CHANNEL_GRAPHICS, "proc output: %s", k_process_output_buffer );
    
    ok = ReadFile( handle_stdout_pipe_read, k_process_output_buffer, CRUDE_CORE_PROCESS_OUTPUT_BUFFER_SIZE, &bytes_read, NULL );
  }
  
  if ( strlen(search_error_string) > 0 && strstr( k_process_output_buffer, search_error_string ) )
  {
    execution_success = false;
  }
  
  /* Close handles. */
  CloseHandle( handle_stdout_pipe_read );
  CloseHandle( handle_stdin_pipe_write );
  
  DWORD process_exit_code = 0;
  GetExitCodeProcess( process_info.hProcess, &process_exit_code );
  
  return execution_success;
#else /* #if defined( _WIN32 ) */
  FILE                                                    *pipe;
  char const                                              *rest;
  char                                                    *command;
  sizet                                                    bytes_read;
  int                                                      status;
  bool                                                     execution_success;

  rest = arguments ? strchr( arguments, ' ' ) : NULL;
  command = NULL;
  if ( asprintf( &command, "cd \"%s\" 2>&1 || exit 127; \"%s\"%s 2>&1", working_directory ? working_directory : ".", process_fullpath, rest ? rest : "" ) < 0 )
  {
    return false;
  }

  CRUDE_LOG_INFO( CRUDE_CHANNEL_SYSTEM, "proc called: %s", command );

  pipe = popen( command, "r" );
  free( command );
  if ( !pipe )
  {
    return false;
  }

  execution_success = true;
  while ( ( bytes_read = fread( k_process_output_buffer, 1, CRUDE_CORE_PROCESS_OUTPUT_BUFFER_SIZE, pipe ) ) > 0 )
  {
    k_process_output_buffer[ bytes_read ] = 0;
    CRUDE_LOG_INFO( CRUDE_CHANNEL_GRAPHICS, "proc output: %s", k_process_output_buffer );

    if ( search_error_string[ 0 ] && strstr( k_process_output_buffer, search_error_string ) )
    {
      execution_success = false;
    }
  }

  status = pclose( pipe );
  if ( status == -1 || WEXITSTATUS( status ) == 126 || WEXITSTATUS( status ) == 127 )
  {
    CRUDE_LOG_INFO( CRUDE_CHANNEL_GRAPHICS, "Execute process error.\n Exe: \"%s\" - Args: \"%s\" - Work_dir: \"%s\"", process_fullpath, arguments, working_directory );
    return false;
  }
  return execution_success;

#endif /* #if defined( _WIN32 ) #else */
}

void
crude_process_expand_environment_strings
(
  _In_ char const                                         *environment,
  _In_ char                                               *buffer,
  _In_ uint32                                              buffer_size
)
{
#if defined( _WIN32 )
  ExpandEnvironmentStringsA( environment, buffer, buffer_size );
#else /* #if defined( _WIN32 ) */
  char const                                              *source;
  uint32                                                   length;

  source = environment;
  length = 0;

  while ( *source && length + 1 < buffer_size )
  {
    char const *end = ( *source == '%' ) ? strchr( source + 1, '%' ) : NULL;
    char const *value = NULL;
    if ( end && end > source + 1 )
    {
      char *name = strndup( source + 1, end - source - 1 );
      value = name ? getenv( name ) : NULL;
      free( name );
    }

    if ( value )
    {
      length += snprintf( buffer + length, buffer_size - length, "%s", value );
      source = end + 1;
    }
    else
    {
      buffer[ length++ ] = *source++;
    }
  }
  if ( length >= buffer_size )
  {
    length = buffer_size - 1;
  }
  buffer[ length ] = 0;
#endif /* #if defined( _WIN32 ) #else */
}

bool
crude_shell_execute
(
  _In_ char const                                         *filepath
)
{
#if defined( _WIN32 )
  HINSTANCE result = ShellExecute( NULL, "open", CRUDE_CAST( const TCHAR*, filepath ), NULL, NULL, SW_SHOWNORMAL );

  switch ( CRUDE_CAST( intptr_t, result ) )
  {
  case SE_ERR_FNF:     return false;
  case SE_ERR_NOASSOC: return false;
  }

  if ( ( int32 )result <= 32 )
  {
    return false;
  }

  return true;
  char *command = NULL;
  if ( asprintf( &command, "xdg-open \"%s\" > /dev/null 2>&1", filepath ) < 0 )
  {
    return false;
  }
  int result = system( command );
  free( command );
  return result == 0;
#else /* #if defined( _WIN32 ) */
  char                                                    *command;
  int                                                      result;

  command = NULL;
  if ( asprintf( &command, "xdg-open \"%s\" > /dev/null 2>&1", filepath ) < 0 )
  {
    return false;
  }
  result = system( command );
  free( command );
  return result == 0;
#endif /* #if defined( _WIN32 ) #else */
}

void
crude_system
(
  _In_ char const                                         *command
)
{
  system( command );
}