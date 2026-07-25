#pragma once

#include <engine/graphics/scene_renderer.h>
#include <engine/graphics/imgui.h>

typedef void (*crude_gui_base_initialize_fun)
(
  _In_ void                                               *ctx
);
typedef void (*crude_gui_base_deinitialize_fun)
(
  _In_ void                                               *ctx
);
typedef void (*crude_gui_base_update_fun)
(
  _In_ void                                               *ctx
);
typedef void (*crude_gui_base_queue_draw_fun)
(
  _In_ void                                               *ctx
);

typedef struct crude_gui_base_container
{
  void                                                    *ctx;
  crude_gui_base_initialize_fun                            initialize_fn;
  crude_gui_base_deinitialize_fun                          deinitialize_fn;
  crude_gui_base_update_fun                                update_fn;
  crude_gui_base_queue_draw_fun                            queue_draw_fn;
} crude_gui_base_container;

#define CRUDE_GUI_BASE_PACK( _name )\ 
  CRUDE_COMPOUNT( crude_gui_base_container, { \
    .ctx = &editor->_name, \
    .initialize_fn = crude_gui_##_name##_initialize, \
    .deinitialize_fn = crude_gui_##_name##_deinitialize, \
    .update_fn = crude_gui_##_name##_update, \
    .queue_draw_fn = crude_gui_##_name##_queue_draw \
  } )

#define CRUDE_GUI_BASE_DECLARE( _name, ... )\ 
typedef struct crude_gui_##_name  __VA_ARGS__ crude_gui_##_name;\
\
CRUDE_API void \
crude_gui_##_name##_initialize \
( \
  _In_ void                                               *ctx \
); \
 \
CRUDE_API void \
crude_gui_##_name##_deinitialize \
( \
  _In_ void                                               *ctx \
); \
 \
CRUDE_API void \
crude_gui_##_name##_update \
( \
  _In_ void                                               *ctx \
); \
 \
CRUDE_API void \
crude_gui_##_name##_queue_draw \
( \
  _In_ void                                               *ctx \
);

#define CRUDE_GUI_BASE_DEFINE_CVAR( _name )\
CRUDE_CVAR_DEFINE_BOOL( \
  cvar_editor_gui_##_name##_enabled, \
  "editor.gui.##_name.enabled", \
  CRUDE_CVAR_ACCESS_READWRITE, \
  false );

#define CRUDE_GUI_BASE_COMMON_VARIABLES_( _name )\
  crude_editor *editor = crude_editor_instance( ); \
  crude_engine *engine = editor->engine;\
  crude_ecs *world = engine->world;\
  crude_gui_##_name *_name = CRUDE_CAST( crude_gui_##_name*, ctx ); 

#define CRUDE_GUI_BASE_INITIALIZE_START( _name )\
CRUDE_API void \
crude_gui_##_name##_initialize \
( \
  _In_ void                                               *ctx \
) \
{ \
  CRUDE_GUI_BASE_COMMON_VARIABLES_( _name )

#define CRUDE_GUI_BASE_INITIALIZE_END( _name )\
}

#define CRUDE_GUI_BASE_DEINITIALIZE_START( _name )\
CRUDE_API void \
crude_gui_##_name##_deinitialize \
( \
  _In_ void                                               *ctx \
) \
{ \
  CRUDE_GUI_BASE_COMMON_VARIABLES_( _name )

#define CRUDE_GUI_BASE_DEINITIALIZE_END( _name )\
}

#define CRUDE_GUI_BASE_UPDATE_START( _name )\
CRUDE_API void \
crude_gui_##_name##_update \
( \
  _In_ void                                               *ctx \
) \
{ \
  CRUDE_GUI_BASE_COMMON_VARIABLES_( _name ) \
  if ( !CRUDE_CVAR_GET_BOOL( cvar_editor_gui_##_name##_enabled ) ) \
  { \
    return; \
  }

#define CRUDE_GUI_BASE_UPDATE_END( _name )\
}

#define CRUDE_GUI_BASE_QUEUE_DRAW_START( _name )\
CRUDE_API void \
crude_gui_##_name##_queue_draw \
( \
  _In_ void                                               *ctx \
) \
{ \
  ImGuiWindowFlags                                         window_flags; \
  CRUDE_GUI_BASE_COMMON_VARIABLES_( _name ) \
  if ( !CRUDE_CVAR_GET_BOOL( cvar_editor_gui_##_name##_enabled ) ) \
  { \
    return; \
  } \
  ImGui::Begin( #_name, NULL, window_flags ); \
  window_flags = 0; \
  if ( CRUDE_ECS_GAME_STAGE_IS_ENABLED( editor->engine->world ) ) \
  { \
    window_flags = ImGuiWindowFlags_NoInputs; \
  }

#define CRUDE_GUI_BASE_QUEUE_DRAW_END( _name )\
  ImGui::End( );\
}
