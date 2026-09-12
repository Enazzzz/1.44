#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace one44 {

/// Linear interpolation at a fractional frame on interleaved float PCM.
inline float lerp_sample(const float *interleaved, uint64_t frames, int channels, int channel, double pos) {
	if (frames == 0 || channels <= 0) {
		return 0.0f;
	}
	if (pos < 0.0) {
		pos = 0.0;
	}
	const double max_pos = static_cast<double>(frames - 1);
	if (pos >= max_pos) {
		const uint64_t last = frames - 1;
		return interleaved[last * static_cast<uint64_t>(channels) + static_cast<uint64_t>(channel)];
	}
	const uint64_t i0 = static_cast<uint64_t>(pos);
	const uint64_t i1 = i0 + 1;
	const float t = static_cast<float>(pos - static_cast<double>(i0));
	const float a = interleaved[i0 * static_cast<uint64_t>(channels) + static_cast<uint64_t>(channel)];
	const float b = interleaved[i1 * static_cast<uint64_t>(channels) + static_cast<uint64_t>(channel)];
	return a + (b - a) * t;
}

/// Mixes all input channels to a single mono value at a fractional frame.
inline float lerp_mono(const float *interleaved, uint64_t frames, int channels, double pos) {
	if (channels <= 1) {
		return lerp_sample(interleaved, frames, channels, 0, pos);
	}
	float sum = 0.0f;
	for (int c = 0; c < channels; ++c) {
		sum += lerp_sample(interleaved, frames, channels, c, pos);
	}
	return sum / static_cast<float>(channels);
}

/// Quantizes a float sample in [-1, 1] to signed 16-bit (ASR-10 depth).
inline int16_t quantize_i16(float sample) {
	const float clipped = std::max(-1.0f, std::min(1.0f, sample));
	const int rounded = static_cast<int>(std::lrint(clipped * 32767.0f));
	return static_cast<int16_t>(std::max(-32768, std::min(32767, rounded)));
}

/// Reads a 16-bit committed sample as float in [-1, 1].
inline float i16_to_float(int16_t sample) {
	return static_cast<float>(sample) / 32768.0f;
}

/// Linear-interpolates 16-bit interleaved PCM at a fractional frame.
inline float lerp_i16(const int16_t *interleaved, uint64_t frames, int channels, int channel, double pos) {
	if (frames == 0 || channels <= 0) {
		return 0.0f;
	}
	if (pos < 0.0) {
		pos = 0.0;
	}
	const double max_pos = static_cast<double>(frames - 1);
	if (pos >= max_pos) {
		const uint64_t last = frames - 1;
		return i16_to_float(interleaved[last * static_cast<uint64_t>(channels) + static_cast<uint64_t>(channel)]);
	}
	const uint64_t i0 = static_cast<uint64_t>(pos);
	const uint64_t i1 = i0 + 1;
	const float t = static_cast<float>(pos - static_cast<double>(i0));
	const float a = i16_to_float(interleaved[i0 * static_cast<uint64_t>(channels) + static_cast<uint64_t>(channel)]);
	const float b = i16_to_float(interleaved[i1 * static_cast<uint64_t>(channels) + static_cast<uint64_t>(channel)]);
	return a + (b - a) * t;
}

} // namespace one44
