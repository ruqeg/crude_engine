#pragma once

#include "engine/core/alias.h"
#include "engine/core/memory.h"
#include "engine/core/resource_pool.h"
#include "engine/core/string.h"
#include <engine/core/hashmapstr.h>

typedef struct crude_cvar_handle
{
  uint32                                                   index;
} crude_cvar_handle;

typedef enum crude_cvar_access
{
  CRUDE_CVAR_ACCESS_READONLY,
  CRUDE_CVAR_ACCESS_READWRITE,
} crude_cvar_access;

typedef enum crude_cvar_value_type
{
  CRUDE_CVAR_VALUE_TYPE_BOOL,
  CRUDE_CVAR_VALUE_TYPE_INT32,
  CRUDE_CVAR_VALUE_TYPE_FLOAT32,
  CRUDE_CVAR_VALUE_TYPE_STRING_VIEW,
} crude_cvar_value_type;

typedef struct crude_cvar_value_union
{
  union
  {
    bool                                                   b;
    int32                                                  i;
    float32                                                f;
    crude_string_view                                      s;
  };
} crude_cvar_value_union;

typedef struct crude_cvar
{
  crude_cvar_value_union                                   data;
  crude_cvar_value_type                                    type;
  crude_cvar_access                                        access;
};

typedef struct crude_cvar_manager
{
  CRUDE_HASHMAPSTR( crude_cvar_handle )                   *name_to_cvar_handle;
  crude_resource_pool                                      cvars_resource_pool;
  crude_heap_allocator                                     allocator;
} crude_cvar_mananager;

CRUDE_API void
crude_cvar_manager_initialize
(
  _In_ crude_cvar_manager                                 *manager
);

CRUDE_API void crude_cvar_manager_deinitialize
(
  _In_ crude_cvar_manager                                 *manager
);

CRUDE_API crude_cvar*
crude_cvar_manager_access_cvar
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle
);

CRUDE_API crude_cvar_handle
crude_cvar_manager_find
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_string_view const                             name
);

CRUDE_API crude_cvar_handle
crude_cvar_manager_create
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_string_view const                             name,
  _In_ crude_cvar_access                                   access,
  _In_ crude_cvar_value_type                               type,
  _In_ crude_cvar_value_union                              data
);

CRUDE_API void
crude_cvar_manager_set_bool
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle,
  _In_ bool                                                value
);

CRUDE_API float32
crude_cvar_manager_get_float32
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle
);

CRUDE_API bool
crude_cvar_manager_get_bool
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle
);

CRUDE_API int32
crude_cvar_manager_get_int32
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle
);

CRUDE_API crude_string_view
crude_cvar_manager_get_string_view
(
  _In_ crude_cvar_manager                                 *manager,
  _In_ crude_cvar_handle                                   handle
);

CRUDE_API bool
crude_cvar_handle_valid
(
  _In_ crude_cvar_handle                                   handle
);

CRUDE_API crude_cvar_manager*
crude_cvar_manager_instance
(
);

#define CRUDE_CVAR_DEFINE_FLOAT32( _var, _name, _access, _value )\
  crude_cvar_handle _var = crude_cvar_manager_create( crude_cvar_manager_instance( ), CRUDE_STRING_VIEW( _name ), _access, CRUDE_CVAR_VALUE_TYPE_FLOAT32, CRUDE_COMPOUNT( crude_cvar_value_union, { .f = _value } ) );

#define CRUDE_CVAR_DEFINE_BOOL( _var, _name, _access, _value )\
  crude_cvar_handle _var = crude_cvar_manager_create( crude_cvar_manager_instance( ), CRUDE_STRING_VIEW( _name ), _access, CRUDE_CVAR_VALUE_TYPE_BOOL, CRUDE_COMPOUNT( crude_cvar_value_union, { .b = _value } ) );

#define CRUDE_CVAR_DEFINE_INT32( _var, _name, _access, _value )\
  crude_cvar_handle _var = crude_cvar_manager_create( crude_cvar_manager_instance( ), CRUDE_STRING_VIEW( _name ), _access, CRUDE_CVAR_VALUE_TYPE_INT32, CRUDE_COMPOUNT( crude_cvar_value_union, { .i = _value } ) );

#define CRUDE_CVAR_DEFINE_STRING_VIEW( _var, _name, _access, _value )\
  crude_cvar_handle _var = crude_cvar_manager_create( crude_cvar_manager_instance( ), CRUDE_STRING_VIEW( _name ), _access, CRUDE_CVAR_VALUE_TYPE_STRING_VIEW, CRUDE_COMPOUNT( crude_cvar_value_union, { .s = CRUDE_STRING_VIEW( _value ) } ) );

#define CRUDE_CVAR_GET_FLOAT32( _var )\
  crude_cvar_manager_get_float32( crude_cvar_manager_instance( ), _var )

#define CRUDE_CVAR_GET_INT32( _var )\
  crude_cvar_manager_get_int32( crude_cvar_manager_instance( ), _var )

#define CRUDE_CVAR_GET_STRING_VIEW( _var )\
  crude_cvar_manager_get_string_view( crude_cvar_manager_instance( ), _var )

#define CRUDE_CVAR_GET_BOOL( _var )\
  crude_cvar_manager_get_bool( crude_cvar_manager_instance( ), _var )

#define CRUDE_CVAR_SET_BOOL( _var, _value )\
  crude_cvar_manager_set_bool( crude_cvar_manager_instance( ), _var, _value );

#define CRUDE_CVAR_FIND( _name )\
  crude_cvar_manager_find( crude_cvar_manager_instance( ), _name )
