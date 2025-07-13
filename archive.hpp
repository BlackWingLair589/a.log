#pragma once

#include "config.hpp"
#include "util.hpp"

#include <rapidjson/document.h>
#include <stringzilla/stringzilla.hpp>

#ifdef USE_REGEX
#include <re2/re2.h>
#endif // USE_REGEX

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class archive {
public:
	struct source {
		template<typename T>
		struct file {
			std::string path;
			T data;

			constexpr file() = default;

			template<typename _T>
			file(
				_T && path
			):
				path{std::forward<_T>(path)}
				, data{util::read<T>(path)}
			{
			}

			template<typename _T, typename __T>
			file(
				_T && path
				, __T && data
			):
				path{std::forward<_T>(path)}
				, data{std::forward<__T>(data)}
			{
			}

			file(const file &) = delete;
			constexpr file(file &&) = default;
			file & operator =(const file &) = delete;
			constexpr file & operator =(file &&) = default;
		};

		struct format {
			struct video_stream { // "vcodec" != "none"
				int fps;
				unsigned width;
				unsigned height;
			};

			struct audio_stream { // "acodec" != "none"
			};

			std::optional<audio_stream> audio;
			std::optional<video_stream> video;
			std::string container;
			std::string format_id;
			std::uint64_t filesize;

			inline bool load(rapidjson::Document::ConstObject && i);
		};

		std::vector<format> formats;
		std::string id;
		std::string info;
		std::string subs;
		file<ashvardanian::stringzilla::string> text;
		file<std::vector<config::timestamp_type> > timestamps;
		std::string title;
		std::int32_t upload_date; // time_since_epoch (seconds).

		inline bool load(rapidjson::Document::Object && i);
	};

	constexpr archive() = default;

	template<typename T>
	archive(
		T && archive
	) {
		if(!util::file_exists(std::forward<T>(archive))) {
			return;
		}

		auto _archive{util::read<std::string>(std::forward<T>(archive))};
		rapidjson::Document document;

		document.ParseInsitu(_archive.data());

		if(!document.IsArray()) [[unlikely]] {
			return; // TODO: Log error.
		}

		const auto _document{document.GetArray()};

		_sources.reserve(_document.Size());

		for(const auto & i: _document) {
			const auto
				formats{i.FindMember("formats")}
				, id{i.FindMember("id")}
				, info{i.FindMember("info")}
				, subs{i.FindMember("subs")}
				, text{i.FindMember("text")}
				, timestamps{i.FindMember("timestamps")}
				, title{i.FindMember("title")}
				, upload_date{i.FindMember("upload_date")}
			;

			if(
				(formats == i.MemberEnd()  || !formats->value.IsArray())
				|| (id == i.MemberEnd() || !id->value.IsString())
				|| (info == i.MemberEnd() || !info->value.IsString())
				|| (subs == i.MemberEnd() || !subs->value.IsString())
				|| (text == i.MemberEnd() || !text->value.IsString())
				|| (timestamps == i.MemberEnd() || !timestamps->value.IsString())
				|| (title == i.MemberEnd() || !title->value.IsString())
				|| (upload_date == i.MemberEnd() || !upload_date->value.IsInt())
			) [[unlikely]] {
				continue; // TODO: Log warning.
			}

			const std::string
				_info{info->value.GetString(), info->value.GetStringLength()}
				, _subs{subs->value.GetString(), subs->value.GetStringLength()}
				, _text{text->value.GetString(), text->value.GetStringLength()}
				, _timestamps{timestamps->value.GetString(), timestamps->value.GetStringLength()}
			;

			if(
				!util::file_exists(_info)
				|| !util::file_exists(_subs)
				|| !util::file_exists(_text)
				|| !util::file_exists(_timestamps)
			) [[unlikely]] {
				continue; // TODO: Log warning.
			}

			source source{
				.formats = [&] {
					decltype(source::formats) _formats;

					_formats.reserve(formats->value.GetArray().Size());
					for(const auto & i: formats->value.GetArray()) {
						const auto
							container{i.FindMember("container")}
							, format_id{i.FindMember("format_id")}
							, filesize{i.FindMember("filesize")}
							, audio{i.FindMember("audio")}
							, video{i.FindMember("video")}
						;

						if(
							(container == i.MemberEnd() || !container->value.IsString())
							|| (format_id == i.MemberEnd() || !format_id->value.IsString())
							|| (filesize == i.MemberEnd() || !filesize->value.IsUint64())
							|| (
								(audio == i.MemberEnd() || !audio->value.IsObject())
								&& (video == i.MemberEnd() || !video->value.IsObject())
							)
						) {
							continue; // TODO: Log error.
						}

						using audio_type = decltype(source::format::audio);
						using video_type = decltype(source::format::video);

						_formats.emplace_back(decltype(_formats)::value_type{
							.audio = [&]()->audio_type {
								if(audio == i.MemberEnd()) {
									return {};
								}

								return source::format::audio_stream{};
							}()
							, .video = [&]()->video_type {
								if(video == i.MemberEnd()) {
									return {};
								}

								const auto
									fps{video->value.FindMember("fps")}
									, width{video->value.FindMember("width")}
									, height{video->value.FindMember("height")}
								;

								return source::format::video_stream{
									.fps = fps->value.GetInt()
									, .width = width->value.GetUint()
									, .height = height->value.GetUint()
								};
							}()
							, .container = std::string{container->value.GetString(), container->value.GetStringLength()}
							, .format_id = std::string{format_id->value.GetString(), format_id->value.GetStringLength()}
							, .filesize = filesize->value.GetUint64()
						});
					}

					return _formats;
				}()
				, .id = {id->value.GetString(), id->value.GetStringLength()}
				, .info = std::move(_info)
				, .subs = std::move(_subs)
				, .text = decltype(source::text){_text}
				, .timestamps = decltype(source::timestamps){_timestamps}
				, .title = {title->value.GetString(), title->value.GetStringLength()}
				, .upload_date = static_cast<decltype(source::upload_date)>(upload_date->value.GetInt())
			};

			if(!source.formats.empty()) [[likely]] {
				_sources.emplace_back(std::move(source));
			}
		}

		std::sort(
			_sources.begin()
			, _sources.end()
			, [](const auto & lhs, const auto & rhs) {return lhs.upload_date > rhs.upload_date;}
		);
	}

	archive(const archive &) = delete;
	constexpr archive(archive &&) = default;
	archive & operator =(const archive &) = delete;
	constexpr archive & operator =(archive &&) = default;

	operator bool() const {return !empty();}
	source & operator [](const std::size_t i) {return _sources[i];}
	const source & operator [](const std::size_t i) const {return _sources[i];}

	template<typename T>
	void append(
		T && source
	) {
		_sources.emplace(
			std::lower_bound(
				_sources.begin()
				, _sources.end()
				, std::forward<T>(source)
				, [](const auto & lhs, const auto & rhs) {return lhs.upload_date > rhs.upload_date;}
			)
			, std::forward<T>(source)
		);
	}

	constexpr auto begin() {return _sources.begin();}
	constexpr auto begin() const {return _sources.begin();}
	constexpr bool empty() const {return _sources.empty();}
	constexpr auto end() {return _sources.end();}
	constexpr auto end() const {return _sources.end();}
	template<typename S, typename F> void find(S && substr, F && f) const;
	void reserve(const std::size_t new_cap) {_sources.reserve(new_cap);}
	constexpr auto size() const {return _sources.size();}

	void store(
		std::FILE * file
	) const {
		std::fputc('[', file);
		for(char separator{' '}; const auto & i: _sources) {
			std::fprintf(file, "%c{\"formats\":[", separator);
			for(char _separator{' '}; const auto & j: i.formats) {
				std::fprintf(
					file
					, "%c{\"container\":\"%s\",\"format_id\":\"%s\",\"filesize\":%ju"
					, _separator
					, j.container.c_str()
					, j.format_id.c_str()
					, std::uintmax_t{j.filesize}
				);
				if(j.video.has_value()) {
					std::fprintf(file, ",\"video\":{\"fps\":%i,\"width\":%u,\"height\":%u}", j.video->fps, j.video->width, j.video->height);
				}
				if(j.audio.has_value()) {
					std::fprintf(file, ",\"audio\":{}");
				}
				std::fputc('}', file);

				_separator = ',';
			}
			std::fputc(']', file);
			std::fprintf(
				file
				, ",\"id\":\"%s\",\"info\":\"%s\",\"subs\":\"%s\",\"text\":\"%s\",\"timestamps\":\"%s\",\"title\":\"%s\",\"upload_date\":%ji}"
				, i.id.c_str()
				, util::json_escape(i.info).c_str()
				, util::json_escape(i.subs).c_str()
				, util::json_escape(i.text.path).c_str()
				, util::json_escape(i.timestamps.path).c_str()
				, util::json_escape(i.title).c_str()
				, std::intmax_t{i.upload_date}
			);

			separator = ',';
		}
		std::fputc(']', file);
	}

private:
	std::vector<source> _sources;
};

