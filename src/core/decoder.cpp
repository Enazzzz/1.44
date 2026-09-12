#include "decoder.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <vector>

#define DR_WAV_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#include "dr_wav.h"
#include "dr_mp3.h"

namespace one44 {
namespace {

/// Reads an entire file into memory for format sniffing.
bool read_all_bytes(const std::string &path, std::vector<uint8_t> *out) {
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return false;
	}
	in.seekg(0, std::ios::end);
	const std::streamoff len = in.tellg();
	if (len <= 0) {
		out->clear();
		return true;
	}
	in.seekg(0, std::ios::beg);
	out->resize(static_cast<size_t>(len));
	in.read(reinterpret_cast<char *>(out->data()), static_cast<std::streamsize>(len));
	return static_cast<size_t>(in.gcount()) == out->size();
}

/// True when the blob looks like a RIFF/WAVE file.
bool looks_like_wav(const uint8_t *data, size_t size) {
	if (size < 12) {
		return false;
	}
	return std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WAVE", 4) == 0;
}

/// True when the blob looks like MPEG audio (ID3 tag or frame sync).
bool looks_like_mp3(const uint8_t *data, size_t size) {
	if (size < 3) {
		return false;
	}
	if (data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
		return true;
	}
	return data[0] == 0xFF && (data[1] & 0xE0) == 0xE0;
}

/// Hint string from a file extension, without the dot.
std::string extension_hint(const std::string &path) {
	const auto dot = path.find_last_of('.');
	if (dot == std::string::npos || dot + 1 >= path.size()) {
		return {};
	}
	std::string ext = path.substr(dot + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return ext;
}

/// Loads WAV bytes into an AudioBuffer via dr_wav.
bool decode_wav_memory(const void *data, size_t size, AudioBuffer *out, std::string *err) {
	unsigned int channels = 0;
	unsigned int sample_rate = 0;
	drwav_uint64 frames = 0;
	float *pcm = drwav_open_memory_and_read_pcm_frames_f32(data, size, &channels, &sample_rate, &frames, nullptr);
	if (pcm == nullptr) {
		*err = "WAV decode failed";
		return false;
	}
	out->channels = channels;
	out->sample_rate = static_cast<double>(sample_rate);
	out->frame_count = frames;
	out->interleaved.assign(pcm, pcm + frames * channels);
	drwav_free(pcm, nullptr);
	return true;
}

/// Loads MP3 bytes into an AudioBuffer via dr_mp3.
bool decode_mp3_memory(const void *data, size_t size, AudioBuffer *out, std::string *err) {
	drmp3_config cfg{};
	drmp3_uint64 frames = 0;
	float *pcm = drmp3_open_memory_and_read_pcm_frames_f32(data, size, &cfg, &frames, nullptr);
	if (pcm == nullptr) {
		*err = "MP3 decode failed";
		return false;
	}
	out->channels = cfg.channels;
	out->sample_rate = static_cast<double>(cfg.sampleRate);
	out->frame_count = frames;
	out->interleaved.assign(pcm, pcm + frames * cfg.channels);
	drmp3_free(pcm, nullptr);
	return true;
}

/// Fills a DecodeResult from in-memory bytes plus an optional format hint.
DecodeResult decode_blob(const void *data, size_t size, const char *hint, const std::string &path) {
	DecodeResult result;
	if (data == nullptr || size == 0) {
		result.error = DecodeError::Empty;
		result.message = "empty input";
		return result;
	}
	const auto *bytes = static_cast<const uint8_t *>(data);
	std::string kind = hint ? hint : "";
	std::transform(kind.begin(), kind.end(), kind.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	const bool want_wav = kind == "wav" || kind == "wave" || (kind.empty() && looks_like_wav(bytes, size));
	const bool want_mp3 = kind == "mp3" || kind == "mpeg" || (kind.empty() && looks_like_mp3(bytes, size));

	std::string err;
	bool ok = false;
	if (want_wav) {
		ok = decode_wav_memory(data, size, &result.audio, &err);
	} else if (want_mp3) {
		ok = decode_mp3_memory(data, size, &result.audio, &err);
	} else if (looks_like_wav(bytes, size)) {
		ok = decode_wav_memory(data, size, &result.audio, &err);
	} else if (looks_like_mp3(bytes, size)) {
		ok = decode_mp3_memory(data, size, &result.audio, &err);
	} else {
		result.error = DecodeError::UnsupportedFormat;
		result.message = "not a WAV or MP3 file";
		return result;
	}

	if (!ok) {
		result.error = DecodeError::ReadFailed;
		result.message = err;
		return result;
	}
	if (result.audio.frame_count == 0 || result.audio.channels == 0) {
		result.error = DecodeError::Empty;
		result.message = "decoded audio was empty";
		return result;
	}
	result.audio.source_path = path;
	result.error = DecodeError::None;
	return result;
}

} // namespace

/// Decodes WAV or MP3 from a filesystem path into float PCM.
DecodeResult decode_file(const std::string &path) {
	std::vector<uint8_t> bytes;
	if (!read_all_bytes(path, &bytes)) {
		DecodeResult result;
		result.error = DecodeError::ReadFailed;
		result.message = "could not open file";
		return result;
	}
	const std::string hint = extension_hint(path);
	return decode_blob(bytes.data(), bytes.size(), hint.c_str(), path);
}

/// Decodes WAV or MP3 from a memory buffer into float PCM.
DecodeResult decode_memory(const void *data, size_t size, const char *hint) {
	return decode_blob(data, size, hint, {});
}

} // namespace one44
