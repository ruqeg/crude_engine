#include "engine/core/alias.h"
#include "engine/core/hashmapstr.h"
#include "engine/core/memory.h"
#include "engine/core/resource_pool.h"
#include <engine/core/cvar.h>

crude_cvar_manager                                        *manager_ = NULL;

void
crude_cvar_manager_initialize
(
  _In_ crude_cvar_manager                                 *manager
)
{
  crude_heap_allocator_initialize( &manager->allocator, 1024 * ( sizeof( crude_cvar ) + 128 ), "cvar_manager_allocator" );
  crude_resource_pool_initialize( &manager->cvars_resource_pool, crude_heap_allocator_pack( &manager->allocator ), 1024, sizeof( crude_cvar ) );
  CRUDE_HASHMAPSTR_INITIALIZE( manager->name_to_cvar_handle, crude_heap_allocator_pack( &manager->allocator ) );
}

void
crude_cvar_manager_deinitialize
(
  _In_ crude_cvar_manager                                 *manager
)
{
  crude_resource_pool_free_all_resource( &manager->cvars_resource_pool );
  crude_resource_pool_deinitialize( &manager->cvars_resource_pool );
  CRUDE_HASHMAPSTR_DEINITIALIZE( manager->name_to_cvar_handle );
  crude_heap_allocator_deinitialize( &manager->allocator );
}

crude_cvar*
crude_cvar_manager_access_cvar
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle
)
{
  return CRUDE_CAST( crude_cvar*, crude_resource_pool_access_resource( &manager->cvars_resource_pool, handle.index ) );
}

crude_cvar_handle
crude_cvar_manager_find
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_string_view const                             name
)
{
  int64 handle_index = CRUDE_HASHMAPSTR_GET_INDEX( manager->name_to_cvar_handle, name );
  if ( handle_index < 0 )
  {
    return CRUDE_COMPOUNT( crude_cvar_handle, { .index = -1u } );
  }

  return manager->name_to_cvar_handle[ handle_index ].value;
}

crude_cvar_handle
crude_cvar_manager_create
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_string_view const                             name,
  _In_ crude_cvar_access                                   access,
  _In_ crude_cvar_value_type                               type,
  _In_ crude_cvar_value_union                              data
)
{
  crude_cvar                                              *cvar;
  crude_cvar_handle                                        cvar_handle;

  cvar_handle.index = crude_resource_pool_obtain_resource( &manager->cvars_resource_pool );
  cvar = CRUDE_CAST( crude_cvar*, crude_resource_pool_access_resource( &manager->cvars_resource_pool, cvar_handle.index ) );
  
  *cvar = CRUDE_COMPOUNT_EMPTY( crude_cvar );
  cvar->access = access;
  cvar->data = data;
  cvar->type = type;

  CRUDE_HASHMAPSTR_SET( manager->name_to_cvar_handle, name, cvar_handle );

  return cvar_handle;
}

void
crude_cvar_manager_set_bool
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle,
  _In_ bool                                                value
)
{
  crude_cvar                                              *cvar;

  cvar = CRUDE_CAST( crude_cvar*, crude_resource_pool_access_resource( &manager->cvars_resource_pool, handle.index ) );

  CRUDE_ASSERT( cvar->type == CRUDE_CVAR_VALUE_TYPE_BOOL );

  cvar->data.b = value;
}

float32
crude_cvar_manager_get_float32
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle
)
{
  crude_cvar                                              *cvar;

  cvar = CRUDE_CAST( crude_cvar*, crude_resource_pool_access_resource( &manager->cvars_resource_pool, handle.index ) );

  CRUDE_ASSERT( cvar->type == CRUDE_CVAR_VALUE_TYPE_FLOAT32 );

  return cvar->data.f;
}

bool
crude_cvar_manager_get_bool
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle
)
{
  crude_cvar                                              *cvar;

  cvar = CRUDE_CAST( crude_cvar*, crude_resource_pool_access_resource( &manager->cvars_resource_pool, handle.index ) );

  CRUDE_ASSERT( cvar->type == CRUDE_CVAR_VALUE_TYPE_BOOL );

  return cvar->data.b;
}

int32
crude_cvar_manager_get_int32
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle
)
{
  crude_cvar                                              *cvar;

  cvar = CRUDE_CAST( crude_cvar*, crude_resource_pool_access_resource( &manager->cvars_resource_pool, handle.index ) );

  CRUDE_ASSERT( cvar->type == CRUDE_CVAR_VALUE_TYPE_INT32 );

  return cvar->data.i;
}

crude_string_view
crude_cvar_manager_get_string_view
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle
)
{
  crude_cvar                                              *cvar;

  cvar = CRUDE_CAST( crude_cvar*, crude_resource_pool_access_resource( &manager->cvars_resource_pool, handle.index ) );

  CRUDE_ASSERT( cvar->type == CRUDE_CVAR_VALUE_TYPE_STRING_VIEW );

  return cvar->data.s;
}

bool
crude_cvar_handle_valid
(
  _In_ crude_cvar_handle                                   handle
)
{
  return handle.index != -1;
}

crude_cvar_manager*
crude_cvar_manager_instance
(
)
{
  if ( manager_ )
  {
    return manager_;
  }
  
  manager_ = CRUDE_CAST( crude_cvar_manager*, malloc( sizeof( crude_cvar_manager ) ) );
  crude_cvar_manager_initialize( manager_ );
  return manager_;
}