inline
bool archive::source::format::load(
	rapidjson::Document::ConstObject && i
) {
#if 1
	if(i.ObjectEmpty()) [[unlikely]] {
		return false;
	}
#endif

	const auto
		container{i.FindMember("container")}
		, filesize{i.FindMember("filesize")}
		, format_id{i.FindMember("format_id")}
	;
	std::string_view _container;

	if(
		(
			(container == i.MemberEnd() || !container->value.IsString())
			|| (_container = std::string_view{container->value.GetString(), container->value.GetStringLength()}).find("_dash") == _container.npos // We need a "_dash" container to download a segment.
		)
		|| (filesize == i.MemberEnd() || !filesize->value.IsUint64())
		|| (format_id == i.MemberEnd() || !format_id->value.IsString())
	) [[unlikely]] {
		return false;
	}

	if(const auto vcodec{i.FindMember("vcodec")};
		vcodec != i.MemberEnd()
		&& vcodec->value.IsString()
		&& std::string_view{vcodec->value.GetString(), vcodec->value.GetStringLength()} != "none"
	) {
		const auto
			fps{i.FindMember("fps")}
			, width{i.FindMember("width")}
			, height{i.FindMember("height")}
		;

		if(
			(fps == i.MemberEnd() || !fps->value.IsInt())
			|| (width == i.MemberEnd() || !width->value.IsNumber())
			|| (height == i.MemberEnd() || !height->value.IsNumber())
		) [[unlikely]] {
			return false;
		}

		this->video = video_stream{
			.fps = fps->value.GetInt()
			, .width = width->value.GetUint()
			, . height = height->value.GetUint()
		};
	}

	if(const auto acodec{i.FindMember("acodec")};
		acodec != i.MemberEnd()
		&& acodec->value.IsString()
		&& std::string_view{acodec->value.GetString(), acodec->value.GetStringLength()} != "none"
	) {
		this->audio = audio_stream{};
	}

	if(
		!this->video.has_value()
		&& !this->audio.has_value()
	) [[unlikely]] {
		return false;
	}

	this->container = std::string_view{container->value.GetString(), container->value.GetStringLength()};
	this->format_id = std::string_view{format_id->value.GetString(), format_id->value.GetStringLength()};
	this->filesize = filesize->value.GetUint64();

	return true;
}

