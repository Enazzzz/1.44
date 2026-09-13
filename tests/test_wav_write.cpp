#include "check.hpp"
#include "core/decoder.hpp"
#include "core/wav_write.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using namespace one44;

namespace {

std::string fixture_dir() {
#ifdef TEST_FIXTURE_DIR
	return TEST_FIXTURE_DIR;
#else
	return ".";
#endif
}

/// Little-endian 16-bit read used to inspect the WAV header.
uint16_t read_u16(const std::vector<uint8_t> &b, size_t off) {
	return static_cast<uint16_t>(b[off] | (static_cast<uint16_t>(b[off + 1]) << 8));
}

/// Little-endian 32-bit read used to inspect the WAV header.
	return static_cast<uint32_t>(b[off]) | (static_cast<uint32_t>(b[off + 1]) << 8) |
		(static_cast<uint32_t>(b[off + 2]) << 16) | (static_cast<uint32_t>(b[off + 3]) << 24);
}

/// Reads an entire file for header/payload assertions.
std::vector<uint8_t> read_all(const std::string &path) {
	std::ifstream in(path, std::ios::binary);
	CHECK(static_cast<bool>(in));
	in.seekg(0, std::ios::end);
	const std::streamoff n = in.tellg();
	in.seekg(0, std::ios::beg);
	std::vector<uint8_t> out(static_cast<size_t>(n));
	in.read(reinterpret_cast<char *>(out.data()), n);
	return out;
}

} // namespace

TEST(wav_writer_header_and_pcm16_payload_size) {
	const std::string path = fixture_dir() + "/export_clip.wav";
	const int channels = 2;
	const uint64_t frames = 100;
	std::vector<int16_t> pcm(static_cast<size_t>(frames * channels));
	for (size_t i = 0; i < pcm.size(); ++i) {
		pcm[i] = static_cast<int16_t>(i * 17);
	}
	std::string err;
	CHECK(write_wav_pcm16(path, pcm.data(), frames, channels, 44100.0, &err));

	const std::vector<uint8_t> bytes = read_all(path);
	const uint32_t data_bytes = static_cast<uint32_t>(frames * channels * 2ull);
	CHECK_EQ(bytes.size(), kWavPcmHeaderBytes + data_bytes);
	CHECK(std::memcmp(bytes.data(), "RIFF", 4) == 0);
	CHECK_EQ(read_u32(bytes, 4), 36u + data_bytes);
	CHECK(std::memcmp(bytes.data() + 8, "WAVE", 4) == 0);
	CHECK(std::memcmp(bytes.data() + 12, "fmt ", 4) == 0);
	CHECK_EQ(read_u32(bytes, 16), 16u);
	CHECK_EQ(read_u16(bytes, 20), 1u); // PCM
	CHECK_EQ(read_u16(bytes, 22), 2u);
	CHECK_EQ(read_u32(bytes, 24), 44100u);
	CHECK_EQ(read_u32(bytes, 28), 44100u * 2u * 2u);
	CHECK_EQ(read_u16(bytes, 32), 4u);
	CHECK_EQ(read_u16(bytes, 34), 16u);
	CHECK(std::memcmp(bytes.data() + 36, "data", 4) == 0);
	CHECK_EQ(read_u32(bytes, 40), data_bytes);
	CHECK(std::memcmp(bytes.data() + 44, pcm.data(), data_bytes) == 0);

	const DecodeResult decoded = decode_file(path);
	CHECK_EQ(static_cast<int>(decoded.error), static_cast<int>(DecodeError::None));
	CHECK_EQ(decoded.audio.frame_count, frames);
	CHECK_EQ(decoded.audio.channels, 2u);
}

TEST(unique_clip_wav_path_adds_numeric_suffix) {
	const std::string dir = fixture_dir() + "/clip_names";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	const std::string first = unique_clip_wav_path(dir);
	CHECK(first.find("one44-clip.wav") != std::string::npos);
	{
		std::ofstream(first, std::ios::binary) << "x";
	}
	const std::string second = unique_clip_wav_path(dir);
	CHECK(second.find("one44-clip-2.wav") != std::string::npos);
}
