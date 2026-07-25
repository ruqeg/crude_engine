#include <engine/engine.h>
#include <editor/editor.h>
#include <engine/core/cvar.h>

#include <editor/gui/viewport.h>

CRUDE_GUI_BASE_DEFINE_CVAR( viewport );

static char const* crude_gui_viewport_ratios_names_[ ] =
{
  "16/9",
  "16/10",
  "4/3"
};

static float32 crude_gui_viewport_ratios_[ ] =
{
  16 / 9.f,
  16 / 10.f,
  4 / 3.f
};

CRUDE_GUI_BASE_INITIALIZE_START( viewport )
  viewport->viewport_ratio_index = 0u;
  viewport->selected_gizmo_operation = ImGuizmo::TRANSLATE;
  viewport->should_draw_grid = false;
CRUDE_GUI_BASE_INITIALIZE_END( viewport )

CRUDE_GUI_BASE_DEINITIALIZE_START( viewport )
CRUDE_GUI_BASE_DEINITIALIZE_END( viewport )

CRUDE_GUI_BASE_UPDATE_START( viewport )
CRUDE_GUI_BASE_UPDATE_END( viewport )

CRUDE_GUI_BASE_QUEUE_DRAW_START( viewport )
  //ImGui::PushStyleColor( ImGuiCol_WindowBg, CRUDE_COMPOUNT( ImVec4, { 0.25f * 30 / 255.f, 0.25f *23 / 255.f, 0.25f *12 / 255.f, 1.00f } ) );
  //ImGui::PopStyleColor( );
  static crude_gfx_texture_handle                          g_viewport_texture_handle = { CRUDE_GFX_FRAME_GRAPH_GAME_FINAL_TEXTURE_HANDLE };

  crude_entity                                             selected_node;
  crude_entity                                             camera_node;
  ImVec2                                                   start_cursor_pos, viewport_pos, viewport_size;
  float32                                                  viewport_ratio;

  editor->selected_node = selected_node;
  camera_node = engine->camera_node;

  /* Viewport Texture */
  {
    start_cursor_pos = ImGui::GetCursorPos();
    
    viewport_ratio = crude_gui_viewport_ratios_[ viewport->viewport_ratio_index ];
    
    viewport_size.x = ImGui::GetContentRegionAvail( ).x;
    viewport_size.y = viewport_size.x / viewport_ratio;

    viewport_pos.x = start_cursor_pos.x;
    viewport_pos.y = start_cursor_pos.y + ( ImGui::GetContentRegionAvail( ).y - viewport_size.y ) * 0.5f;

    ImGui::SetCursorPos( viewport_pos );
    ImGui::Image( CRUDE_CAST( ImTextureRef, &g_viewport_texture_handle ), viewport_size );
    
    ImGui::SetCursorPos( start_cursor_pos );
    if ( ImGui::BeginCombo( "Ratio", crude_gui_viewport_ratios_names_[ viewport->viewport_ratio_index ], ImGuiComboFlags_WidthFitPreview ) ) {
      for ( uint32 i = 0; i < CRUDE_COUNTOF( crude_gui_viewport_ratios_names_ ); i++ )
      {
        bool is_selected = ( viewport->viewport_ratio_index == i );
        if ( ImGui::Selectable( crude_gui_viewport_ratios_names_[ i ], is_selected ) )
        {
          viewport->viewport_ratio_index = i;
        }
        
        if ( is_selected )
        {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    ImGui::SameLine( );
    if ( ImGui::Button( "Start" ) )
    {
      if ( !CRUDE_ECS_GAME_STAGE_IS_ENABLED( world ) )
      {
        crude_editor_start_game( editor );
      }
    }
    
    ImGui::SameLine( );
    if ( ImGui::Button( "Stop" ) )
    {
      if ( CRUDE_ECS_GAME_STAGE_IS_ENABLED( world ) )
      {
        crude_editor_stop_game( editor );
      }
    }
  }
  
  /* Viewport Imguizmo */
  if ( crude_entity_valid( world, selected_node ) && crude_entity_valid( world, camera_node ) && CRUDE_ENTITY_HAS_COMPONENT( world, selected_node, crude_transform ) )
  {
    crude_camera                                            *camera;
    crude_transform                                         *camera_transform;
    crude_transform                                         *selected_node_transform;
    crude_transform                                         *selected_node_parent_transform;
    crude_entity                                             selected_node_parent;
    XMFLOAT4X4                                               camera_world_to_view, camera_view_to_clip, selected_node_to_parent, selected_parent_to_camera_view;
    
    selected_node_transform = CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, selected_node, crude_transform );
    camera = CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, camera_node, crude_camera  );
    camera_transform = CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, camera_node, crude_transform  );
    selected_node_parent = crude_entity_get_parent( world, selected_node );
    
    if ( engine->platform.input.mouse.right.current && ImGui::IsKeyPressed( ImGuiKey_Z ) )
    {
      viewport->selected_gizmo_operation = ImGuizmo::TRANSLATE;
    }
    
    if ( engine->platform.input.mouse.right.current && ImGui::IsKeyPressed( ImGuiKey_X ) )
    {
      viewport->selected_gizmo_operation = ImGuizmo::ROTATE;
    }
    if ( engine->platform.input.mouse.right.current && ImGui::IsKeyPressed( ImGuiKey_C ) )
    {
      viewport->selected_gizmo_operation = ImGuizmo::SCALE;
    }
    
    if ( ImGui::RadioButton( "Translate", viewport->selected_gizmo_operation == ImGuizmo::TRANSLATE ) )
    {
      viewport->selected_gizmo_operation = ImGuizmo::TRANSLATE;
    }
    
    ImGui::SameLine();
    
    if ( ImGui::RadioButton( "Rotate", viewport->selected_gizmo_operation == ImGuizmo::ROTATE ) )
    {
      viewport->selected_gizmo_operation = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if ( ImGui::RadioButton( "Scale", viewport->selected_gizmo_operation == ImGuizmo::SCALE ) )
    {
      viewport->selected_gizmo_operation = ImGuizmo::SCALE;
    }
    
    ImGui::SameLine();

    ImGui::Checkbox( "Grid", &viewport->should_draw_grid );
    
    ImGui::SetCursorPos( viewport_pos );
    ImGuizmo::SetDrawlist( );
    ImGuizmo::SetRect(ImGui::GetWindowPos().x + viewport_pos.x, ImGui::GetWindowPos().y + viewport_pos.y, viewport_size.x, viewport_size.y);
    
    if ( crude_entity_valid( world, selected_node_parent ) )
    {
      selected_node_parent_transform = CRUDE_ENTITY_GET_MUTABLE_COMPONENT( world, selected_node_parent, crude_transform  );
    }
    else
    {
      selected_node_parent_transform = NULL;
    }
    
    if ( selected_node_parent_transform )
    {
      XMStoreFloat4x4( &selected_parent_to_camera_view, DirectX::XMMatrixMultiply( crude_transform_node_to_world( world, selected_node_parent, selected_node_parent_transform ), XMMatrixInverse( NULL, crude_transform_node_to_world( world, camera_node, camera_transform ) ) ) );
    }
    else
    {
      XMStoreFloat4x4( &selected_parent_to_camera_view, XMMatrixIdentity( ) );
    }
    
    XMStoreFloat4x4( &camera_view_to_clip, crude_camera_view_to_clip( camera ) );
    XMStoreFloat4x4( &selected_node_to_parent, crude_transform_node_to_parent( selected_node_transform ) );
    
    ImGuizmo::SetID( 0 );
    if ( ImGuizmo::Manipulate( &selected_parent_to_camera_view._11, &camera_view_to_clip._11, viewport->selected_gizmo_operation, ImGuizmo::MODE::WORLD, &selected_node_to_parent._11, NULL, NULL ) )
    {
      crude_transform_decompose( selected_node_transform, XMLoadFloat4x4( &selected_node_to_parent ) );
    
      CRUDE_ENTITY_COMPONENT_MODIFIED( world, selected_node, crude_transform );
    }

    if ( viewport->should_draw_grid )
    {
      XMFLOAT4X4 identity_matrix;
      XMStoreFloat4x4( &camera_world_to_view, XMMatrixInverse( NULL, crude_transform_node_to_world( world, camera_node, camera_transform ) ) );
      XMStoreFloat4x4( &identity_matrix, XMMatrixIdentity( ) );
      
      ImGuizmo::DrawGrid( &camera_world_to_view._11, &camera_view_to_clip._11, &identity_matrix._11, 100.f );
    }
  }
  

  if ( ImGui::BeginDragDropTargetCustom( ImGui::GetCurrentWindow( )->ContentRegionRect, ImGui::GetCurrentWindow( )->ID ) )
  {
    ImGuiPayload const                                  *im_payload;
    crude_string_view                                    replace_relative_filepath;
  
    im_payload = ImGui::AcceptDragDropPayload( "crude_content_browser_file" );
    if ( im_payload )
    {
      replace_relative_filepath = crude_string_view_make( CRUDE_CAST( char const*, im_payload->Data ) );
      if ( crude_string_raw_substr( replace_relative_filepath.data, ".crude_node" ) )
      {
        crude_engine_commands_manager_push_load_node_command( &engine->commands_manager, replace_relative_filepath );
      }
      else if ( crude_string_raw_substr( replace_relative_filepath.data, ".gltf" ) )
      {
        crude_transform const                             *selected_node_transform;
        crude_gfx_model_renderer_resources                *model_renderer_resources;
        crude_gfx_model_renderer_resources_handle          model_renderer_resources_handle;
        crude_entity                                       new_node;
        crude_gltf                                         gltf;
        crude_transform                                    transform;
        bool                                               hitted;
        
        gltf = crude_gltf_empty( );          
        transform = crude_transform_empty( );
        
        new_node = crude_entity_create_empty_without_name( world );
        crude_entity_set_parent( world, new_node, crude_entity_valid( world, selected_node ) ? selected_node : engine->main_node );
        crude_entity_set_name( world, new_node, replace_relative_filepath.data );
        
        model_renderer_resources_handle = crude_gfx_model_renderer_resources_manager_get_gltf_model( &engine->model_renderer_resources_manager, replace_relative_filepath );
      
        crude_gfx_model_renderer_resources_instance_initialize(
          &gltf.model_renderer_resources_instance,
          &engine->model_renderer_resources_manager,
          model_renderer_resources_handle );
        
        CRUDE_ENTITY_SET_COMPONENT( world, new_node, crude_gltf, { gltf } );
        
        selected_node_transform = CRUDE_ENTITY_GET_IMMUTABLE_COMPONENT( world, crude_entity_valid( world, selected_node ) ? selected_node : engine->main_node, crude_transform );
      
        if ( selected_node_transform )
        {
          transform = *selected_node_transform;
        }
        CRUDE_ENTITY_SET_COMPONENT( world, new_node, crude_transform, { transform } );
      
        editor->selected_node = new_node;
      }
      else
      {
        CRUDE_ASSERT( false && "TODO implement uncommented" );
      }
    }
    ImGui::EndDragDropTarget();
  }
CRUDE_GUI_BASE_QUEUE_DRAW_END( viewport )
