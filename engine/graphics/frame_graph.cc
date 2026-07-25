#include "engine/core/alias.h"
#include "engine/core/array.h"
#include "engine/core/cvar.h"
#include "engine/core/hashmapstr.h"
#include "engine/core/log.h"
#include "engine/core/memory.h"
#include "engine/core/resource_pool.h"
#include "engine/graphics/gpu_device.h"
#include "engine/graphics/gpu_memory.h"
#include <engine/core/hashmap.h>
#include <engine/graphics/command_buffer.h>

#include <engine/graphics/frame_graph.h>

CRUDE_CVAR_DEFINE_INT32(
  cvar_frame_graph_frames_idle_resource_destroy,
  "r.frame_graph.frames_idle_resource_destroy",
  CRUDE_CVAR_ACCESS_READWRITE,
  3 );

static crude_gfx_frame_graph_node*
crude_gfx_frame_graph_access_node_
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_node_handle                   handle
);

static void
crude_gfx_frame_graph_build_compute_edges_
(
  _In_ crude_gfx_frame_graph                              *graph
);

static void
crude_gfx_frame_graph_build_topological_sorting_
(
  _In_ crude_gfx_frame_graph                              *graph
);

static void
crude_gfx_frame_graph_build_compute_resource_aliasing_
(
  _In_ crude_gfx_frame_graph                              *graph
);

static void
crude_gfx_frame_graph_build_create_render_passes_
(
  _In_ crude_gfx_frame_graph                              *graph
);

static void
crude_gfx_frame_graph_build_create_framebuffers_
(
  _In_ crude_gfx_frame_graph                              *graph
);

static void
crude_gfx_frame_graph_render_texture_barrier_
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_cmd_buffer                               *cmd,
  _In_ crude_gfx_frame_graph_texture_handle                handle,
  _In_ crude_gfx_frame_graph_resource_synchronization     *synchronization
);

static void
crude_gfx_frame_graph_render_attachment_barrier_
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_cmd_buffer                               *cmd,
  _In_ crude_gfx_frame_graph_resource_union const         *resource_union,
  _In_ crude_gfx_frame_graph_resource_synchronization     *synchronization,
  _In_ bool                                                should_clear,
  _In_opt_ uint32                                          clear_index
);

static void
crude_gfx_frame_graph_render_memory_barrier_
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_cmd_buffer                               *cmd,
  _In_ crude_gfx_frame_graph_memory_handle                 handle,
  _In_ crude_gfx_rhi_resource_state                        new_state
);

void
crude_gfx_frame_graph_initialize
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_device                                   *gpu
)
{
  graph->gpu = gpu;
  graph->allocator = gpu->allocator;

  crude_resource_pool_initialize( &graph->memories_resource_pool, crude_heap_allocator_pack( gpu->allocator ), 1024u, sizeof( crude_gfx_frame_graph_memory ) );
  crude_resource_pool_initialize( &graph->textures_resource_pool, crude_heap_allocator_pack( gpu->allocator ), 1024u, sizeof( crude_gfx_frame_graph_texture ) );
  crude_resource_pool_initialize( &graph->nodes_resource_pool, crude_heap_allocator_pack( gpu->allocator ), 1024u, sizeof( crude_gfx_frame_graph_node ) );
  crude_resource_pool_initialize( &graph->resources_resource_pool, crude_heap_allocator_pack( gpu->allocator ), 1024u, sizeof( crude_gfx_frame_graph_resource ) );

  CRUDE_HASHMAPSTR_INITIALIZE( graph->resources_name_to_handle, crude_heap_allocator_pack( gpu->allocator ) );
  CRUDE_HASHMAPSTR_INITIALIZE( graph->resources_union_name_to_cache, crude_heap_allocator_pack( gpu->allocator ) );
  CRUDE_HASHMAPSTR_INITIALIZE( graph->texture_view_name_to_cache, crude_heap_allocator_pack( gpu->allocator ) );
  CRUDE_ARRAY_INITIALIZE_WITH_LENGTH( graph->nodes, 0, crude_heap_allocator_pack( gpu->allocator ) );
}

void
crude_gfx_frame_graph_deinitialize
(
  _In_ crude_gfx_frame_graph                              *graph
)
{
  for ( uint32 node_index = 0; node_index < CRUDE_ARRAY_LENGTH( graph->nodes ); ++node_index )
  {
    crude_gfx_frame_graph_node                            *node;

    node = crude_gfx_frame_graph_access_node_( graph, graph->nodes[ node_index ] );

    CRUDE_ARRAY_DEINITIALIZE( node->edges );

    for ( uint32 input_index = 0; input_index < CRUDE_ARRAY_LENGTH( node->inputs ); ++input_index )
    {
      crude_resource_pool_release_resource( &graph->resources_resource_pool, node->inputs[ input_index ].index );
    }
    CRUDE_ARRAY_DEINITIALIZE( node->inputs );

    for ( uint32 output_index = 0; output_index < CRUDE_ARRAY_LENGTH( node->outputs ); ++output_index )
    {
      crude_resource_pool_release_resource( &graph->resources_resource_pool, node->outputs[ output_index ].index );
    }
    CRUDE_ARRAY_DEINITIALIZE( node->outputs );

    switch ( node->type )
    {
    case CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS:
    {
      crude_gfx_destroy_render_pass( graph->gpu, node->render_pass );
      crude_gfx_destroy_framebuffer( graph->gpu, node->framebuffer );
      break;
    }
    case CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_UNKNOWN:
    {
      break;
    }
    default:
    {
      CRUDE_ASSERT( false );
    }
    }

    crude_resource_pool_release_resource( &graph->nodes_resource_pool, graph->nodes[ node_index ].index );
  }
  CRUDE_ASSERT( graph->resources_resource_pool.used_indices == 0u );

  for ( uint32 i = 0; i < CRUDE_HASHMAPSTR_CAPACITY( graph->resources_union_name_to_cache ); ++i )
  {
    if ( crude_hashmapstr_backet_key_hash_valid( graph->resources_union_name_to_cache[ i ].key.key_hash ) )
    {
      crude_gfx_frame_graph_resource_union_cache const    *resource_union_cache;

      resource_union_cache = &graph->resources_union_name_to_cache[ i ].value;

      if ( resource_union_cache->is_texture )
      {
        crude_gfx_destroy_texture_instant( graph->gpu, resource_union_cache->resource.texture );
      }
      else
      {
        crude_gfx_memory_deallocate( graph->gpu, resource_union_cache->resource.memory );
      }
    }
  }

  for ( uint32 i = 0; i < CRUDE_HASHMAPSTR_CAPACITY( graph->texture_view_name_to_cache ); ++i )
  {
    if ( crude_hashmapstr_backet_key_hash_valid( graph->texture_view_name_to_cache[ i ].key.key_hash ) )
    {
      crude_gfx_frame_graph_texture_view_cache            *texture_view_cache;

      texture_view_cache = &graph->texture_view_name_to_cache[ i ].value;

      for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( texture_view_cache->texture_mips_views ); ++i )
      {
        crude_gfx_destroy_texture_instant( graph->gpu, texture_view_cache->texture_mips_views[ i ]->handle );
      }
      CRUDE_ARRAY_DEINITIALIZE( texture_view_cache->texture_mips_views );
    }
  }

  CRUDE_ARRAY_DEINITIALIZE( graph->nodes );
  CRUDE_HASHMAPSTR_DEINITIALIZE( graph->resources_name_to_handle );
  CRUDE_HASHMAPSTR_DEINITIALIZE( graph->resources_union_name_to_cache );
  CRUDE_HASHMAPSTR_DEINITIALIZE( graph->texture_view_name_to_cache );

  crude_resource_pool_free_all_resource( &graph->nodes_resource_pool );
  crude_resource_pool_free_all_resource( &graph->resources_resource_pool );
  crude_resource_pool_free_all_resource( &graph->textures_resource_pool );
  crude_resource_pool_free_all_resource( &graph->memories_resource_pool );

  crude_resource_pool_deinitialize( &graph->memories_resource_pool );
  crude_resource_pool_deinitialize( &graph->textures_resource_pool );
  crude_resource_pool_deinitialize( &graph->nodes_resource_pool );
  crude_resource_pool_deinitialize( &graph->resources_resource_pool );
}

