#pragma once

#include <engine/graphics/gpu_device.h>
#include <engine/core/memory.h>

/**********************************************************
 *
 *  Handles
 *
 **********************************************************/
typedef struct crude_gfx_frame_graph_texture_handle
{
  crude_gfx_resource_index                                 index;
} crude_gfx_frame_graph_texture_handle;

typedef struct crude_gfx_frame_graph_memory_handle
{
  crude_gfx_resource_index                                 index;
} crude_gfx_frame_graph_memory_handle;

typedef struct crude_gfx_frame_graph_node_handle
{
  crude_gfx_resource_index                                 index;
} crude_gfx_frame_graph_node_handle;

typedef struct crude_gfx_frame_graph_resource_handle
{
  crude_gfx_resource_index                                 index;
} crude_gfx_frame_graph_resource_handle;

#define CRUDE_GFX_FRAME_GRAPH_RESOURCE_HANDLE_INVALID ( CRUDE_COMPOUNT( crude_gfx_frame_graph_resource_handle, { CRUDE_RESOURCE_INDEX_INVALID } ) )
#define CRUDE_GFX_FRAME_GRAPH_NODE_HANDLE_INVALID ( CRUDE_COMPOUNT( crude_gfx_frame_graph_node_handle, { CRUDE_RESOURCE_INDEX_INVALID } ) )

/**********************************************************
 *
 *  Callbacks
 *
 **********************************************************/
typedef struct crude_gfx_frame_graph_node crude_gfx_frame_graph_node;

typedef void (*crude_gfx_frame_graph_render_pass_render)
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_gfx_cmd_buffer                               *primary_cmd
);
typedef void (*crude_gfx_frame_graph_render_pass_on_resize)( );
typedef void (*crude_gfx_frame_graph_render_pass_on_techniques_reloaded)( );

typedef struct crude_gfx_frame_graph_pass_container
{
  crude_gfx_frame_graph_render_pass_render                render;
  crude_gfx_frame_graph_render_pass_on_resize             on_resize;
  crude_gfx_frame_graph_render_pass_on_techniques_reloaded on_techniques_reloaded;
} crude_gfx_frame_graph_pass_container;

/**********************************************************
 *
 *  Resources
 *
 **********************************************************/
typedef enum crude_gfx_frame_graph_resource_type
{
  CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MEMORY,
  CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT,
  CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_TEXTURE,
  CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_REFERENCE,
  CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MARKER,
} crude_gfx_frame_graph_resource_type;

typedef enum crude_gfx_frame_graph_resource_creation_pin_type
{
  CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_INPUT,
  CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_OUTPUT,
} crude_gfx_frame_graph_resource_creation_pin_type;

typedef struct crude_gfx_frame_graph_memory_creation
{
  uint64                                                   size;
  crude_gfx_memory_type                                    type;
  crude_string_view                                        name;
  crude_gfx_rhi_buffer_usage_flags                         additional_flags;
} crude_gfx_frame_graph_memory_creation;

typedef struct crude_gfx_frame_graph_texture_creation
{
  void                                                    *initial_data;
  uint32                                                   width;
  uint32                                                   height;
  uint16                                                   depth;
  crude_gfx_texture_subresource                            subresource;
  uint8                                                    flags;
  crude_gfx_rhi_format                                     format;
  crude_gfx_texture_type                                   type;
  crude_string_view                                        name;
  crude_gfx_sampler_handle                                 sampler_handle;
} crude_gfx_frame_graph_texture_creations;

typedef struct crude_gfx_frame_graph_resource_synchronization
{
  crude_gfx_rhi_resource_state                             state;
  XMFLOAT4                                                 clear_values;
} crude_gfx_frame_graph_resource_synchronization;

typedef struct crude_gfx_frame_graph_resource_union
{
  union
  {
    crude_string_view                                      marker;
    crude_gfx_frame_graph_memory_handle                    memory;
    crude_gfx_frame_graph_texture_handle                   texture;
  };
} crude_gfx_frame_graph_resource_union;

typedef struct crude_gfx_frame_graph_resource_creation
{
  crude_gfx_frame_graph_resource_union                     resource_union;
  crude_gfx_frame_graph_resource_synchronization           synchronization;
  crude_gfx_frame_graph_resource_type                      type;
  crude_gfx_frame_graph_resource_creation_pin_type         pin;
} crude_gfx_frame_graph_resource_creation;

