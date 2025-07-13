#pragma once

#include "flog.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace config {

using timestamp_type = std::uint16_t; // uint16_t is enough to address approximately 18 hours of "content", otherwise use uint32_t. Using uint16_t does *not* reduce file size, but it *does* increase precision.

constexpr auto log_level{flog::Level::info}; // debug > info > warning > error > none.
constexpr auto server_listen_ip{"127.0.0.1"};
constexpr auto server_listen_port{31337};
constexpr auto server_max_age{7*24*60*60}; // Cache-Control:max_age=server_max_age.
constexpr auto skip{std::array{ // Remove these substrings from the subs cache.
	std::string_view{"[Applause]"}
	, std::string_view{"[Music]"}
	, std::string_view{"[Laughter]"}
	, std::string_view{"[ __ ]"}
	, std::string_view{"\""} // We either do this (remove the double quotes completely), or escape them "manually". Removing the quotes is easier (and potentially (but probably not) faster), so we'll do that, because who really gives a shit.
#if 1 // At some point YouTube started adding punctuation to the subs, which is nice, but it doesn't help us whatsoever.
	, std::string_view{"."}
	, std::string_view{","}
	, std::string_view{"?"}
	, std::string_view{"-"}
#endif
}};
constexpr auto results_per_page{2048}; // Because trying to do this using JS/HTML was a *bad* idea, but I'm invested now.
constexpr auto min_search_size{3}; // Min length of a search term. 1 is obviously useless, 2 is (more) manageable but realistically this should be set to something like 3 or 4.
constexpr auto substr_size_max{256}; // Max length of substring(s) returned by the search. Lower values reduce bandwidth, but also "reduce" context.
constexpr auto substr_size_min{32};
constexpr auto timestamp_length{8}; // Timestamps are written every (timestamp_length*sizeof(timestamp_type))'th *byte* of input string. Lower values increase search precision, but increase *.timestamps' size.

} // namespace config