void
crude_gfx_frame_graph_build
(
  _In_ crude_gfx_frame_graph                              *graph
)
{
#if 0
  CRUDE_LOG_INFO( CRUDE_CHANNEL_GRAPHICS, "Build frame graph" );
#endif

  for ( uint32 node_index = 0; node_index < CRUDE_ARRAY_LENGTH( graph->nodes ); ++node_index )
  {
    crude_gfx_frame_graph_node                            *node;

    node = crude_gfx_frame_graph_access_node_( graph, graph->nodes[ node_index ] );

    for ( uint32 output_index = 0; output_index < CRUDE_ARRAY_LENGTH( node->outputs ); ++output_index )
    {
      crude_gfx_frame_graph_resource                      *output_resource;

      output_resource = crude_gfx_frame_graph_access_resource( graph, node->outputs[ output_index ] );
      
      switch ( output_resource->type )
      {
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT:
      {
        crude_gfx_frame_graph_texture                     *graph_texture;
        crude_gfx_frame_graph_resource_union_cache        *graph_resource_union_cache;
        crude_gfx_texture_handle                           texture_handle;

        graph_texture = crude_gfx_frame_graph_access_texture( graph, output_resource->resource_union.texture );

        graph_resource_union_cache = crude_gfx_frame_graph_access_resource_union_cache_by_name( graph, graph_texture->creation.name );
        if ( graph_resource_union_cache )
        {
          graph_resource_union_cache->frames_idle = 0;
          texture_handle = graph_resource_union_cache->resource.texture;
        }
        else
        {
          crude_gfx_texture_creation                       texture_creation;
          crude_gfx_frame_graph_resource_union_cache       new_graph_resource_union_cache;

          texture_creation = crude_gfx_texture_creation_empty( );
          texture_creation.initial_data = graph_texture->creation.initial_data;
          texture_creation.width = graph_texture->creation.width;
          texture_creation.height = graph_texture->creation.height;
          texture_creation.depth = graph_texture->creation.depth;
          texture_creation.subresource = graph_texture->creation.subresource;
          texture_creation.flags = graph_texture->creation.flags;
          texture_creation.format = graph_texture->creation.format;
          texture_creation.type = graph_texture->creation.type;
          crude_string_raw_copy( texture_creation.name, graph_texture->creation.name.data, sizeof( texture_creation.name ) );

          texture_handle = crude_gfx_create_texture( graph->gpu, &texture_creation );

          new_graph_resource_union_cache = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_union_cache );
          new_graph_resource_union_cache.resource.texture = texture_handle;
          new_graph_resource_union_cache.is_texture = true;
          CRUDE_HASHMAPSTR_SET( graph->resources_union_name_to_cache, graph_texture->creation.name, new_graph_resource_union_cache );
        }

        graph_texture->texture = crude_gfx_access_texture( graph->gpu, texture_handle );
        crude_gfx_link_texture_sampler( graph->gpu, graph_texture->texture->handle, graph_texture->creation.sampler_handle );

        output_resource->output_handle = node->outputs[ output_index ];
        CRUDE_HASHMAPSTR_SET( graph->resources_name_to_handle, output_resource->name, node->outputs[ output_index ] );
        break;
      }
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MEMORY:
      {
        crude_gfx_frame_graph_resource_union_cache        *graph_resource_union_cache;
        crude_gfx_frame_graph_memory                      *graph_memory;

        graph_memory = crude_gfx_frame_graph_access_memory( graph, output_resource->resource_union.memory );

        graph_resource_union_cache = crude_gfx_frame_graph_access_resource_union_cache_by_name( graph, graph_memory->creation.name );
        if ( graph_resource_union_cache )
        {
          graph_resource_union_cache->frames_idle = 0;
          graph_memory->allocation = graph_resource_union_cache->resource.memory;
        }
        else
        {
          crude_gfx_frame_graph_resource_union_cache       new_graph_resource_union_cache;

          graph_memory->allocation = crude_gfx_memory_allocate_with_name( graph->gpu, graph_memory->creation.size, graph_memory->creation.type, graph_memory->creation.name, graph_memory->creation.additional_flags );

          new_graph_resource_union_cache = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_union_cache );
          new_graph_resource_union_cache.is_texture = false;
          new_graph_resource_union_cache.resource.memory = graph_memory->allocation;
          CRUDE_HASHMAPSTR_SET( graph->resources_union_name_to_cache, graph_memory->creation.name, new_graph_resource_union_cache );
        }

        output_resource->output_handle = node->outputs[ output_index ];
        CRUDE_HASHMAPSTR_SET( graph->resources_name_to_handle, output_resource->name, node->outputs[ output_index ] );
        break;
      }
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MARKER:
      {
        output_resource->output_handle = node->outputs[ output_index ];
        CRUDE_HASHMAPSTR_SET( graph->resources_name_to_handle, output_resource->resource_union.marker, node->outputs[ output_index ] );
        break;
      }
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_TEXTURE:
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_REFERENCE:
      {
        break;
      }
      default:
      {
        CRUDE_ASSERT( false );
      }
      }
    }
  }

  crude_gfx_frame_graph_build_compute_edges_( graph );
  crude_gfx_frame_graph_build_topological_sorting_( graph );
  crude_gfx_frame_graph_build_compute_resource_aliasing_( graph );
  crude_gfx_frame_graph_build_create_render_passes_( graph );
  crude_gfx_frame_graph_build_create_framebuffers_( graph );
}

