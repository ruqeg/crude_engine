#include <engine/core/cvar.h>
#include <engine/engine.h>
#include <editor/editor.h>

#include <editor/gui/node_inspector.h>

CRUDE_GUI_BASE_DEFINE_CVAR( node_inspector )

CRUDE_GUI_BASE_INITIALIZE_START( node_inspector )
  node_inspector->components_serialization_manager = &engine->components_serialization_manager;
  node_inspector->node_manager = &engine->node_manager;
  node_inspector->current_node = 0;
  node_inspector->current_node_changed_name[ 0 ] = 0;
CRUDE_GUI_BASE_INITIALIZE_END( node_inspector )

CRUDE_GUI_BASE_DEINITIALIZE_START( node_inspector )
CRUDE_GUI_BASE_DEINITIALIZE_END( node_inspector )

CRUDE_GUI_BASE_UPDATE_START( node_inspector )
CRUDE_GUI_BASE_UPDATE_END( node_inspector )

CRUDE_GUI_BASE_QUEUE_DRAW_START( node_inspector )
  crude_node_external const                               *node_external;

  if ( !crude_entity_valid( world, editor->selected_node ) )
  {
    return;
  }

  if ( node_inspector->current_node != editor->selected_node )
  {
    crude_string_raw_copy( node_inspector->current_node_changed_name, crude_entity_get_name( world, editor->selected_node ), sizeof( node_inspector->current_node_changed_name ) );
    node_inspector->current_node = editor->selected_node;
  }

  CRUDE_IMGUI_START_OPTIONS;
  
  CRUDE_IMGUI_OPTION( "Name", {
    ImGui::InputText( "##Name", node_inspector->current_node_changed_name, sizeof( node_inspector->current_node_changed_name ) );
    ImGui::SameLine( );
    if ( ImGui::Button( "Apply" ) )
    {
      crude_entity_set_name( world, editor->selected_node, node_inspector->current_node_changed_name );
    }
  } );

  node_external = CRUDE_ENTITY_GET_IMMUTABLE_COMPONENT( world, editor->selected_node, crude_node_external );

  for ( uint32 i = 0; i < CRUDE_HASHMAP_CAPACITY( node_inspector->components_serialization_manager->component_id_to_imgui_funs ); ++i )
  {
    if ( crude_hashmap_backet_key_valid( node_inspector->components_serialization_manager->component_id_to_imgui_funs[ i ].key ) )
    {
      if ( node_external && node_external->type == CRUDE_NODE_EXTERNAL_TYPE_REFERENCE )
      {
        /* Display only external component, hide other since we will now safe this changes anyway */
        if ( node_inspector->components_serialization_manager->component_id_to_imgui_funs[ i ].key != ecs_id( crude_node_external ) )
        {
          continue;
        }
      }
      node_inspector->components_serialization_manager->component_id_to_imgui_funs[ i ].value( world, editor->selected_node, node_inspector->node_manager );
    }
  }
CRUDE_GUI_BASE_QUEUE_DRAW_END( node_inspector )
