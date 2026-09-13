#include "wav_write.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace one44 {
namespace {

/// Writes a little-endian uint16.
void put_u16(std::ostream &out, uint16_t v) {
	char b[2];
	b[0] = static_cast<char>(v & 0xFF);
	b[1] = static_cast<char>((v >> 8) & 0xFF);
	out.write(b, 2);
}

/// Writes a little-endian uint32.
void put_u32(std::ostream &out, uint32_t v) {
	char b[4];
	b[0] = static_cast<char>(v & 0xFF);
	b[1] = static_cast<char>((v >> 8) & 0xFF);
	b[2] = static_cast<char>((v >> 16) & 0xFF);
	b[3] = static_cast<char>((v >> 24) & 0xFF);
	out.write(b, 4);
}

} // namespace

bool write_wav_pcm16(
	const std::string &path,
	const int16_t *pcm,
	uint64_t frames,
	int channels,
	double sample_rate,
	std::string *error) {
	auto fail = [&](const char *msg) {
		if (error) {
			*error = msg;
		}
		return false;
	};
	if (pcm == nullptr || frames == 0 || channels < 1 || sample_rate <= 0.0) {
		return fail("nothing to write");
	}
	const uint32_t sr = static_cast<uint32_t>(sample_rate + 0.5);
	const uint16_t ch = static_cast<uint16_t>(channels);
	const uint32_t data_bytes = static_cast<uint32_t>(frames * static_cast<uint64_t>(ch) * 2ull);
	const uint32_t byte_rate = sr * ch * 2u;
	const uint16_t block_align = static_cast<uint16_t>(ch * 2u);

	std::ofstream out(path, std::ios::binary);
	if (!out) {
		return fail("could not create WAV file");
	}
	out.write("RIFF", 4);
	put_u32(out, 36u + data_bytes);
	out.write("WAVE", 4);
	out.write("fmt ", 4);
	put_u32(out, 16);
	put_u16(out, 1); // PCM
	put_u16(out, ch);
	put_u32(out, sr);
	put_u32(out, byte_rate);
	put_u16(out, block_align);
	put_u16(out, 16);
	out.write("data", 4);
	put_u32(out, data_bytes);
	out.write(reinterpret_cast<const char *>(pcm), static_cast<std::streamsize>(data_bytes));
	if (!out) {
		return fail("WAV write failed");
	}
	return true;
}

std::string unique_clip_wav_path(const std::string &dir) {
	namespace fs = std::filesystem;
	const fs::path folder(dir);
	fs::path candidate = folder / "one44-clip.wav";
	std::error_code ec;
	if (!fs::exists(candidate, ec)) {
		return candidate.string();
	}
	for (int n = 2; n < 10000; ++n) {
		char name[64];
		std::snprintf(name, sizeof(name), "one44-clip-%d.wav", n);
		candidate = folder / name;
		ec.clear();
		if (!fs::exists(candidate, ec)) {
			return candidate.string();
		}
	}
	return (folder / "one44-clip-9999.wav").string();
}

} // namespace one44