void
crude_gfx_frame_graph_render
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_cmd_buffer                               *cmd
)
{
  for ( uint32 node_index = 0; node_index < CRUDE_ARRAY_LENGTH( graph->nodes ); ++node_index )
  {
    crude_gfx_frame_graph_node                            *node;

    node = crude_gfx_frame_graph_access_node_( graph, graph->nodes[ node_index ] );

    crude_gfx_cmd_push_marker( cmd, node->name );
#if 0
    CRUDE_LOG_INFO( CRUDE_CHANNEL_GENERAL, "crude_gfx_frame_graph_render node\"%s\"", node->name.data );
#endif

    for ( uint32 input_index = 0; input_index < CRUDE_ARRAY_LENGTH( node->inputs ); ++input_index )
    {
      crude_gfx_frame_graph_resource *input_resource = crude_gfx_frame_graph_access_resource( graph, node->inputs[ input_index ] );
      crude_gfx_frame_graph_resource *resource = crude_gfx_frame_graph_access_resource( graph, input_resource->output_handle );

      if ( !resource )
      {
        continue;
      }

      switch ( input_resource->type )
      {
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_TEXTURE:
      {
        crude_gfx_frame_graph_render_texture_barrier_( graph, cmd, resource->resource_union.texture, &input_resource->synchronization );
        break;
      }
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT:
      {
        crude_gfx_frame_graph_render_attachment_barrier_( graph, cmd, &resource->resource_union, &input_resource->synchronization, false, 0 );
        break;
      }
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MEMORY:
      {
        crude_gfx_frame_graph_render_memory_barrier_( graph, cmd, resource->resource_union.memory, input_resource->synchronization.state );
        break;
      }
      default:
      {
        break;
      }
      }
    }

    for ( uint32 output_index = 0; output_index < CRUDE_ARRAY_LENGTH( node->outputs ); ++output_index )
    {
      crude_gfx_frame_graph_resource *resource = crude_gfx_frame_graph_access_resource( graph, node->outputs[ output_index ] );

      switch ( resource->type )
      {
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT:
      {
        switch ( node->type )
        {
        case CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS:
        {
          crude_gfx_frame_graph_render_attachment_barrier_( graph, cmd, &resource->resource_union, &resource->synchronization, true, output_index );
          break;
        }
        default:
        {
          crude_gfx_frame_graph_texture *graph_texture = crude_gfx_frame_graph_access_texture( graph, resource->resource_union.texture );
          crude_gfx_cmd_add_image_barrier( cmd, graph_texture->texture->handle, resource->synchronization.state, 0, graph_texture->texture->subresource.mip_level_count, crude_gfx_rhi_format_has_depth_or_stencil( graph_texture->texture->format ) );
        }
        }
        break;
      }
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MEMORY:
      {
        crude_gfx_frame_graph_render_memory_barrier_( graph, cmd, resource->resource_union.memory, resource->synchronization.state );
        break;
      }
      default:
      {
        break;
      }
      }
    }

    switch ( node->type )
    {
    case CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS:
    {
      crude_gfx_rect2d_int                                 scissor;
      crude_gfx_viewport                                   viewport;

      scissor = CRUDE_COMPOUNT_EMPTY( crude_gfx_rect2d_int );
      scissor.x = 0;
      scissor.y = 0;
      scissor.width = node->resolution.x;
      scissor.height = node->resolution.y;
      crude_gfx_cmd_set_scissor( cmd, &scissor );

      viewport = CRUDE_COMPOUNT_EMPTY( crude_gfx_viewport );
      viewport.rect.x = 0;
      viewport.rect.y = 0;
      viewport.rect.width = node->resolution.x;
      viewport.rect.height = node->resolution.y;
      viewport.min_depth = 0.0f;
      viewport.max_depth = 1.0f;
      crude_gfx_cmd_set_viewport( cmd, &viewport );

      crude_gfx_cmd_bind_render_pass( cmd, node->render_pass, node->framebuffer );
      if ( node->pass_container.render )
      {
        node->pass_container.render( node, cmd );
      }
      crude_gfx_cmd_end_render_pass( cmd );
      break;
    }
    default:
    {
      if ( node->pass_container.render )
      {
        node->pass_container.render( node, cmd );
      }
      break;
    }
    }

    crude_gfx_cmd_pop_marker( cmd );
  }
}

void
crude_gfx_frame_graph_new_frame
(
  _In_ crude_gfx_frame_graph                              *graph
)
{
  CRUDE_HASHMAPSTR( crude_gfx_frame_graph_resource_union_cache ) *new_resources_union_name_to_cache;
  CRUDE_HASHMAPSTR( crude_gfx_frame_graph_texture_view_cache ) *new_texture_view_name_to_cache;
  
  for ( uint32 node_index = 0; node_index < CRUDE_ARRAY_LENGTH( graph->nodes ); ++node_index )
  {
    crude_gfx_frame_graph_node                            *node;

    node = crude_gfx_frame_graph_access_node_( graph, graph->nodes[ node_index ] );

    CRUDE_ARRAY_DEINITIALIZE( node->edges );

    for ( uint32 input_index = 0; input_index < CRUDE_ARRAY_LENGTH( node->inputs ); ++input_index )
    {
      crude_resource_pool_release_resource( &graph->resources_resource_pool, node->inputs[ input_index ].index );
    }
    CRUDE_ARRAY_DEINITIALIZE( node->inputs );

    for ( uint32 output_index = 0; output_index < CRUDE_ARRAY_LENGTH( node->outputs ); ++output_index )
    {
      crude_resource_pool_release_resource( &graph->resources_resource_pool, node->outputs[ output_index ].index );
    }
    CRUDE_ARRAY_DEINITIALIZE( node->outputs );

    switch ( node->type )
    {
    case CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS:
    {
      crude_gfx_destroy_render_pass( graph->gpu, node->render_pass );
      crude_gfx_destroy_framebuffer( graph->gpu, node->framebuffer );
      break;
    }
    case CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_UNKNOWN:
    {
      break;
    }
    default:
    {
      CRUDE_ASSERT( false );
    }
    }

    crude_resource_pool_release_resource( &graph->nodes_resource_pool, graph->nodes[ node_index ].index );
  }
  CRUDE_ASSERT( graph->resources_resource_pool.used_indices == 0u );
  CRUDE_ARRAY_SET_LENGTH( graph->nodes, 0 );

  crude_resource_pool_free_all_resource( &graph->nodes_resource_pool );
  crude_resource_pool_free_all_resource( &graph->resources_resource_pool );
  crude_resource_pool_free_all_resource( &graph->textures_resource_pool );
  crude_resource_pool_free_all_resource( &graph->memories_resource_pool );

  CRUDE_HASHMAPSTR_CLEAN( graph->resources_name_to_handle );

  CRUDE_HASHMAPSTR_INITIALIZE( new_resources_union_name_to_cache, crude_heap_allocator_pack( graph->allocator ) );

  for ( uint32 i = 0; i < CRUDE_HASHMAPSTR_CAPACITY( graph->resources_union_name_to_cache ); ++i )
  {
    if ( crude_hashmapstr_backet_key_hash_valid( graph->resources_union_name_to_cache[ i ].key.key_hash ) )
    {
      crude_gfx_frame_graph_resource_union_cache const    *resource_union_cache;

      resource_union_cache = &graph->resources_union_name_to_cache[ i ].value;

      if ( resource_union_cache->frames_idle > CRUDE_CVAR_GET_INT32( cvar_frame_graph_frames_idle_resource_destroy ) )
      {
        if ( resource_union_cache->is_texture )
        {
          crude_gfx_destroy_texture_instant( graph->gpu, resource_union_cache->resource.texture );
        }
        else
        {
          crude_gfx_memory_deallocate( graph->gpu, resource_union_cache->resource.memory );
        }
      }
      else
      {
        crude_gfx_frame_graph_resource_union_cache         new_resource_union_cache;

        new_resource_union_cache = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_union_cache );
        new_resource_union_cache.frames_idle = resource_union_cache->frames_idle + 1;
        new_resource_union_cache.resource = resource_union_cache->resource;
        new_resource_union_cache.is_texture = resource_union_cache->is_texture;
      
        CRUDE_HASHMAPSTR_SET( new_resources_union_name_to_cache, graph->resources_union_name_to_cache[ i ].key.key, new_resource_union_cache );
      }
    }
  }
  CRUDE_HASHMAPSTR_DEINITIALIZE( graph->resources_union_name_to_cache );

  graph->resources_union_name_to_cache = CRUDE_CAST( CRUDE_TYPE( graph->resources_union_name_to_cache ), new_resources_union_name_to_cache );

  CRUDE_HASHMAPSTR_INITIALIZE( new_texture_view_name_to_cache, crude_heap_allocator_pack( graph->allocator ) );

  for ( uint32 i = 0; i < CRUDE_HASHMAPSTR_CAPACITY( graph->texture_view_name_to_cache ); ++i )
  {
    if ( crude_hashmapstr_backet_key_hash_valid( graph->texture_view_name_to_cache[ i ].key.key_hash ) )
    {
      crude_gfx_frame_graph_texture_view_cache            *texture_view_cache;

      texture_view_cache = &graph->texture_view_name_to_cache[ i ].value;

      if ( texture_view_cache->frames_idle > CRUDE_CVAR_GET_INT32( cvar_frame_graph_frames_idle_resource_destroy ) )
      {
        for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( texture_view_cache->texture_mips_views ); ++i )
        {
          crude_gfx_destroy_texture_instant( graph->gpu, texture_view_cache->texture_mips_views[ i ]->handle );
        }
        CRUDE_ARRAY_DEINITIALIZE( texture_view_cache->texture_mips_views );
      }
      else
      {
        crude_gfx_frame_graph_texture_view_cache           new_texture_view_cache;

        new_texture_view_cache = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_texture_view_cache );
        new_texture_view_cache.texture_mips_views = texture_view_cache->texture_mips_views;
        new_texture_view_cache.frames_idle = texture_view_cache->frames_idle + 1;
      
        CRUDE_HASHMAPSTR_SET( new_texture_view_name_to_cache, graph->texture_view_name_to_cache[ i ].key.key, new_texture_view_cache );
      }
    }
  }
  CRUDE_HASHMAPSTR_DEINITIALIZE( graph->texture_view_name_to_cache );

  graph->texture_view_name_to_cache = CRUDE_CAST( CRUDE_TYPE( graph->texture_view_name_to_cache ), new_texture_view_name_to_cache );
}