typedef struct crude_gfx_frame_graph_resource
{
  crude_gfx_frame_graph_resource_union                     resource_union;
  crude_gfx_frame_graph_resource_synchronization           synchronization;
  crude_gfx_frame_graph_resource_type                      type;
  crude_string_view                                        name;
  crude_gfx_frame_graph_node_handle                        producer;
  crude_gfx_frame_graph_resource_handle                    output_handle;
} crude_gfx_frame_graph_resource;

typedef struct crude_gfx_frame_graph_texture
{
  crude_gfx_texture*                                       texture;
  crude_gfx_frame_graph_texture_creation                   creation;
} crude_gfx_frame_graph_texture;

typedef struct crude_gfx_frame_graph_memory
{
  crude_gfx_memory_allocation                              allocation;
  crude_gfx_frame_graph_memory_creation                    creation;
} crude_gfx_frame_graph_memory;

typedef struct crude_gfx_frame_graph_resource_union_cache
{
  uint32                                                   frames_idle;
  bool                                                     is_texture;
  union
  {
    crude_gfx_memory_allocation                            memory;
    crude_gfx_texture_handle                               texture;
  } resource;
} crude_gfx_frame_graph_resource_union_cache;

typedef struct crude_gfx_frame_graph_texture_view_cache
{
  uint32                                                   frames_idle;
  crude_gfx_texture                                      **texture_mips_views;
} crude_gfx_frame_graph_texture_view_cache;

/**********************************************************
 *
 *  Nodes
 *
 **********************************************************/
typedef enum crude_gfx_frame_graph_node_type
{
  CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_UNKNOWN,
  CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS
} crude_gfx_frame_graph_node_type;

typedef struct crude_gfx_frame_graph_node_creation
{
  void                                                    *ctx;
  crude_gfx_frame_graph_resource_creation                  resources[ 64 ];
  uint32                                                   resources_count;
  crude_string_view                                        name;
  crude_gfx_frame_graph_render_pass_render                 render;
  crude_gfx_frame_graph_node_type                          type;
} crude_gfx_frame_graph_node_creation;

typedef struct crude_gfx_frame_graph crude_gfx_frame_graph;

typedef struct crude_gfx_frame_graph_node
{
  crude_gfx_frame_graph                                   *graph;
  void                                                    *ctx;
  crude_gfx_frame_graph_node_handle                       *edges;
  crude_gfx_frame_graph_resource_handle                   *inputs;
  crude_gfx_frame_graph_resource_handle                   *outputs;
  crude_string_view                                        name;
  crude_gfx_frame_graph_node_type                          type;
  crude_gfx_render_pass_handle                             render_pass;
  crude_gfx_framebuffer_handle                             framebuffer;
  XMFLOAT2                                                 resolution;
  crude_gfx_frame_graph_pass_container                     pass_container;
} crude_gfx_frame_graph_node;

/**********************************************************
 *
 *  Graph
 *
 **********************************************************/
typedef struct crude_gfx_frame_graph
{
  crude_gfx_device                                        *gpu;
  crude_heap_allocator                                    *allocator;
  crude_resource_pool                                      textures_resource_pool;
  crude_resource_pool                                      memories_resource_pool;
  crude_resource_pool                                      nodes_resource_pool;
  crude_resource_pool                                      resources_resource_pool;
  crude_gfx_frame_graph_node_handle                       *nodes;
  CRUDE_HASHMAPSTR( crude_gfx_frame_graph_resource_handle ) *resources_name_to_handle;
  CRUDE_HASHMAPSTR( crude_gfx_frame_graph_resource_union_cache ) *resources_union_name_to_cache;
  CRUDE_HASHMAPSTR( crude_gfx_frame_graph_texture_view_cache ) *texture_view_name_to_cache;
} crude_gfx_frame_graph;

/**********************************************************
 *
 *  Graph
 *
 **********************************************************/
CRUDE_API void
crude_gfx_frame_graph_initialize
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_device                                   *gpu
);

CRUDE_API void
crude_gfx_frame_graph_deinitialize
(
  _In_ crude_gfx_frame_graph                              *graph
);

CRUDE_API void
crude_gfx_frame_graph_build
(
  _In_ crude_gfx_frame_graph                              *graph
);

