#include "openwow/foundation/math/calc_swim_wave_bob.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using openwow::math::CalcSwimWaveBob;

TEST_CASE("CalcSwimWaveBob returns exactly 0 for zero amplitude, regardless of time",
          "[math][swimbob]") {
  CHECK(CalcSwimWaveBob(0, 0.0f) == 0.0f);
  CHECK(CalcSwimWaveBob(123456, 0.0f) == 0.0f);
}

TEST_CASE("CalcSwimWaveBob at time_ms=0 matches the closed-form 3*avg*amplitude*ampScale",
          "[math][swimbob]") {
  // At time_ms == 0, `base` (== time_ms * kDegToRad * amplitude) is 0
  // regardless of amplitude, so all three cos() terms evaluate to
  // cos(0) == 1 and the result collapses to a simple, exactly-derivable
  // linear function of amplitude.
  constexpr float kWaveAvgFactor = 0.333333f;
  constexpr float kWaveAmpScale = 0.013090f;

  for (const float amplitude : {1.0f, 2.0f, 5.5f}) {
    CAPTURE(amplitude);
    const float expected = 3.0f * kWaveAvgFactor * (amplitude * kWaveAmpScale);
    CHECK(CalcSwimWaveBob(0, amplitude) == Approx(expected).margin(0.0001));
  }
}

TEST_CASE("CalcSwimWaveBob is deterministic for a fixed (time, amplitude) pair",
          "[math][swimbob]") {
  const float first = CalcSwimWaveBob(5000, 1.5f);
  const float second = CalcSwimWaveBob(5000, 1.5f);
  CHECK(first == second);
}
