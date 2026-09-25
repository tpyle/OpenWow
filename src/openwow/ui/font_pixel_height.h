#pragma once

#include <cmath>

namespace openwow::ui {

/// Converts a stored font size to the whole pixel height text is drawn at.
///
/// Font sizes are stored as floats that have been through UI-unit
/// conversions, so 14 can read back as 13.999999. Anything measuring text
/// (string widths, edit box carets) must round the way the compositor does
/// when drawing, not truncate, or it measures with a smaller face and drifts
/// further from the drawn text with every character. Non-positive or
/// non-finite sizes give 0 (no font).
[[nodiscard]] inline int ResolveFontPixelHeight(const double stored_size) {
  if (!std::isfinite(stored_size) || !(stored_size > 0.0)) {
    return 0;
  }
  return static_cast<int>(std::lround(stored_size));
}

}  // namespace openwow::ui