CRUDE_API void
crude_gfx_frame_graph_render
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_cmd_buffer                               *cmd
);

CRUDE_API void
crude_gfx_frame_graph_new_frame
(
  _In_ crude_gfx_frame_graph                              *graph
);

CRUDE_API void
crude_gfx_frame_graph_add_node
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_node_creation const          *creation
);

CRUDE_API crude_gfx_frame_graph_resource*
crude_gfx_frame_graph_access_resource
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_resource_handle               handle
);

CRUDE_API crude_gfx_frame_graph_resource*
crude_gfx_frame_graph_access_resource_by_name
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_string_view const                             name
);

CRUDE_API crude_gfx_frame_graph_resource_union_cache*
crude_gfx_frame_graph_access_resource_union_cache_by_name
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_string_view const                             name
);

CRUDE_API crude_gfx_frame_graph_texture_view_cache*
crude_gfx_frame_graph_access_texture_view_cache_by_name
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_string_view const                             name
);

CRUDE_API crude_gfx_frame_graph_memory*
crude_gfx_frame_graph_access_memory
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_memory_handle                 memory
);

CRUDE_API crude_gfx_frame_graph_texture*
crude_gfx_frame_graph_access_texture
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_texture_handle                handle
);

CRUDE_API crude_gfx_frame_graph_texture_handle
crude_gfx_frame_graph_create_texture
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_texture_creation const       *creation
);

CRUDE_API crude_gfx_frame_graph_memory_handle
crude_gfx_frame_graph_create_memory
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_memory_creation const        *creation
);

CRUDE_API crude_gfx_texture**
crude_gfx_frame_graph_create_texture_mips_views
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_texture                                  *texture,
  _In_ crude_string_view const                             name
);

/**********************************************************
 *
 *  Nodes
 *
 **********************************************************/
CRUDE_API crude_gfx_frame_graph_memory*
crude_gfx_frame_graph_node_find_memory
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_string_view const                             name
);

CRUDE_API crude_gfx_frame_graph_texture*
crude_gfx_frame_graph_node_find_texture
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_string_view const                             name
);


/**********************************************************
 *
 *  Creation
 *
 **********************************************************/
CRUDE_API crude_gfx_frame_graph_memory_creation
crude_gfx_frame_graph_memory_creation_empty
(
);

CRUDE_API crude_gfx_frame_graph_texture_creation
crude_gfx_frame_graph_texture_creation_empty
(
);

CRUDE_API crude_gfx_frame_graph_node_creation
crude_gfx_frame_graph_node_creation_empty
(
);

CRUDE_API void
crude_gfx_frame_graph_node_creation_set_name
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_string_view const                             name
);

CRUDE_API void
crude_gfx_frame_graph_node_creation_set_ctx
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ void                                               *ctx
);

CRUDE_API void
crude_gfx_frame_graph_node_creation_set_render
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_render_pass_render            render
);

CRUDE_API void
crude_gfx_frame_graph_node_creation_add_inout_image
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_texture_handle                texture,
  _In_ crude_gfx_rhi_resource_state                        state
);

CRUDE_API void
crude_gfx_frame_graph_node_creation_add_out_image
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_texture_handle                texture,
  _In_ crude_gfx_rhi_resource_state                        state,
  _In_ XMFLOAT4                                            clear_values
);

CRUDE_API void
crude_gfx_frame_graph_node_creation_add_in_image
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_texture_handle                texture,
  _In_ crude_gfx_rhi_resource_state                        state
);

CRUDE_API void
crude_gfx_frame_graph_node_creation_add_in_memory
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_memory_handle                 memory,
  _In_ crude_gfx_rhi_resource_state                        state
);

CRUDE_API void
crude_gfx_frame_graph_node_creation_add_out_memory
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_memory_handle                 memory,
  _In_ crude_gfx_rhi_resource_state                        state
);

CRUDE_API void
crude_gfx_frame_graph_node_creation_add_in_marker
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_string_view const                             name
);

CRUDE_API void
crude_gfx_frame_graph_node_creation_add_out_marker
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_string_view const                             name
);

/**********************************************************
 *
 *  Utils
 *
 **********************************************************/
CRUDE_API bool
crude_gfx_frame_graph_resource_type_is_image
(
  _In_ crude_gfx_frame_graph_resource_type                 type
);

