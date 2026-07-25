#pragma once

#include <editor/gui/base.h>

CRUDE_GUI_BASE_DECLARE( content_browser, {
  crude_heap_allocator                                    *allocator;
  crude_environment const                                 *environment;
  char                                                     selected_relative_filepath[ 4096 ];
} );
