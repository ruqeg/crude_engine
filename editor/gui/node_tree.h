#pragma once

#include <editor/gui/base.h>

typedef enum crude_gui_node_type
{
  CRUDE_GUI_NODE_TYPE_EMPTY_3D = 0,
  CRUDE_GUI_NODE_TYPE_GLTF = 1,
  CRUDE_GUI_NODE_TYPE_RAY,
  CRUDE_GUI_NODE_TYPE_LIGHT,
  CRUDE_GUI_NODE_TYPE_CAMERA,
  CRUDE_GUI_NODE_TYPE_PHYSICS_BODY,
  CRUDE_GUI_NODE_TYPE_NODE_EXTERNAL,
  CRUDE_GUI_NODE_TYPE_AUDO_RELATIVE,
  CRUDE_GUI_NODE_TYPE_AUDIO_LISTENER,
  CRUDE_GUI_NODE_TYPE_DDGI_AREA,
  CRUDE_GUI_NODE_TYPE_WORLD_ENVIRONMENT,
  CRUDE_GUI_NODE_TYPE_TERRAIN,
  CRUDE_GUI_NODE_TYPE_ANIMATION_PLAYER,
  CRUDE_GUI_NODE_TYPE_COUNT,
} crude_gui_node_type;

CRUDE_GUI_BASE_DECLARE( node_tree, {
  crude_node_manager                                      *node_manager;
  crude_entity                                             node_reference;
  char                                                     new_node_name[ 512 ];
  int32                                                    new_node_type_index;
  bool                                                     node_popup_should_be_opened;
} );