void
crude_gfx_frame_graph_add_node
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_node_creation const          *creation
)
{
  crude_gfx_frame_graph_node                              *node;
  crude_gfx_frame_graph_node_handle                        node_handle;

  node_handle.index = crude_resource_pool_obtain_resource( &graph->nodes_resource_pool );

  node = crude_gfx_frame_graph_access_node_( graph, node_handle );

  *node = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_node );
  node->graph = graph;
  node->ctx = creation->ctx;
  node->name = creation->name;
  node->type = creation->type;
  node->pass_container.render = creation->render;

  CRUDE_ARRAY_INITIALIZE_WITH_LENGTH( node->edges, 0, crude_heap_allocator_pack( graph->allocator ) );

  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( node->inputs, 0, crude_heap_allocator_pack( graph->allocator ) );
  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( node->outputs, 0, crude_heap_allocator_pack( graph->allocator ) );

  for ( uint32 i = 0; i < creation->resources_count; ++i )
  {
    crude_gfx_frame_graph_resource                        *resource;
    crude_gfx_frame_graph_resource_creation const         *resource_creation;
    crude_gfx_frame_graph_resource_handle                  resource_handle;

    resource_handle.index = crude_resource_pool_obtain_resource( &graph->resources_resource_pool );

    resource_creation = &creation->resources[ i ];

    resource = crude_gfx_frame_graph_access_resource( graph, resource_handle );
    *resource = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource );
    resource->resource_union = resource_creation->resource_union;
    resource->synchronization = resource_creation->synchronization;
    resource->type = resource_creation->type;

    switch ( resource->type )
    {
    case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT:
    case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_TEXTURE:
    case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_REFERENCE:
    {
      crude_gfx_frame_graph_texture *texture = crude_gfx_frame_graph_access_texture( graph, resource->resource_union.texture );
      resource->name = texture->creation.name;
      break;
    }
    case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MEMORY:
    {
      crude_gfx_frame_graph_memory *memory = crude_gfx_frame_graph_access_memory( graph, resource->resource_union.memory );
      resource->name = memory->creation.name;
      break;
    }
    case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MARKER:
    {
      resource->name = resource->resource_union.marker;
      break;
    }
    default:
    {
      CRUDE_ASSERT( false );
    }
    }

    if ( resource_creation->pin == CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_INPUT )
    {
      CRUDE_ARRAY_PUSH( node->inputs, resource_handle );
    }
    else if ( resource_creation->pin == CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_OUTPUT )
    {
      switch ( resource->type )
      {
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT:
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MARKER:
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MEMORY:
      {
        resource->producer = node_handle;
        break;
      }
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_REFERENCE:
      case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_TEXTURE:
      {
        break;
      }
      default:
      {
        CRUDE_ASSERT( false );
      }
      }
      CRUDE_ARRAY_PUSH( node->outputs, resource_handle );
    }
    else
    {
      CRUDE_ASSERT( false );
    }
  }

  CRUDE_ARRAY_PUSH( graph->nodes, node_handle );
}

crude_gfx_frame_graph_resource*
crude_gfx_frame_graph_access_resource
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_resource_handle               handle
)
{
  return CRUDE_CAST( crude_gfx_frame_graph_resource*, crude_resource_pool_access_resource( &graph->resources_resource_pool, handle.index ) );
}

crude_gfx_frame_graph_texture*
crude_gfx_frame_graph_access_texture
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_texture_handle                handle
)
{
  return CRUDE_CAST( crude_gfx_frame_graph_texture*, crude_resource_pool_access_resource( &graph->textures_resource_pool, handle.index ) );
}

crude_gfx_frame_graph_memory*
crude_gfx_frame_graph_access_memory
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_memory_handle                 handle
)
{
  return CRUDE_CAST( crude_gfx_frame_graph_memory*, crude_resource_pool_access_resource( &graph->memories_resource_pool, handle.index ) );
}

crude_gfx_frame_graph_resource*
crude_gfx_frame_graph_access_resource_by_name
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_string_view const                             name
)
{
  int64 handle_index = CRUDE_HASHMAPSTR_GET_INDEX( graph->resources_name_to_handle, name );
  if ( handle_index < 0 )
  {
    return NULL;
  }

  return crude_gfx_frame_graph_access_resource( graph, graph->resources_name_to_handle[ handle_index ].value );
}

crude_gfx_frame_graph_resource_union_cache*
crude_gfx_frame_graph_access_resource_union_cache_by_name
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_string_view const                             name
)
{
  int64 handle_index = CRUDE_HASHMAPSTR_GET_INDEX( graph->resources_union_name_to_cache, name );
  if ( handle_index < 0 )
  {
    return NULL;
  }

  return &graph->resources_union_name_to_cache[ handle_index ].value;
}

