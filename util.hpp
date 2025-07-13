#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace util {

template<typename T>
constexpr
const char * c_str(
	T && s
) {
	if constexpr(requires {s.c_str();}) {
		return s.c_str();
	} else {
		return s;
	}
}

template<typename T>
constexpr
const auto * data(
	T && s
) {
	if constexpr(requires {s.data();}) {
		return s.data();
	} else {
		return s;
	}
}

template<typename T>
constexpr
auto strlen(
	T && x
) {
	if constexpr(requires {x.size();}) {
		return x.size();
	} else if constexpr(std::is_same_v<std::remove_cvref_t<T>, char>) {
		return sizeof(char);
	} else {
		return std::string_view{std::forward<T>(x)}.size();
	}
}

template<typename T, typename ... _T>
constexpr
std::size_t strcat(
	T * s
	, _T && ... argv
) {
	std::size_t size{strlen(*s)};

	([&] {
		size += strlen(std::forward<_T>(argv));
	}(), ...);

	s->reserve(size);

	([&] {
		*s += argv;
	}(), ...);

	return size;
}

class file { // Just a simple RAII wrapper, because dealing with fclose is a PITA.
public:
	constexpr file() = default;
	template<typename ... T> file(T && ... argv): _file{std::fopen(std::forward<T>(argv) ...)} {}
	~file() {if(_file != nullptr) [[likely]] {std::fclose(_file);}}

	file(const file &) = delete;
	constexpr file(file && other): _file{other._file} {other._file = nullptr;}
	file & operator =(const file &) = delete;
	constexpr file & operator =(file && rhs) {std::swap(_file, rhs._file); return *this;}

	constexpr operator bool() const {return _file != nullptr;}
	explicit operator std::FILE *() {return _file;}

private:
	std::FILE * _file{nullptr};
};

template<typename T, typename _T> requires (
	std::ranges::contiguous_range<T>
	&& !std::is_same_v<typename T::value_type, void>
	&& requires(T x) {x.resize({});}
	&& requires(T x) {x.data();}
	&& requires(T x) {x.size();}
)
auto read(
	const _T & path
) {
	file file{c_str(path), "rb"};

	if(!file) [[unlikely]] {
		return T{};
	}

	T result;

	std::fseek(static_cast<std::FILE *>(file), 0, SEEK_END);
	if constexpr(requires {result.try_resize({});}) { // Because StringZilla doesn't have a *_NO_EXCEPTIONS (or equivalent). And since we're here we might as well check the result (despite the fact that it'll (probably) never be false).
		if(!result.try_resize(std::ftell(static_cast<std::FILE *>(file))/sizeof(typename T::value_type))) [[unlikely]] {
			return T{}; // We have no idea what state "result" is in after a failed allocation. Not that it matters, because if we've managed to get here somehow, we got bigger problems.
		}
	} else {
		result.resize(std::ftell(static_cast<std::FILE *>(file))/sizeof(typename T::value_type));
	}
	std::rewind(static_cast<std::FILE *>(file));

	if(
		std::fread(result.data(), sizeof(typename T::value_type), result.size(), static_cast<std::FILE *>(file))
		!= result.size()
	) [[unlikely]] {
		return T{}; // Okay mom, I'm not ignoring the result. A lot of good will that do if the system is fucked enough to somehow fail an fread. TODO: Of course, the result.size() (or result.empty()) *must* be checked now, but I'm obviously not going to do that and just let the thing segfault in case we actually get here.
	}

	return result;
}

template<typename T, typename _T> requires (
	std::ranges::contiguous_range<T>
	&& !std::is_same_v<typename T::value_type, void>
	&& requires(T x) {x.data();}
	&& requires(T x) {x.size();}
)
bool write(
	const _T & path
	, const T & data
) {
	file file{c_str(path), "wb"};

	if(!file) [[unlikely]] {
		return false;
	}

	return std::fwrite(
		data.data()
		, sizeof(typename T::value_type)
		, data.size()
		, static_cast<std::FILE *>(file)
	) == data.size()*sizeof(typename T::value_type);
}

