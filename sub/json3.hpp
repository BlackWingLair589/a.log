#pragma once

#include "../archive.hpp"
#include "../config.hpp"
#include "../util.hpp"

#include <rapidjson/document.h>
#include <unicode/uchar.h>
#include <utf8/unchecked.h>

namespace sub {

namespace detail {

class back_insert_iterator {
public:
	using difference_type = std::ptrdiff_t;
	using iterator_category = std::output_iterator_tag;
	using pointer = void;
	using reference = void;
	using value_type = void;

	constexpr back_insert_iterator(ashvardanian::stringzilla::string * string): _string{string} {}

	back_insert_iterator & operator =(const ashvardanian::stringzilla::string::value_type c) {_string->try_push_back(c); return *this;};
	constexpr back_insert_iterator & operator *() {return *this;}
	constexpr back_insert_iterator & operator ++() {return *this;}
	constexpr back_insert_iterator & operator ++(int) {return *this;}

private:
	ashvardanian::stringzilla::string * _string;
};

} // namespace detail

template<typename T>
bool json3(
	decltype(archive::source::text.data) * text
	, decltype(archive::source::timestamps.data) * timestamps
	, T && path
) {
	auto file{util::read<std::string>(std::forward<T>(path))};
	rapidjson::Document json;

	json.ParseInsitu(file.data());

	if(!json.IsObject()) [[unlikely]] {
		return false;
	}

	const auto events{json.FindMember("events")};

	if(
		events == json.MemberEnd()
		|| !events->value.IsArray()
	) [[unlikely]] {
		return false;
	}

	for(const auto & i: events->value.GetArray()) {
		const auto segs{i.FindMember("segs")};

		if(
			segs == i.MemberEnd()
			|| !segs->value.IsArray()
		) [[unlikely]] {
			continue; // TODO: Error.
		}

		config::timestamp_type timestamp;

		{
			const auto tStartMs{i.FindMember("tStartMs")};

			if(
				tStartMs == i.MemberEnd()
				|| !tStartMs->value.IsUint()
			) [[unlikely]] {
				continue; // TODO: Error.
			}

			timestamp = static_cast<decltype(timestamp)>(tStartMs->value.GetUint()/unsigned{1000}); // milliseconds to seconds.
		}

		const auto begin{text->size()};

		for(const auto & s: segs->value.GetArray()) {
			const auto utf8{s.FindMember("utf8")};

			if(
				utf8 == s.MemberEnd()
				|| !utf8->value.IsString()
			) [[unlikely]] {
				continue; // TODO: Error.
			}

			std::string _utf8{utf8->value.GetString(), utf8->value.GetStringLength()};

			for(const auto & skip: config::skip) {
				for(auto j{_utf8.find(skip)}; j != _utf8.npos; j = _utf8.find(skip, j)) {
					_utf8.erase(j, skip.size());
				}
			}

			const auto left{std::find_if_not(_utf8.begin(), _utf8.end(), [](int c) {return std::isspace(c);})};

			if(left == _utf8.end()) { // The entire string isspace.
				continue;
			}

			_utf8.resize(_utf8.size()-std::distance(_utf8.rbegin(), std::find_if_not(_utf8.rbegin(), _utf8.rend(), [](int c) {return std::isspace(c);}))); // This abomination is basically rtrim, but retarded. The reason this works (LOL) is that there's no overlap between what is considered a space (by isspace) and "trailing" UTF-8 characters, so there's no need to fuck around with UTF-16/32 or whatever.

			for(auto c{&(*left)}, end{_utf8.data()+_utf8.size()}; c < end;) {
				auto _c{utf8::unchecked::next(c)};

				_c = u_tolower(_c);
				utf8::unchecked::utf32to8(&_c, &_c+1, detail::back_insert_iterator(text));
			}

			text->try_append(ashvardanian::stringzilla::string_view{" "});
		}

		constexpr auto block_size{config::timestamp_length*sizeof(config::timestamp_type)};

		for(
			auto end{text->size()}, k{util::align(begin, block_size)}
			; k < end
			; k += block_size
		) {
			timestamps->emplace_back(timestamp);
		}
	}

	text->try_shrink_to_fit(); // Useless, but why not.
	timestamps->shrink_to_fit(); // ^.

	return true;
}

} // namespace sub