crude_gfx_frame_graph_texture_view_cache*
crude_gfx_frame_graph_access_texture_view_cache_by_name
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_string_view const                             name
)
{
  int64 handle_index = CRUDE_HASHMAPSTR_GET_INDEX( graph->texture_view_name_to_cache, name );
  if ( handle_index < 0 )
  {
    return NULL;
  }

  return &graph->texture_view_name_to_cache[ handle_index ].value;
}

crude_gfx_frame_graph_memory*
crude_gfx_frame_graph_node_find_memory
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_string_view const                             name
)
{
  // !TODO as hashmap
  for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( node->inputs ); ++i )
  {
    crude_gfx_frame_graph_resource                        *graph_resource;

    graph_resource = crude_gfx_frame_graph_access_resource( node->graph, node->inputs[ i ] );
    if ( crude_string_view_cmp( graph_resource->name, name ) == 0 )
    {
      return crude_gfx_frame_graph_access_memory( node->graph, graph_resource->resource_union.memory );
    }
  }
  for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( node->outputs ); ++i )
  {
    crude_gfx_frame_graph_resource                        *graph_resource;

    graph_resource = crude_gfx_frame_graph_access_resource( node->graph, node->outputs[ i ] );
    if ( crude_string_view_cmp( graph_resource->name, name ) == 0 )
    {
      return crude_gfx_frame_graph_access_memory( node->graph, graph_resource->resource_union.memory );
    }
  }
  return NULL;
}

crude_gfx_frame_graph_texture*
crude_gfx_frame_graph_node_find_texture
(
  _In_ crude_gfx_frame_graph_node                         *node,
  _In_ crude_string_view const                             name
)
{
  // !TODO as hashmap
  for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( node->inputs ); ++i )
  {
    crude_gfx_frame_graph_resource                        *graph_resource;

    graph_resource = crude_gfx_frame_graph_access_resource( node->graph, node->inputs[ i ] );
    if ( crude_string_view_cmp( graph_resource->name, name ) == 0 )
    {
      return crude_gfx_frame_graph_access_texture( node->graph, graph_resource->resource_union.texture );
    }
  }
  for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( node->outputs ); ++i )
  {
    crude_gfx_frame_graph_resource                        *graph_resource;

    graph_resource = crude_gfx_frame_graph_access_resource( node->graph, node->outputs[ i ] );
    if ( crude_string_view_cmp( graph_resource->name, name ) == 0 )
    {
      return crude_gfx_frame_graph_access_texture( node->graph, graph_resource->resource_union.texture );
    }
  }
  return NULL;
}

crude_gfx_frame_graph_texture_handle
crude_gfx_frame_graph_create_texture
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_texture_creation const       *creation
)
{
  crude_gfx_frame_graph_texture                           *texture;
  crude_gfx_frame_graph_texture_handle                     texture_handle;

  texture_handle.index = crude_resource_pool_obtain_resource( &graph->textures_resource_pool );

  texture = crude_gfx_frame_graph_access_texture( graph, texture_handle );
  texture->creation = *creation;

  return texture_handle;
}

crude_gfx_frame_graph_memory_handle
crude_gfx_frame_graph_create_memory
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_memory_creation const        *creation
)
{
  crude_gfx_frame_graph_memory                            *memory;
  crude_gfx_frame_graph_memory_handle                      memory_handle;

  memory_handle.index = crude_resource_pool_obtain_resource( &graph->memories_resource_pool );

  memory = crude_gfx_frame_graph_access_memory( graph, memory_handle );
  memory->creation = *creation;

  return memory_handle;
}

crude_gfx_texture**
crude_gfx_frame_graph_create_texture_mips_views
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_texture                                  *texture,
  _In_ crude_string_view const                             name
)
{
  crude_gfx_texture                                      **texture_mips_views;
  crude_gfx_frame_graph_texture_view_cache                *texture_view_cache;
  crude_gfx_frame_graph_texture_view_cache                 new_texture_view_cache;

  if ( !texture )
  {
    return NULL;
  }

  texture_view_cache = crude_gfx_frame_graph_access_texture_view_cache_by_name( graph, name );

  if ( texture_view_cache )
  {
    texture_view_cache->frames_idle = 0u;
    return texture_view_cache->texture_mips_views;
  }

  CRUDE_ARRAY_INITIALIZE_WITH_LENGTH( texture_mips_views, texture->subresource.mip_level_count, crude_heap_allocator_pack( graph->allocator ) );
  for ( uint32 i = 0; i < CRUDE_ARRAY_LENGTH( texture_mips_views ); ++i )
  {
    crude_gfx_texture_view_creation                      view_creation;
    crude_gfx_texture_handle                             texture_handle;

    view_creation = crude_gfx_texture_view_creation_empty( );
    view_creation.parent_texture_handle = texture->handle;
    view_creation.view_type = CRUDE_GFX_RHI_IMAGE_VIEW_TYPE_2D;
    view_creation.subresource.array_base_layer = 0;
    view_creation.subresource.array_layer_count = 1;
    view_creation.subresource.mip_base_level = i;
    view_creation.subresource.mip_level_count = 1;
    view_creation.name = name;
    texture_handle = crude_gfx_create_texture_view( graph->gpu, &view_creation );

    texture_mips_views[ i ] = crude_gfx_access_texture( graph->gpu, texture_handle );
  }

  new_texture_view_cache = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_texture_view_cache );
  new_texture_view_cache.texture_mips_views = texture_mips_views;
  CRUDE_HASHMAPSTR_SET( graph->texture_view_name_to_cache, name, new_texture_view_cache );

  return texture_mips_views;
}

crude_gfx_frame_graph_memory_creation
crude_gfx_frame_graph_memory_creation_empty
(
)
{
  crude_gfx_frame_graph_memory_creation creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_memory_creation );
  return creation;
}

crude_gfx_frame_graph_texture_creation
crude_gfx_frame_graph_texture_creation_empty
(
)
{
  crude_gfx_texture_creation                               texture_creation;
  crude_gfx_frame_graph_texture_creation                   creation;

  texture_creation = crude_gfx_texture_creation_empty( );

  creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_texture_creation );
  creation.initial_data = texture_creation.initial_data;
  creation.width = texture_creation.width;
  creation.height = texture_creation.height;
  creation.depth = texture_creation.depth;
  creation.subresource = texture_creation.subresource;
  creation.flags = texture_creation.flags;
  creation.format = texture_creation.format;
  creation.type = texture_creation.type;
  return creation;
}


crude_gfx_frame_graph_node_creation
crude_gfx_frame_graph_node_creation_empty
(
)
{
  crude_gfx_frame_graph_node_creation creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_node_creation );
  return creation;
}

void
crude_gfx_frame_graph_node_creation_set_name
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_string_view const                             name
)
{
  creation->name = name;
}

void
crude_gfx_frame_graph_node_creation_set_ctx
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ void                                               *ctx
)
{
  creation->ctx = ctx;
}

void
crude_gfx_frame_graph_node_creation_set_render
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_render_pass_render            render
)
{
  creation->render = render;
}