template<typename T> requires std::is_integral_v<T> && std::is_unsigned_v<T>
constexpr
T align
(
	const T x
	, const T n
) {
	return n*((x+(n-T{1}))/n);
}

constexpr
void json_escape(
	std::string * s
) {
	for(auto j{s->find('"')}; j != s->npos; j = s->find('"', j+strlen("\\\""))) {
		s->replace(j, 1, "\\\"");
	}
}

constexpr std::string json_escape(std::string s) {json_escape(&s); return s;}

template<typename T> requires requires(T x) {c_str(x);}
bool file_exists(
	const T & path
) {
	auto file{std::fopen(c_str(path), "r+")}; // The "+" is spooky, but it prevents false positives when trying to open a directory (which is something that happens, apparently).

	if(file == nullptr) [[unlikely]] { // Is it, though?
		return false;
	}

	std::fclose(file);

	return true;
}

consteval
auto path_separators(
) {
#if defined(__linux__)
	return std::string_view{"/"};
#elif defined(_WIN32)
	return std::string_view{"/\\"};
#elif defined(__APPLE__)
	return std::string_view{"/:"}; // I seriously doubt this'll even compile (I'm referring to the entire "project" here), but I feel like I should put this here for the sake of completeness.
#else
	static_assert(false);
#endif
}

consteval auto path_separator() {return path_separators().front();}

template<typename T>
auto extension(
	T && path
) {
	const std::string_view _path{data(std::forward<T>(path)), strlen(std::forward<T>(path))};
	const auto dot{_path.find_last_of('.')};

	if(dot == _path.npos) {
		return std::string{};
	}

	const auto _path_separator{_path.find_last_of(path_separator())};

	return dot < _path_separator ? std::string{} : std::string{_path.substr(dot)};
}

template<typename ... T>
std::string format(
	T && ... argv
) {
	std::array<char, 1024> buffer; // FIXME: Ought to be enough for anybody.

	if(const auto size{std::snprintf(buffer.data(), buffer.size(), std::forward<T>(argv) ...)}; size > 0) {
		return std::string{buffer.data(), static_cast<std::size_t>(size)};
	} else {
		assert(false);

		return std::string{};
	}
}

inline
std::string from_u8string(
	const std::u8string & s
) {
	return std::string(reinterpret_cast<const char *>(s.data()), s.size()); // I don't care, the entirety of std::u8* can fuck itself.
}

template<typename T>
const char8_t * to_char8_t(
	T && s
) {
	return reinterpret_cast<const char8_t *>(c_str(std::forward<T>(s))); // ^.
}

template<char Padding = '='>
constexpr
std::string base64_encode(
	const void * data
	, const std::size_t size
) {
	using block = std::array<std::uint8_t, 3>;

	static constexpr std::array<char, 64> alpha{
		  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q'
		, 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'
		, 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y'
		, 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
	};

	std::string _data;

	_data.resize(((size+(3-1))/3)*4);

	auto * p{_data.data()};

	for(auto && i: std::span{static_cast<const block *>(data), static_cast<const block *>(data)+(size/sizeof(block))}) {
		*(p++) = alpha[i[0]>>2];
		*(p++) = alpha[((i[0]<<4) & 0b111111) | (i[1]>>4)];
		*(p++) = alpha[((i[1]<<2) & 0b111111) | (i[2]>>6)];
		*(p++) = alpha[i[2] & 0b111111];
	}

	const auto * i{static_cast<const block::value_type *>(data)+((size/sizeof(block))*sizeof(block))};

	switch(size % 3) {
		case 1:
			*(p++) = alpha[i[0]>>2];
			*(p++) = alpha[((i[0]<<4) & 0b111111) /*| (i[1]>>4)*/];
			*(p++) = Padding;
			*(p++) = Padding;

			break;
		case 2:
			*(p++) = alpha[i[0]>>2];
			*(p++) = alpha[((i[0]<<4) & 0b111111) | (i[1]>>4)];
			*(p++) = alpha[((i[1]<<2) & 0b111111) /*| (i[2]>>6)*/];
			*(p++) = Padding;

			break;
		default:
			break;
	}

	return _data;
}

} // namespace util
