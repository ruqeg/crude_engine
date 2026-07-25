#pragma once

#include <engine/core/alias.h>
#include <engine/graphics/gpu_device.h>

typedef void (*crude_gfx_initialize_techniques_func)
(
  _In_ crude_gfx_device                               *gpu
);

CRUDE_API void
crude_gfx_initialize_geometry_meshlet_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_pointshadow_meshlet_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_depth_pyramid_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_culling_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_luminance_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_postprocessing_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_ssr_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_compose_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_debug_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_imgui_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_ddgi_techniques
(
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_initialize_ray_tracing_debug_techniques
(
  _In_ crude_gfx_device                                   *gpu
);