#pragma once

#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_SEPARATOR && BUI_FEATURE_BADGE && BUI_FEATURE_GROUPBOX \
 && BUI_FEATURE_SEARCHBOX && BUI_FEATURE_SPINBOX && BUI_FEATURE_SLIDER \
 && BUI_FEATURE_PROGRESSBAR && BUI_FEATURE_SCROLLBAR

#include "../Controls/Basic/DuiSeparator.h"
#include "../Controls/Basic/DuiBadge.h"
#include "../Controls/Basic/DuiGroupBox.h"
#include "../Controls/Input/DuiSearchBox.h"
#include "../Controls/Input/DuiSpinBox.h"
#include "../Controls/Input/DuiSlider.h"
#include "../Controls/Feedback/DuiProgressBar.h"
#include "../Controls/Window/DuiScrollBar.h"
#include "../Controls/Layout/DuiLayout.h"

namespace balloonwjui {

namespace DuiTier3Tests {
CString RunAll();
}

} // namespace balloonwjui

#endif // tier3 features all on
