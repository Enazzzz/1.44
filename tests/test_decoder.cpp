#include "check.hpp"
#include "core/decoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace one44;

namespace {

/// Writes a 16-bit PCM WAV (little-endian) for decoder tests.
void write_wav_file(const std::string &path, int sr, int channels, const std::vector<int16_t> &pcm) {
	const uint32_t data_bytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
	const uint32_t byte_rate = static_cast<uint32_t>(sr * channels * 2);
	const uint16_t block_align = static_cast<uint16_t>(channels * 2);
	std::ofstream out(path, std::ios::binary);
	auto put_u32 = [&](uint32_t v) {
		char b[4];
		b[0] = static_cast<char>(v & 0xFF);
		b[1] = static_cast<char>((v >> 8) & 0xFF);
		b[2] = static_cast<char>((v >> 16) & 0xFF);
		b[3] = static_cast<char>((v >> 24) & 0xFF);
		out.write(b, 4);
	};
	auto put_u16 = [&](uint16_t v) {
		char b[2];
		b[0] = static_cast<char>(v & 0xFF);
		b[1] = static_cast<char>((v >> 8) & 0xFF);
		out.write(b, 2);
	};
	out.write("RIFF", 4);
	put_u32(36 + data_bytes);
	out.write("WAVE", 4);
	out.write("fmt ", 4);
	put_u32(16);
	put_u16(1);
	put_u16(static_cast<uint16_t>(channels));
	put_u32(static_cast<uint32_t>(sr));
	put_u32(byte_rate);
	put_u16(block_align);
	put_u16(16);
	out.write("data", 4);
	put_u32(data_bytes);
	out.write(reinterpret_cast<const char *>(pcm.data()), static_cast<std::streamsize>(data_bytes));
}

/// Builds a short sine (or dual-channel sine) at 440 Hz.
std::vector<int16_t> sine_pcm(int sr, int channels, double seconds, double hz) {
	const int frames = static_cast<int>(sr * seconds);
	std::vector<int16_t> pcm(static_cast<size_t>(frames * channels));
	for (int i = 0; i < frames; ++i) {
		const float s = std::sin(2.0f * 3.14159265f * static_cast<float>(hz) * static_cast<float>(i) / static_cast<float>(sr));
		const auto q = static_cast<int16_t>(s * 16000.0f);
		for (int c = 0; c < channels; ++c) {
			pcm[static_cast<size_t>(i * channels + c)] = q;
		}
	}
	return pcm;
}

std::string fixture_dir() {
#ifdef TEST_FIXTURE_DIR
	return TEST_FIXTURE_DIR;
#else
	return ".";
#endif
}

} // namespace

TEST(wav_decodes_to_pcm) {
	const std::string path = fixture_dir() + "/tone.wav";
	const int sr = 44100;
	const auto pcm = sine_pcm(sr, 2, 0.2, 440.0);
	write_wav_file(path, sr, 2, pcm);

	const DecodeResult result = decode_file(path);
	CHECK_EQ(static_cast<int>(result.error), static_cast<int>(DecodeError::None));
	CHECK_EQ(result.audio.channels, 2u);
	CHECK_NEAR(result.audio.sample_rate, 44100.0, 0.1);
	CHECK(result.audio.frame_count > 8000);
	CHECK_EQ(result.audio.interleaved.size(), result.audio.frame_count * 2);

	double peak = 0.0;
	for (float s : result.audio.interleaved) {
		peak = std::max(peak, static_cast<double>(std::fabs(s)));
	}
	CHECK(peak > 0.2);
}

TEST(mp3_decodes_to_pcm) {
	const std::string wav_path = fixture_dir() + "/tone_mp3_src.wav";
	const std::string mp3_path = fixture_dir() + "/tone.mp3";
	const auto pcm = sine_pcm(44100, 1, 0.35, 330.0);
	write_wav_file(wav_path, 44100, 1, pcm);

	const std::string cmd = "ffmpeg -y -loglevel error -i \"" + wav_path + "\" -codec:a libmp3lame -q:a 4 \"" + mp3_path + "\"";
	const int rc = std::system(cmd.c_str());
	CHECK_EQ(rc, 0);

	const DecodeResult result = decode_file(mp3_path);
	CHECK_EQ(static_cast<int>(result.error), static_cast<int>(DecodeError::None));
	CHECK(result.audio.channels >= 1);
	CHECK(result.audio.sample_rate > 0.0);
	CHECK(result.audio.frame_count > 1000);
	double peak = 0.0;
	for (float s : result.audio.interleaved) {
		peak = std::max(peak, static_cast<double>(std::fabs(s)));
	}
	CHECK(peak > 0.05);
}

TEST(unsupported_format_is_rejected) {
	const std::string path = fixture_dir() + "/not_audio.txt";
	std::ofstream(path) << "hello";
	const DecodeResult result = decode_file(path);
	CHECK_EQ(static_cast<int>(result.error), static_cast<int>(DecodeError::UnsupportedFormat));
}
