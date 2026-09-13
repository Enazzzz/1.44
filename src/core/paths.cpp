#include "paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace one44 {

std::string home_directory() {
#ifdef _WIN32
	if (const char *profile = std::getenv("USERPROFILE")) {
		return profile;
	}
	const char *drive = std::getenv("HOMEDRIVE");
	const char *path = std::getenv("HOMEPATH");
	if (drive && path) {
		return std::string(drive) + path;
	}
	return "C:\\";
#else
	if (const char *home = std::getenv("HOME")) {
		return home;
	}
	return "/tmp";
#endif
}

std::string samples_directory(bool create) {
	namespace fs = std::filesystem;
	const fs::path home(home_directory());
	const fs::path samples = home / "Samples";
	const fs::path music_samples = home / "Music" / "Samples";
	std::error_code ec;
	if (fs::is_directory(samples, ec)) {
		return samples.string();
	}
	ec.clear();
	if (fs::is_directory(music_samples, ec)) {
		return music_samples.string();
	}
	if (create) {
		ec.clear();
		fs::create_directories(samples, ec);
	}
	return samples.string();
}

} // namespace one44
