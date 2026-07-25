#include <thirdparty/cr/cr.h>

#include <engine/core/alias.h>

CR_EXPORT int
cr_main
(
  _In_ struct cr_plugin                                   *ctx,
  _In_ enum cr_op                                          operation
)
{
  assert( ctx );
  return 0;
}