void
crude_gfx_frame_graph_node_creation_add_inout_image
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_texture_handle                texture,
  _In_ crude_gfx_rhi_resource_state                        state
)
{
  crude_gfx_frame_graph_resource_creation                 *resource_creation;

  resource_creation = &creation->resources[ creation->resources_count++ ];
  *resource_creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_creation );
  resource_creation->type = CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT;
  resource_creation->resource_union.texture = texture;
  resource_creation->synchronization.state = state;
  resource_creation->pin = CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_INPUT;

  resource_creation = &creation->resources[ creation->resources_count++ ];
  *resource_creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_creation );
  resource_creation->type = CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_REFERENCE;
  resource_creation->resource_union.texture = texture;
  resource_creation->pin = CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_OUTPUT;

  CRUDE_ASSERT( creation->resources_count < CRUDE_COUNTOF( creation->resources ) );
}

void
crude_gfx_frame_graph_node_creation_add_out_image
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_texture_handle                texture,
  _In_ crude_gfx_rhi_resource_state                        state,
  _In_ XMFLOAT4                                            clear_values
)
{
  crude_gfx_frame_graph_resource_creation                 *resource_creation;

  resource_creation = &creation->resources[ creation->resources_count++ ];
  *resource_creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_creation );
  resource_creation->type = CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT;
  resource_creation->resource_union.texture = texture;
  resource_creation->synchronization.state = state;
  resource_creation->synchronization.clear_values = clear_values;
  resource_creation->pin = CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_OUTPUT;

  CRUDE_ASSERT( creation->resources_count < CRUDE_COUNTOF( creation->resources ) );
}

void
crude_gfx_frame_graph_node_creation_add_in_image
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_texture_handle                texture,
  _In_ crude_gfx_rhi_resource_state                        state
)
{
  crude_gfx_frame_graph_resource_creation                 *resource_creation;

  resource_creation = &creation->resources[ creation->resources_count++ ];
  *resource_creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_creation );
  resource_creation->type = CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_TEXTURE;
  resource_creation->resource_union.texture = texture;
  resource_creation->synchronization.state = state;
  resource_creation->pin = CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_INPUT;

  CRUDE_ASSERT( creation->resources_count < CRUDE_COUNTOF( creation->resources ) );
}

void
crude_gfx_frame_graph_node_creation_add_in_memory
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_memory_handle                 memory,
  _In_ crude_gfx_rhi_resource_state                        state
)
{
  crude_gfx_frame_graph_resource_creation                 *resource_creation;

  resource_creation = &creation->resources[ creation->resources_count++ ];
  *resource_creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_creation );
  resource_creation->type = CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MEMORY;
  resource_creation->resource_union.memory = memory;
  resource_creation->synchronization.state = state;
  resource_creation->pin = CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_INPUT;

  CRUDE_ASSERT( creation->resources_count < CRUDE_COUNTOF( creation->resources ) );
}

void
crude_gfx_frame_graph_node_creation_add_out_memory
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_gfx_frame_graph_memory_handle                 memory,
  _In_ crude_gfx_rhi_resource_state                        state
)
{
  crude_gfx_frame_graph_resource_creation                 *resource_creation;

  resource_creation = &creation->resources[ creation->resources_count++ ];
  *resource_creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_creation );
  resource_creation->type = CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MEMORY;
  resource_creation->resource_union.memory = memory;
  resource_creation->synchronization.state = state;
  resource_creation->pin = CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_OUTPUT;

  CRUDE_ASSERT( creation->resources_count < CRUDE_COUNTOF( creation->resources ) );
}

void
crude_gfx_frame_graph_node_creation_add_in_marker
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_string_view const                             name
)
{
  crude_gfx_frame_graph_resource_creation                 *resource_creation;

  resource_creation = &creation->resources[ creation->resources_count++ ];
  *resource_creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_creation );
  resource_creation->type = CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MARKER;
  resource_creation->resource_union.marker = name;
  resource_creation->pin = CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_INPUT;

  CRUDE_ASSERT( creation->resources_count < CRUDE_COUNTOF( creation->resources ) );
}

void
crude_gfx_frame_graph_node_creation_add_out_marker
(
  _Inout_ crude_gfx_frame_graph_node_creation             *creation,
  _In_ crude_string_view const                             name
)
{
  crude_gfx_frame_graph_resource_creation                 *resource_creation;

  resource_creation = &creation->resources[ creation->resources_count++ ];
  *resource_creation = CRUDE_COMPOUNT_EMPTY( crude_gfx_frame_graph_resource_creation );
  resource_creation->type = CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_MARKER;
  resource_creation->resource_union.marker = name;
  resource_creation->pin = CRUDE_GFX_FRAME_GRAPH_RESOURCE_CREATION_PIN_TYPE_OUTPUT;

  CRUDE_ASSERT( creation->resources_count < CRUDE_COUNTOF( creation->resources ) );
}

bool
crude_gfx_frame_graph_resource_type_is_image
(
  _In_ crude_gfx_frame_graph_resource_type                 type
)
{
  switch ( type )
  {
  case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT:
  case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_TEXTURE:
  case CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_REFERENCE:
  return true;
  }
  return false;
}

crude_gfx_frame_graph_node*
crude_gfx_frame_graph_access_node_
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_frame_graph_node_handle                   handle
)
{
  return CRUDE_CAST( crude_gfx_frame_graph_node*, crude_resource_pool_access_resource( &graph->nodes_resource_pool, handle.index ) );
}

void
crude_gfx_frame_graph_build_compute_edges_
(
  _In_ crude_gfx_frame_graph                              *graph
)
{
  for ( uint32 node_index = 0; node_index < CRUDE_ARRAY_LENGTH( graph->nodes ); ++node_index )
  {
    crude_gfx_frame_graph_node *node = crude_gfx_frame_graph_access_node_( graph, graph->nodes[ node_index ] );
    CRUDE_ARRAY_SET_LENGTH( node->edges, 0 );
  }

  for ( uint32 node_index = 0; node_index < CRUDE_ARRAY_LENGTH( graph->nodes ); ++node_index )
  {
    crude_gfx_frame_graph_node *node = crude_gfx_frame_graph_access_node_( graph, graph->nodes[ node_index ] );

    for ( uint32 input_index = 0; input_index < CRUDE_ARRAY_LENGTH( node->inputs ); ++input_index )
    {
      crude_gfx_frame_graph_resource                       *resource;
      crude_gfx_frame_graph_resource                       *output_resource;
      crude_gfx_frame_graph_node                           *parent_node;

      resource = crude_gfx_frame_graph_access_resource( graph, node->inputs[ input_index ] );
      output_resource = crude_gfx_frame_graph_access_resource_by_name( graph, resource->name );
      if ( output_resource == NULL )
      {
        CRUDE_LOG_ERROR( CRUDE_CHANNEL_GRAPHICS, "Requested resource is not produced by any node and is not external." );
        continue;
      }

      resource->producer = output_resource->producer;
      resource->resource_union = output_resource->resource_union;
      resource->output_handle = output_resource->output_handle;

      parent_node = crude_gfx_frame_graph_access_node_( graph, resource->producer );
      CRUDE_ARRAY_PUSH( parent_node->edges, graph->nodes[ node_index ] );
    }
  }
}

