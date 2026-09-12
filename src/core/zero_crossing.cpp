#include "zero_crossing.hpp"

#include <cmath>

namespace one44 {
namespace {

/// Mono mix of one interleaved frame.
float frame_mono(const float *interleaved, int channels, uint64_t frame) {
	if (channels <= 1) {
		return interleaved[frame];
	}
	float sum = 0.0f;
	for (int c = 0; c < channels; ++c) {
		sum += interleaved[frame * static_cast<uint64_t>(channels) + static_cast<uint64_t>(c)];
	}
	return sum / static_cast<float>(channels);
}

} // namespace

/// Nearest sign-change or exact-zero sample; returns `index` if none exist.
uint64_t snap_to_zero_crossing(
	const float *interleaved,
	uint64_t frames,
	int channels,
	uint64_t index,
	uint64_t min_index,
	uint64_t max_index) {
	if (interleaved == nullptr || frames == 0 || channels <= 0) {
		return index;
	}
	if (max_index >= frames) {
		max_index = frames - 1;
	}
	if (min_index > max_index) {
		return index;
	}
	if (index < min_index) {
		index = min_index;
	}
	if (index > max_index) {
		index = max_index;
	}

	uint64_t best = index;
	uint64_t best_dist = frames;
	bool found = false;

	auto consider = [&](uint64_t i) {
		const uint64_t dist = i > index ? i - index : index - i;
		if (!found || dist < best_dist) {
			best = i;
			best_dist = dist;
			found = true;
		}
	};

	for (uint64_t i = min_index; i <= max_index; ++i) {
		const float s = frame_mono(interleaved, channels, i);
		if (std::fabs(s) <= 1.0e-7f) {
			consider(i);
			continue;
		}
		if (i > min_index) {
			const float prev = frame_mono(interleaved, channels, i - 1);
			if ((prev < 0.0f && s >= 0.0f) || (prev > 0.0f && s <= 0.0f)) {
				consider(i);
			}
		}
	}

	return found ? best : index;
}

} // namespace one44
