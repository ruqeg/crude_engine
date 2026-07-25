#pragma once

#include "engine/graphics/gpu_resources.h"
#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_internal.h>

#include <thirdparty/imgui/backends/imgui_impl_sdl3.h>

#if CRUDE_DEVELOP
#include <thirdparty/ImGuizmo/ImGuizmo.h>
#endif

#include <engine/core/math.h>
#include <engine/graphics/rhi/rhi.h>

#define CRUDE_IMGUI_START_OPTIONS uint32 __crude_total_width = CRUDE_MIN( ImGui::GetContentRegionAvail().x * 0.75, 250 );
#define CRUDE_IMGUI_OPTION( text, ... )\
{\
  ImGui::Text( text );\
  ImGui::SameLine( __crude_total_width );\
  ImGui::SetNextItemWidth( __crude_total_width ); \
  __VA_ARGS__\
  ImGui::Separator( );\
}

#define CRUDE_GFX_FRAME_GRAPH_GAME_FINAL_TEXTURE_HANDLE 0x0fffffff