#define CRUDE_GFX_FG_TEXTURE( _name )\
  crude_gfx_frame_graph_resource *graph_resource_##_name = crude_gfx_frame_graph_access_resource_by_name( node->graph, CRUDE_STRING_VIEW( #_name ) );\
  CRUDE_ASSERT( graph_resource_##_name && crude_gfx_frame_graph_resource_type_is_image( graph_resource_##_name->type ) );\
  crude_gfx_frame_graph_texture *graph_##_name = crude_gfx_frame_graph_access_texture( node->graph, graph_resource_##_name->resource_union.texture );\
  CRUDE_ASSERT( graph_##_name );\
  crude_gfx_texture *_name = graph_##_name->texture;

#define CRUDE_GFX_FG_TEXTURE_OPTIONAL( _name )\
  crude_gfx_frame_graph_resource *graph_resource_##_name = crude_gfx_frame_graph_access_resource_by_name( node->graph, CRUDE_STRING_VIEW( #_name ) );\
  crude_gfx_frame_graph_texture *graph_##_name = NULL;\
  crude_gfx_texture *_name = NULL;\
  if (graph_resource_##_name)\
  {\
    CRUDE_ASSERT( crude_gfx_frame_graph_resource_type_is_image( graph_resource_##_name->type ) );\
    graph_##_name = crude_gfx_frame_graph_access_texture( node->graph, graph_resource_##_name->resource_union.texture );\
    if ( graph_##_name )\
    {\
      _name = graph_##_name->texture;\
    }\
  }

#define CRUDE_GFX_FG_CREATE_RENDER_TARGET_2D( _name, _w, _h, _format, _sampler )\
  CRUDE_GFX_FG_CREATE_TEXTURE_2D( _name, _w, _h, _format, CRUDE_GFX_TEXTURE_MASK_RENDER_TARGET, 1u, _sampler );

#define CRUDE_GFX_FG_CREATE_TARGET_2D( _name, _w, _h, _format, _sampler )\
  CRUDE_GFX_FG_CREATE_TEXTURE_2D( _name, _w, _h, _format, CRUDE_GFX_TEXTURE_MASK_RENDER_TARGET | CRUDE_GFX_TEXTURE_MASK_COMPUTE, 1u, _sampler );

#define CRUDE_GFX_FG_CREATE_TARGET_2D_MIPS( _name, _w, _h, _format, _mips, _sampler )\
  CRUDE_GFX_FG_CREATE_TEXTURE_2D( _name, _w, _h, _format, CRUDE_GFX_TEXTURE_MASK_RENDER_TARGET | CRUDE_GFX_TEXTURE_MASK_COMPUTE, _mips, _sampler );

#define CRUDE_GFX_FG_CREATE_TEXTURE_2D( _name, _w, _h, _format, _flags, _mips, _sampler )\
  graph_texture_creation = crude_gfx_frame_graph_texture_creation_empty( );\
  graph_texture_creation.width = _w;\
  graph_texture_creation.height = _h;\
  graph_texture_creation.depth = 1u;\
  graph_texture_creation.format = _format;\
  graph_texture_creation.type = CRUDE_GFX_TEXTURE_TYPE_TEXTURE_2D;\
  graph_texture_creation.flags = _flags;\
  graph_texture_creation.subresource.mip_level_count = _mips;\
  graph_texture_creation.name = CRUDE_STRING_VIEW( #_name );\
  graph_texture_creation.sampler_handle = _sampler;\
  crude_gfx_frame_graph_texture_handle _name = crude_gfx_frame_graph_create_texture( graph, &graph_texture_creation );

#define CRUDE_GFX_FG_CREATE_MEMORY_GPU( _name, _size )\
  graph_memory_creation = crude_gfx_frame_graph_memory_creation_empty( );\
  graph_memory_creation.name = CRUDE_STRING_VIEW( #_name );\
  graph_memory_creation.type = CRUDE_GFX_MEMORY_TYPE_GPU;\
  graph_memory_creation.size = _size;\
  crude_gfx_frame_graph_memory_handle _name = crude_gfx_frame_graph_create_memory( graph, &graph_memory_creation );

#define CRUDE_GFX_FG_CREATE_TEXTURE_MIPS_VIEWS( _name, _texture_name )\
  crude_gfx_texture                                      **_name;\
  _name = crude_gfx_frame_graph_create_texture_mips_views( graph, _texture_name, CRUDE_STRING_VIEW( #_name ) );

#define CRUDE_GFX_FGN_TEXTURE( name)\
  crude_gfx_frame_graph_texture *graph_##name = crude_gfx_frame_graph_node_find_texture( node, CRUDE_STRING_VIEW( #name ) );\
  CRUDE_ASSERT( graph_##name );\
  crude_gfx_texture *name = graph_##name->texture;

#define CRUDE_GFX_FGN_MEMORY( name)\
  crude_gfx_frame_graph_memory *graph_##name = crude_gfx_frame_graph_node_find_memory( node, CRUDE_STRING_VIEW( #name ) );\
  CRUDE_ASSERT( graph_##name );\
  crude_gfx_memory_allocation name = graph_##name->allocation;

#define CRUDE_GFX_FGN_CONTEXT( type )\
  type *ctx = CRUDE_CAST( type*, node->ctx );

#define CRUDE_GFX_FGN_UTILS()\
  crude_gfx_frame_graph *graph = node->graph;\
  crude_gfx_device *gpu = graph->gpu;

#define CRUDE_GFX_FGN_TEXTURE_OPTIONAL( name )\
  crude_gfx_frame_graph_texture *graph_##name = crude_gfx_frame_graph_node_find_texture( node, CRUDE_STRING_VIEW( #name ) );\
  crude_gfx_texture *name = graph_##name ? graph_##name->texture : NULL;

#define CRUDE_GFX_FGN_MEMORY_OPTIONAL( name )\
  crude_gfx_frame_graph_memory *graph_##name = crude_gfx_frame_graph_node_find_memory( node, CRUDE_STRING_VIEW( #name ) );\
  crude_gfx_memory_allocation name = graph_##name ? graph_##name->allocation : CRUDE_COMPOUNT_EMPTY( crude_gfx_memory_allocation );

#define CRUDE_GFX_FGNC_INIT( _name, _ctx )\
  graph_node_creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_node_creation );\
  crude_gfx_frame_graph_node_creation_set_name( &graph_node_creation, CRUDE_STRING_VIEW( _name ) );\
  crude_gfx_frame_graph_node_creation_set_ctx( &graph_node_creation, &_ctx );\
  crude_gfx_frame_graph_node_creation_add_out_marker( &graph_node_creation, CRUDE_STRING_VIEW( _name ) );

#define CRUDE_GFX_FGNC_TYPE( _type )\
  graph_node_creation.type = _type;

#define CRUDE_GFX_FGNC_QUEUE( _render_fn )\
  crude_gfx_frame_graph_node_creation_set_render( &graph_node_creation, _render_fn );

#define CRUDE_GFX_FGNC_OUT_MEMORY( _name, _stage )\
  crude_gfx_frame_graph_node_creation_add_out_memory( &graph_node_creation, _name, _stage );

#define CRUDE_GFX_FGNC_IN_MEMORY( _name, _stage )\
  crude_gfx_frame_graph_node_creation_add_in_memory( &graph_node_creation, _name, _stage );

#define CRUDE_GFX_FGNC_IN_IMAGE( _name, _stage )\
  crude_gfx_frame_graph_node_creation_add_in_image( &graph_node_creation, _name, _stage );

#define CRUDE_GFX_FGNC_OUT_IMAGE( _name, _stage, ... )\
  crude_gfx_frame_graph_node_creation_add_out_image( &graph_node_creation, _name, _stage, CRUDE_COMPOUNT( XMFLOAT4, ##__VA_ARGS__ ) );

#define CRUDE_GFX_FGNC_INOUT_IMAGE( _name, _stage )\
  crude_gfx_frame_graph_node_creation_add_inout_image( &graph_node_creation, _name, _stage );

#define CRUDE_GFX_FGNC_IN_MARKER( _name )\
  crude_gfx_frame_graph_node_creation_add_in_marker( &graph_node_creation, CRUDE_STRING_VIEW( #_name ) );

#define CRUDE_GFX_FGNC_OUT_MARKER( _name )\
  crude_gfx_frame_graph_node_creation_add_out_marker( &graph_node_creation, CRUDE_STRING_VIEW( #_name ) );

#define CRUDE_GFX_FGNC_SUBMIT\
  crude_gfx_frame_graph_add_node( graph, &graph_node_creation );
