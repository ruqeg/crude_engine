#include <engine/core/cvar.h>
#include <editor/editor.h>
#include <engine/engine.h>

#include <editor/gui/cvard_editor.h>

CRUDE_GUI_BASE_DEFINE_CVAR( cvar_editor )

typedef struct crude_cvar_packed
{
  crude_cvar                                              *cvar;
  crude_string_view                                        name;
} crude_cvar_packed;

static int32
cmp_str_
(
  _In_ void const                                          *a,
  _In_ void const                                          *b
);

CRUDE_GUI_BASE_INITIALIZE_START( cvar_editor )
  cvar_editor->allocator = &engine->common_allocator;
CRUDE_GUI_BASE_INITIALIZE_END( cvar_editor )

CRUDE_GUI_BASE_DEINITIALIZE_START( cvar_editor )
CRUDE_GUI_BASE_DEINITIALIZE_END( cvar_editor )

CRUDE_GUI_BASE_UPDATE_START( cvar_editor )
CRUDE_GUI_BASE_UPDATE_END( cvar_editor )

CRUDE_GUI_BASE_QUEUE_DRAW_START( cvar_editor )
  crude_cvar_manager                                      *manager;
  crude_cvar_packed                                       *cvars_packed;
  ImGuiViewport const                                     *im_viewport;
  ImGuiWindowFlags                                         im_flags;

  if ( !CRUDE_CVAR_GET_BOOL( cvar_editor_gui_cvar_editor_enabled ) )
  {
    return;
  }

  manager = crude_cvar_manager_instance( );

  CRUDE_ARRAY_INITIALIZE_WITH_CAPACITY( cvars_packed, CRUDE_HASHMAPSTR_CAPACITY( manager->name_to_cvar_handle ), crude_heap_allocator_pack( cvar_editor->allocator ) );

  for ( uint32 i = 0; i < CRUDE_HASHMAPSTR_CAPACITY( manager->name_to_cvar_handle ); ++i )
  {
    if ( crude_hashmapstr_backet_key_hash_valid( manager->name_to_cvar_handle[ i ].key.key_hash ) )
    {
      crude_cvar_packed                                    cvar_packed;

      cvar_packed.cvar = crude_cvar_manager_access_cvar( manager, manager->name_to_cvar_handle[ i ].value );
      cvar_packed.name = manager->name_to_cvar_handle[ i ].key.key;
      CRUDE_ARRAY_PUSH( cvars_packed, cvar_packed );
    }
  }

  qsort( cvars_packed, CRUDE_ARRAY_LENGTH( cvars_packed ), sizeof( crude_cvar_packed ), cmp_str_ );

  im_viewport = ImGui::GetMainViewport( );

  const ImGuiViewport* vp = ImGui::GetMainViewport();
ImDrawList* dl = ImGui::GetForegroundDrawList(); // or GetBackgroundDrawList()

ImVec2 p0 = vp->Pos;
ImVec2 p1 = ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);
dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));

const char* text = "Hello, world!";
float fontSize = 48.0f;                 // any size you like
ImFont* font = ImGui::GetFont();
ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
ImVec2 pos(vp->Pos.x + (vp->Size.x - ts.x) * 0.5f,
           vp->Pos.y + (vp->Size.y - ts.y) * 0.5f);
dl->AddText(font, fontSize, pos, IM_COL32(255, 255, 255, 255), text );

  CRUDE_ARRAY_DEINITIALIZE( cvars_packed );
CRUDE_GUI_BASE_QUEUE_DRAW_END( cvar_editor )

int32
cmp_str_
(
  _In_ void const                                          *a,
  _In_ void const                                          *b
)
{
  return strcmp( CRUDE_CAST( crude_cvar_packed*, a )->name.data, CRUDE_CAST( crude_cvar_packed*, b )->name.data );
}
