#pragma once

#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_AVATAR

#include "../Controls/Basic/DuiAvatar.h"

namespace balloonwjui {

// Unit tests for DuiAvatar. Pure-API tests run with no HWND. Paint smoke
// tests use an off-screen memory DC + DIBSection so failures don't pop
// any UI.
namespace DuiAvatarTests
{

CString RunAll();

} // namespace DuiAvatarTests

} // namespace balloonwjui

#endif // BUI_FEATURE_AVATAR
