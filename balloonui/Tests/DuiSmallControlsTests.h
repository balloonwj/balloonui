#pragma once

#include "../BalloonUiFeatures.h"
#if BUI_FEATURE_PROGRESSBAR && BUI_FEATURE_SLIDER && BUI_FEATURE_COMBOBOX

#include "../Controls/Feedback/DuiProgressBar.h"
#include "../Controls/Input/DuiSlider.h"
#include "../Controls/Input/DuiComboBox.h"

namespace balloonwjui {

namespace DuiSmallControlsTests {
    CString RunAll();
}

} // namespace balloonwjui

#endif // BUI_FEATURE_PROGRESSBAR && BUI_FEATURE_SLIDER && BUI_FEATURE_COMBOBOX
