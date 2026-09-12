#pragma once

#include <cstdint>
#include <vector>

namespace one44 {

/// Finds the nearest zero-crossing of the (mono-mixed) waveform to `index`,
/// clamped to `[min_index, max_index]`. Used by the optional loop-point snap.
uint64_t snap_to_zero_crossing(
	const float *interleaved,
	uint64_t frames,
	int channels,
	uint64_t index,
	uint64_t min_index,
	uint64_t max_index);

} // namespace one44
