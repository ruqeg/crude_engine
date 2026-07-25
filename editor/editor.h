#pragma once

#include "editor/gui/base.h"
#if CRUDE_DEVELOP

#include <engine/graphics/scene_renderer.h>
#include <engine/graphics/gpu_profiler.h>
#include <engine/graphics/imgui.h>
#include <editor/gui/node_inspector.h>
#include <editor/gui/node_tree.h>
#include <editor/gui/viewport.h>
#include <editor/gui/log_viewer.h>
#include <editor/gui/content_browser.h>
#include <editor/gui/gpu_visual_profiler.h>
#include <editor/gui/texture_inspector.h>
#include <editor/gui/cvard_editor.h>
#include <editor/editor_camera_ecs.h>

typedef struct crude_engine crude_engine;

typedef enum crude_editor_workspace_type
{
  CRUDE_EDITOR_WORKSPACE_TYPE_PERFOMANCE,
  CRUDE_EDITOR_WORKSPACE_TYPE_LEVEL_EDITOR,
} crude_editor_workspace_type;

typedef struct crude_editor
{
  /* Context */
  crude_engine                                            *engine;

  /* Common */
  crude_entity                                             editor_camera_node;
  crude_editor_camera_system_context                       editor_camera_system_context;
  ImVec2                                                   viewport_position;
  ImVec2                                                   viewport_size;
  crude_editor_workspace_type                              workspace_type;
  crude_entity                                             selected_node;
  crude_gui_base_container                                *gui_containers;

  /* Gui */
  crude_gui_viewport                                       viewport;
  crude_gui_node_inspector                                 node_inspector;
  crude_gui_node_tree                                      node_tree;
  crude_gui_log_viewer                                     log_viewer;
  crude_gui_content_browser                                content_browser;
  crude_gui_gpu_visual_profiler                            gpu_visual_profiler;
  crude_gui_texture_inspector                              texture_inspector;
  crude_gui_cvar_editor                                    cvar_editor;

  /* Other */
  char                                                     editor_selected_entity_name[ 4096 ];
} crude_editor;

CRUDE_API void
crude_editor_initialize
(
  _In_ crude_editor                                       *editor,
  _In_ crude_engine                                       *engine
);

CRUDE_API void
crude_editor_deinitialize
(
  _In_ crude_editor                                       *editor
);

CRUDE_API void
crude_editor_update
(
  _In_ crude_editor                                       *editor
);

CRUDE_API void
crude_editor_start_game
(
  _In_ crude_editor                                       *editor
);

CRUDE_API void
crude_editor_stop_game
(
  _In_ crude_editor                                       *editor
);

CRUDE_API void
crude_editor_queue_draw
(
  _In_ void                                               *ctx
);

CRUDE_API void
crude_editor_instance_intialize
(
);

CRUDE_API void
crude_editor_instance_deintialize
(
);

CRUDE_API crude_editor*
crude_editor_instance
(
);

#endif