inline
bool archive::source::load(
	rapidjson::Document::Object && i
) {
#if 1
	if(i.ObjectEmpty()) [[unlikely]] {
		return false;
	}
#endif

	const auto
		formats{i.FindMember("formats")}
		, id{i.FindMember("id")}
		, title{i.FindMember("title")}
		, upload_date{i.FindMember("upload_date")}
	;
	std::string_view _upload_date;

	if(
		(formats == i.MemberEnd() || !formats->value.IsArray())
		|| (id == i.MemberEnd() || !id->value.IsString())
		|| (title == i.MemberEnd() || !title->value.IsString())
		|| (
			(upload_date == i.MemberEnd() || !upload_date->value.IsString())
			|| ((_upload_date = std::string_view{upload_date->value.GetString(), upload_date->value.GetStringLength()}).length() != 4+2+2)
		)
	) [[unlikely]] {
		return false;
	}

	{
		const auto _formats{formats->value.GetArray()};

		this->formats.reserve(_formats.Size());
		for(const auto & j: formats->value.GetArray()) {
			source::format format;

			if(format.load(j.GetObject())) {
				this->formats.emplace_back(std::move(format));
			}
		}
	}

	if(this->formats.empty()) {
		return false;
	}

	this->id = std::string_view{id->value.GetString(), id->value.GetStringLength()};
	this->title = std::string_view{title->value.GetString(), title->value.GetStringLength()};

	{
		using namespace std::chrono; // This piece of shit is way too verbose.

		const auto ymd{
			year{std::stoi(std::string{_upload_date.substr(0, 4)})}
			/month{static_cast<unsigned>(std::stoul(std::string{_upload_date.substr(4, 2)}))}
			/day{static_cast<unsigned>(std::stoul(std::string{_upload_date.substr(6, 2)}))}
		};

		this->upload_date = static_cast<decltype(this->upload_date)>(duration_cast<seconds>(sys_days{ymd}.time_since_epoch()).count());
	}

	return true;
}

template<typename S, typename F>
void archive::find
(
	S && substr
	, F && f
) const {
#ifdef USE_REGEX
	const re2::RE2 regex{std::string{'('}+std::forward<S>(substr)+std::string{')'}}; // FIXME: FindAndConsume(...) fails unless the *entire* expression is a group (or we omit the result arg altogether). I don't know enough about regexes to know what kind of side effects this can have, but it seems to just work(tm).

	if(!regex.ok()) {
		return; // TODO: Log error.
	}

	absl::string_view result;

	for(const auto & i: _sources) {
		absl::string_view text(i.text.data.data(), i.text.data.size());

		while(re2::RE2::FindAndConsume(&text, regex, &result)) {
			const auto j{text.data()-i.text.data.data()};

			std::forward<F>(f)(
				std::string_view(i.text.data.data(), i.text.data.size())
				, j-result.size()
				, result.size()
				, i.timestamps.data[j/(config::timestamp_length*sizeof(config::timestamp_type))]
				, i
			);
		}
	}
#else // !USE_REGEX
	for(const auto length{util::strlen(std::forward<S>(substr))}; const auto & i: _sources) {
		for(
			std::size_t j{i.text.data.find(std::forward<S>(substr))}
			; j != i.text.data.npos
			; j = i.text.data.find(std::forward<S>(substr), j+length)
		) {
			std::forward<F>(f)(
				std::string_view{i.text.data}
				, j
				, substr.size()
				, i.timestamps.data[j/(config::timestamp_length*sizeof(config::timestamp_type))]
				, i
			);
		}
	}
#endif // USE_REGEX
}
