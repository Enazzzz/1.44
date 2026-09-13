#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace one44 {

/// Size of a canonical PCM WAV header (RIFF/fmt/data, no extra chunks).
constexpr size_t kWavPcmHeaderBytes = 44;

/// Writes a little-endian 16-bit PCM WAV. `pcm` is interleaved; `frames * channels` samples.
bool write_wav_pcm16(
	const std::string &path,
	const int16_t *pcm,
	uint64_t frames,
	int channels,
	double sample_rate,
	std::string *error);

/// `one44-clip.wav`, or `one44-clip-N.wav` if that name already exists in `dir`.
std::string unique_clip_wav_path(const std::string &dir);

} // namespace one44
