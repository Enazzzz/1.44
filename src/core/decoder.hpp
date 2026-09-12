#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace one44 {

/// Decoded linear PCM. Channel data is interleaved; samples are float32 in [-1, 1].
struct AudioBuffer {
	std::vector<float> interleaved;
	uint32_t channels = 0;
	uint64_t frame_count = 0;
	double sample_rate = 0.0;
	std::string source_path;
};

/// Decode failure kinds used by tests and the GUI.
enum class DecodeError {
	None,
	UnsupportedFormat,
	ReadFailed,
	Empty,
};

/// Result of a WAV/MP3 decode. `error == None` means `audio` holds PCM.
struct DecodeResult {
	AudioBuffer audio;
	DecodeError error = DecodeError::None;
	std::string message;
};

/// Decodes a file from disk. WAV and MP3 are accepted; PCM is always float internally.
DecodeResult decode_file(const std::string &path);

/// Decodes a memory blob. `hint` is "wav", "mp3", or empty for magic-byte sniffing.
DecodeResult decode_memory(const void *data, size_t size, const char *hint);

} // namespace one44