void
crude_gfx_frame_graph_build_topological_sorting_
(
  _In_ crude_gfx_frame_graph                              *graph
)
{
  crude_gfx_frame_graph_node_handle                     *sorted_nodes;
  crude_gfx_frame_graph_node_handle                     *stack;
  uint8                                                 *visited;

  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( sorted_nodes, CRUDE_ARRAY_LENGTH( graph->nodes ), crude_heap_allocator_pack( graph->allocator ) );
  CRUDE_ARRAY_INITIALIZE_WITH_LENGTH( visited, CRUDE_ARRAY_LENGTH( graph->nodes ), crude_heap_allocator_pack( graph->allocator ) );
  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( stack, CRUDE_ARRAY_LENGTH( graph->nodes ), crude_heap_allocator_pack( graph->allocator ) );

  memset( visited, 0, sizeof( uint8 ) * CRUDE_ARRAY_LENGTH( visited ) );

  for ( uint32 node_index = 0; node_index < CRUDE_ARRAY_LENGTH( graph->nodes ); ++node_index )
  {
    crude_gfx_frame_graph_node *node = crude_gfx_frame_graph_access_node_( graph, graph->nodes[ node_index ] );

    CRUDE_ARRAY_PUSH( stack, graph->nodes[ node_index ] );

    while ( CRUDE_ARRAY_LENGTH( stack ) > 0 )
    {
      crude_gfx_frame_graph_node                        *stack_node;
      crude_gfx_frame_graph_node_handle                  stack_node_handle;

      stack_node_handle = CRUDE_ARRAY_BACK( stack );
      if ( visited[ stack_node_handle.index ] == 2 )
      {
        CRUDE_ARRAY_POP( stack );
        continue;
      }

      if ( visited[ stack_node_handle.index ] == 1 )
      {
        visited[ stack_node_handle.index ] = 2;
        CRUDE_ARRAY_PUSH( sorted_nodes, stack_node_handle );
        CRUDE_ARRAY_POP( stack );
        continue;
      }

      visited[ stack_node_handle.index ] = 1;

      stack_node = crude_gfx_frame_graph_access_node_( graph, stack_node_handle );

      if ( CRUDE_ARRAY_LENGTH( stack_node->edges ) == 0 )
      {
        continue;
      }

      for ( uint32 edge_index = 0; edge_index < CRUDE_ARRAY_LENGTH( stack_node->edges ); ++edge_index )
      {
        crude_gfx_frame_graph_node_handle                child_handle;

        child_handle = stack_node->edges[ edge_index ];
        if ( !visited[ child_handle.index ] )
        {
          CRUDE_ARRAY_PUSH( stack, child_handle );
        }
      }
    }
  }

  CRUDE_ASSERT( CRUDE_ARRAY_LENGTH( sorted_nodes ) == CRUDE_ARRAY_LENGTH( graph->nodes ) );
  CRUDE_ARRAY_SET_LENGTH( graph->nodes, 0 );

  for ( int32 i = CRUDE_ARRAY_LENGTH( sorted_nodes ) - 1; i >= 0; --i )
  {
    CRUDE_ARRAY_PUSH( graph->nodes, sorted_nodes[ i ] );
  }

  CRUDE_ARRAY_DEINITIALIZE( sorted_nodes );
  CRUDE_ARRAY_DEINITIALIZE( visited );
  CRUDE_ARRAY_DEINITIALIZE( stack );
}

void
crude_gfx_frame_graph_build_compute_resource_aliasing_
(
  _In_ crude_gfx_frame_graph                              *graph
)
{
  // !TOOD
  // MAKE WITH RESURSION TO HANDLE
  //   NODE0 [ ALLOCATION BIG]                           | allocate big
  //    |
  //   NODE1 [ ALLOCATION SMALL ]                        | allocate small (don't use big alias since it will be used
  //    |                                                                  later at the same time as small being used)
  //   NODE2 [ ALLOCATION BIG, ALLOCATION SMALL ]        | use big alias :D

  for ( uint32 node_index = 0; node_index < CRUDE_ARRAY_LENGTH( graph->nodes ); ++node_index )
  {

  }
}

void
crude_gfx_frame_graph_build_create_render_passes_
(
  _In_ crude_gfx_frame_graph                              *graph
)
{
  for ( uint32 node_index = 0; node_index < CRUDE_ARRAY_LENGTH( graph->nodes ); ++node_index )
  {
    crude_gfx_frame_graph_node                            *node;
    crude_gfx_render_pass_creation                         render_pass_creation;

    node = crude_gfx_frame_graph_access_node_( graph, graph->nodes[ node_index ] );

    if ( node->type != CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS )
    {
      continue;
    }

    render_pass_creation = crude_gfx_render_pass_creation_empty();
    render_pass_creation.name = node->name;

    for ( uint32 output_index = 0; output_index < CRUDE_ARRAY_LENGTH( node->outputs ); ++output_index )
    {
      crude_gfx_frame_graph_resource                      *output_resource;
      crude_gfx_frame_graph_texture                       *graph_texture;

      output_resource = crude_gfx_frame_graph_access_resource( graph, node->outputs[ output_index ] );
      if ( output_resource->type != CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT )
      {
        continue;
      }

      graph_texture = crude_gfx_frame_graph_access_texture( graph, output_resource->resource_union.texture );
      if ( crude_gfx_rhi_format_has_depth_or_stencil( graph_texture->texture->format ) )
      {
        render_pass_creation.depth_stencil_format = graph_texture->texture->format;
        render_pass_creation.stencil_operation = CRUDE_GFX_RENDER_PASS_OPERATION_DONT_CARE;
        render_pass_creation.depth_operation = CRUDE_GFX_RENDER_PASS_OPERATION_CLEAR;
      }
      else
      {
        render_pass_creation.color_formats[ render_pass_creation.num_render_targets ] = graph_texture->texture->format;
        render_pass_creation.color_operations[ render_pass_creation.num_render_targets ] = CRUDE_GFX_RENDER_PASS_OPERATION_CLEAR;
        render_pass_creation.color_final_layouts[ render_pass_creation.num_render_targets ] = CRUDE_GFX_RHI_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ++render_pass_creation.num_render_targets;
      }
    }

    for ( uint32 input_index = 0; input_index < CRUDE_ARRAY_LENGTH( node->inputs ); ++input_index )
    {
      crude_gfx_frame_graph_resource                      *input_resource;
      crude_gfx_frame_graph_texture                       *graph_texture;

      input_resource = crude_gfx_frame_graph_access_resource( graph, node->inputs[ input_index ] );
      if ( input_resource->type != CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT )
      {
        continue;
      }

      graph_texture = crude_gfx_frame_graph_access_texture( graph, input_resource->resource_union.texture );
      if ( crude_gfx_rhi_format_has_depth_or_stencil( graph_texture->texture->format ) )
      {
        render_pass_creation.depth_stencil_format = graph_texture->texture->format;
        render_pass_creation.stencil_operation = CRUDE_GFX_RENDER_PASS_OPERATION_DONT_CARE;
        render_pass_creation.depth_operation = CRUDE_GFX_RENDER_PASS_OPERATION_LOAD;
      }
      else
      {
        render_pass_creation.color_formats[ render_pass_creation.num_render_targets ] = graph_texture->texture->format;
        render_pass_creation.color_operations[ render_pass_creation.num_render_targets ] = CRUDE_GFX_RENDER_PASS_OPERATION_LOAD;
        render_pass_creation.color_final_layouts[ render_pass_creation.num_render_targets ] = CRUDE_GFX_RHI_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ++render_pass_creation.num_render_targets;
      }
    }

    node->render_pass = crude_gfx_create_render_pass( graph->gpu, &render_pass_creation );
  }
}

