#if CRUDE_DEVELOP

#include <engine/engine.h>

#include <engine/gui/debug.h>

void
crude_gui_debug_initialize
(
  _In_ crude_gui_debug                                    *debug,
  _In_ crude_engine                                       *engine
)
{
  debug->engine = engine;
}

void
crude_gui_debug_deinitialize
(
  _In_ crude_gui_debug                                    *debug
)
{
}

void
crude_gui_debug_update
(
  _In_ crude_gui_debug                                    *debug
)
{
}

void
crude_gui_debug_queue_draw
(
  _In_ crude_gui_debug                                    *debug
)
{
  crude_editor_camera                                     *editor_camera;

  ImGui::DragInt( "Framerate", &debug->engine->framerate );

  if ( ImGui::Checkbox( "Enable Simulation", &debug->engine->physics.simulation_enabled ) )
  {
    crude_physics_enable_simulation( &debug->engine->physics, debug->engine->world, debug->engine->physics.simulation_enabled );
  }

#if CRUDE_EDITOR
  editor_camera = CRUDE_ENTITY_GET_MUTABLE_COMPONENT( debug->engine->world, debug->engine->editor.editor_camera_node, crude_editor_camera );
  ImGui::DragFloat( "Editor Camera Speed", &editor_camera->walk_speed );
#endif /* CRUDE_EDITOR */

  ImGui::Checkbox( "DDGI Debug", &debug->engine->scene_renderer.ddgi_debug );
  ImGui::DragFloat( "DDGI Rotation Scaler", &debug->engine->scene_renderer.ddgi_rotation_scaler );

  ImGui::CheckboxFlags( "Force Roughness", &debug->engine->scene_renderer.options.debug.flags1, 1 << 0 );
  ImGui::CheckboxFlags( "Force Metalness", &debug->engine->scene_renderer.options.debug.flags1, 1 << 1 );
  ImGui::DragFloat( "Force Roughness Value", &debug->engine->scene_renderer.options.debug.force_roughness, 0.01f, 0.f, 1.f );
  ImGui::DragFloat( "Force Metalness Value", &debug->engine->scene_renderer.options.debug.force_metalness, 0.01f, 0.f, 1.f );
}

#endif /* CRUDE_DEVELOP */
