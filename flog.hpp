#pragma once

#include "util.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <string_view>
#include <utility>

#ifndef NDEBUG
#include <source_location>
#endif // !NDEBUG

namespace flog {

enum class Level : unsigned {
	debug
	, info
	, warning
	, error
	, none
};

inline
auto & level(
) {
	static Level _level{Level::debug};

	return _level;
}

template<typename T>
constexpr
void write(
	T && message
	, const Level level = Level::error
	, std::FILE * stream = stdout
#ifndef NDEBUG
	, const std::source_location source_location = std::source_location::current()
#endif // !NDEBUG
) {
	assert(std::to_underlying(level) <= std::to_underlying(Level::none)); // LMAO

	if(level < flog::level()) {
		return;
	}

	static constexpr std::array<std::string_view, std::to_underlying(Level::none)> _level{
		"DBUG"
		, "INFO"
		, "WARN"
		, "ERRR"
	};

#ifndef NDEBUG
	std::fprintf(
		stream
		, "[%s] %s:%ju: %s\n"
		, _level[std::to_underlying(level)].data()
		, source_location.file_name()
		, std::uintmax_t{source_location.line()}
		, util::c_str(std::forward<T>(message))
	);
#else // NDEBUG
	std::fprintf(stream, "[%s] %s\n", _level[std::to_underlying(level)].data(), util::c_str(std::forward<T>(message)));
#endif // !NDEBUG
}

} // namespace flog