void
crude_gfx_frame_graph_build_create_framebuffers_
(
  _In_ crude_gfx_frame_graph                              *graph
)
{
  for ( uint32 node_index = 0; node_index < CRUDE_ARRAY_LENGTH( graph->nodes ); ++node_index )
  {
    crude_gfx_frame_graph_node                            *node;
    crude_gfx_framebuffer_creation                         framebuffer_creation;
    XMFLOAT2                                               framebuffer_size;

    node = crude_gfx_frame_graph_access_node_( graph, graph->nodes[ node_index ] );

    if ( node->type != CRUDE_GFX_FRAME_GRAPH_NODE_TYPE_RENDER_PASS )
    {
      continue;
    }

    framebuffer_size = CRUDE_COMPOUNT_EMPTY( XMFLOAT2 );

    framebuffer_creation = crude_gfx_framebuffer_creation_empty();
    crude_string_raw_copy( framebuffer_creation.name, node->name.data, sizeof( framebuffer_creation.name ) );

    for ( uint32 output_index = 0; output_index < CRUDE_ARRAY_LENGTH( node->outputs ); ++output_index )
    {
      crude_gfx_frame_graph_resource                      *output_resource;
      crude_gfx_frame_graph_texture                       *graph_texture;

      output_resource = crude_gfx_frame_graph_access_resource( graph, node->outputs[ output_index ] );
      if ( output_resource->type != CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT )
      {
        continue;
      }

      graph_texture = crude_gfx_frame_graph_access_texture( graph, output_resource->resource_union.texture );

      CRUDE_ASSERT( ( framebuffer_size.x == 0 ) || ( framebuffer_size.x == graph_texture->texture->width ) );
      CRUDE_ASSERT( ( framebuffer_size.y == 0 ) || ( framebuffer_size.y == graph_texture->texture->height ) );

      framebuffer_size.x = graph_texture->texture->width;
      framebuffer_size.y = graph_texture->texture->height;

      if ( crude_gfx_rhi_format_has_depth_or_stencil( graph_texture->texture->format ) )
      {
        framebuffer_creation.depth_stencil_texture = graph_texture->texture->handle;
      }
      else
      {
        framebuffer_creation.output_textures[ framebuffer_creation.num_render_targets++ ] = graph_texture->texture->handle;
      }
    }

    for ( uint32 input_index = 0; input_index < CRUDE_ARRAY_LENGTH( node->inputs ); ++input_index )
    {
      crude_gfx_frame_graph_resource                      *input_resource;
      crude_gfx_frame_graph_resource                      *resource;
      crude_gfx_frame_graph_texture                       *graph_texture;

      input_resource = crude_gfx_frame_graph_access_resource( graph, node->inputs[ input_index ] );

      if ( input_resource->type != CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_ATTACHMENT )
      {
        continue;
      }

      resource = crude_gfx_frame_graph_access_resource_by_name( graph, input_resource->name );
      graph_texture = crude_gfx_frame_graph_access_texture( graph, resource->resource_union.texture );

      CRUDE_ASSERT( ( framebuffer_size.x == 0 ) || ( framebuffer_size.x == graph_texture->texture->width ) );
      CRUDE_ASSERT( ( framebuffer_size.y == 0 ) || ( framebuffer_size.y == graph_texture->texture->height ) );

      framebuffer_size.x = graph_texture->texture->width;
      framebuffer_size.y = graph_texture->texture->height;

      if ( input_resource->type == CRUDE_GFX_FRAME_GRAPH_RESOURCE_TYPE_TEXTURE )
      {
        continue;
      }

      if ( crude_gfx_rhi_format_has_depth_or_stencil( graph_texture->texture->format ) )
      {
        framebuffer_creation.depth_stencil_texture = graph_texture->texture->handle;
      }
      else
      {
        framebuffer_creation.output_textures[ framebuffer_creation.num_render_targets++ ] = graph_texture->texture->handle;
      }
    }

    framebuffer_creation.width = framebuffer_size.x;
    framebuffer_creation.height = framebuffer_size.y;

    framebuffer_creation.manual_resources_free = true;

    node->resolution = framebuffer_size;

    node->framebuffer = crude_gfx_create_framebuffer( graph->gpu, &framebuffer_creation );
  }
}

void
crude_gfx_frame_graph_render_texture_barrier_
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_cmd_buffer                               *cmd,
  _In_ crude_gfx_frame_graph_texture_handle                handle,
  _In_ crude_gfx_frame_graph_resource_synchronization     *synchronization
)
{
  crude_gfx_frame_graph_texture *graph_texture = crude_gfx_frame_graph_access_texture( graph, handle );
  crude_gfx_cmd_add_image_barrier( cmd, graph_texture->texture->handle, synchronization->state, 0, graph_texture->texture->subresource.mip_level_count, crude_gfx_rhi_format_has_depth_or_stencil( graph_texture->texture->format ) );
}

void
crude_gfx_frame_graph_render_attachment_barrier_
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_cmd_buffer                               *cmd,
  _In_ crude_gfx_frame_graph_resource_union const         *resource_union,
  _In_ crude_gfx_frame_graph_resource_synchronization     *synchronization,
  _In_ bool                                                should_clear,
  _In_opt_ uint32                                          clear_index
)
{
  crude_gfx_frame_graph_texture *graph_texture = crude_gfx_frame_graph_access_texture( graph, resource_union->texture );

  if ( should_clear )
  {
    if ( crude_gfx_rhi_format_has_depth_or_stencil( graph_texture->texture->format ) )
    {
      crude_gfx_cmd_set_clear_depth_and_stencil( cmd, synchronization->clear_values.x, synchronization->clear_values.y );
    }
    else
    {
      crude_gfx_cmd_set_clear_color_f32( cmd, synchronization->clear_values.x, synchronization->clear_values.y, synchronization->clear_values.z, synchronization->clear_values.w, clear_index );
    }
  }

  if ( crude_gfx_rhi_format_has_depth_or_stencil( graph_texture->texture->format ) )
  {
    crude_gfx_cmd_add_image_barrier( cmd, graph_texture->texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_DEPTH_WRITE, 0, graph_texture->texture->subresource.mip_level_count, true );
  }
  else
  {
    crude_gfx_cmd_add_image_barrier( cmd, graph_texture->texture->handle, CRUDE_GFX_RHI_RESOURCE_STATE_RENDER_TARGET, 0, graph_texture->texture->subresource.mip_level_count, false );
  }
}

void
crude_gfx_frame_graph_render_memory_barrier_
(
  _In_ crude_gfx_frame_graph                              *graph,
  _In_ crude_gfx_cmd_buffer                               *cmd,
  _In_ crude_gfx_frame_graph_memory_handle                 handle,
  _In_ crude_gfx_rhi_resource_state                        new_state
)
{
  crude_gfx_frame_graph_memory *graph_memory = crude_gfx_frame_graph_access_memory( graph, handle );
  crude_gfx_cmd_buffer_barrier( cmd, graph_memory->allocation.buffer_handle, new_state );
}
