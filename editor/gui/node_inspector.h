#pragma once

#include <editor/gui/base.h>

CRUDE_GUI_BASE_DECLARE( node_inspector, {
  crude_components_serialization_manager                  *components_serialization_manager;
  crude_node_manager                                      *node_manager;
  crude_entity                                             current_node;
  char                                                     current_node_changed_name[ CRUDE_NODE_RELATIVE_FILEPATH_LENGTH_MAX ];
} );
