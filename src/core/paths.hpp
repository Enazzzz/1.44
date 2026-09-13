#pragma once

#include <string>

namespace one44 {

/// User home (`USERPROFILE` on Windows, `HOME` elsewhere).
std::string home_directory();

/// Clip drop folder: `%USERPROFILE%\Samples` or `~/Samples`.
/// If that folder is missing but `%USERPROFILE%\Music\Samples` exists, that path is used instead.
/// When `create` is true and neither exists, `Samples` is created under home.
std::string samples_directory(bool create);

} // namespace one44
