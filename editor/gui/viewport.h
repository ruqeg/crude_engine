#pragma once

#include <editor/gui/base.h>

CRUDE_GUI_BASE_DECLARE( viewport, {
  uint8                                                    viewport_ratio_index;
  ImGuizmo::OPERATION                                      selected_gizmo_operation;
  bool                                                     should_draw_grid;
} );
