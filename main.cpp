#include "archive.hpp"
#include "config.hpp"
#include "flog.hpp"
#include "sub/json3.hpp"
#include "util.hpp"

#include <cmrc/cmrc.hpp>
#include <httplib.h>
#include <utf8/unchecked.h>

CMRC_DECLARE(rc);

#include <chrono>
#include <clocale>
#include <filesystem>
#include <thread>

#ifdef GetObject
#undef GetObject // httplib #includes <Windows.h> which pollutes global namespace with its bullshit.
#endif // GetObject

// yt-dlp --abort-on-error --match-filters '!is_live & requested_subtitles' --skip-download --write-auto-subs --sub-format 'json3' --sub-lang='en' --download-archive 'archive.txt' --print-to-file 'youtube %(id)s' 'archive.txt' --write-info-json

int main(
	int argc
	, char * argv[]
) {
#ifdef _WIN32
	std::setlocale(LC_ALL, ".65001"); // aka UTF-8. This should (lol) allow us to use fopen (among other things) with UTF-8-encoded strings.
#endif // _WIN32

#ifdef NDEBUG
	flog::level() = config::log_level;
#endif // NDEBUG

	if(
		argc != 1+1
		|| !util::file_exists(argv[1])
	) [[unlikely]] {
		std::printf(
R"(Usage: a-log [path/to/config.json]

config.example.json:
	{
		{"name": "Maldavius Figtree", "path": "/path/to/subs", "icon": "/path/to/icon/image.{png,webp,jp(e)?g"}
		, {"name": "Icon Is Optional", "path": "/path/to/subs"}
	}

"subs" can (and probably should) be obtained by running the following command:
	yt-dlp --abort-on-error --match-filters '!is_live & requested_subtitles' --skip-download --write-auto-subs --sub-format 'json3' --sub-lang='en' --download-archive 'archive.txt' --print-to-file 'youtube %%(id)s' 'archive.txt' --write-info-json 'https://www.youtube.com/whatever'

	Most of the options can be omitted, except for "--sub-format 'json3'" and "--write-info-json".)"
		"\n");

		return EXIT_FAILURE;
	}

	struct _archive {
		class archive archive;
		std::string path;
		std::string icon;
		std::string name;
	};

	std::vector<_archive> archives;
	std::string cache_dir;
	std::string server_listen_ip{config::server_listen_ip};
	std::remove_cvref_t<decltype(config::server_listen_port)> server_listen_port{config::server_listen_port};

	{
		auto file{util::read<std::string>(argv[1])};
		rapidjson::Document document;

		if(
			file.empty()
			|| !document.ParseInsitu(file.data()).IsObject()
		) [[unlikely]] {
			flog::write(util::format("Invalid config ('%s').", argv[1]));

			return EXIT_FAILURE;
		}

		const auto _archives{document.FindMember("archives")};

		if(
			_archives == document.MemberEnd()
			|| !_archives->value.IsArray()
		) [[unlikely]] {
			flog::write("Invalid \"archives\".");

			return EXIT_FAILURE;
		}

		if(const auto _cache_dir{document.FindMember("cache_dir")}; _cache_dir != document.MemberEnd()) {
			if(_cache_dir->value.IsString()) [[likely]] {
				cache_dir = std::string{_cache_dir->value.GetString(), _cache_dir->value.GetStringLength()};
			} else [[unlikely]] {
				flog::write("\"cache_dir\" is not a string.");

				return EXIT_FAILURE;
			}
		} else {
			flog::write("\"cache_dir\" not specified.");

			return EXIT_FAILURE;
		}

		for(const auto & i: _archives->value.GetArray()) {
			const auto
				name{i.FindMember("name")}
				, path{i.FindMember("path")}
				, icon{i.FindMember("icon")}
			;

			if(
				name == i.MemberEnd()
				|| path == i.MemberEnd()
				/*|| icon == i.MemberEnd()*/
			) [[unlikely]] {
				flog::write("Invalid archive definition.", flog::Level::warning); // TODO: Actually print the offending JSON?

				continue;
			}

			const std::string
				_name{name->value.GetString(), name->value.GetStringLength()}
				, _path{path->value.GetString(), path->value.GetStringLength()}
			;

			if(std::find_if(
				archives.begin()
				, archives.end()
				, [&_name](const auto & x) {
					return x.name == _name;
				}
			) != archives.end()) [[unlikely]] {
				flog::write(util::format("Duplicate archive name ('%s').", _name.c_str()));

				return EXIT_FAILURE;
			} else if(std::find_if(
				archives.begin()
				, archives.end()
				, [&, __path{std::filesystem::path{_path}}](const auto & x) {
					return std::filesystem::equivalent(__path, util::to_char8_t(x.path));
				}
			) != archives.end()) [[unlikely]] {
				flog::write(util::format("Duplicate archive path ('%s').", _path.c_str()));

				return EXIT_FAILURE;
			}

			auto _icon{[&i, &icon] {
				if(icon == i.MemberEnd()) [[unlikely]] {
					return std::string{};
				}

				auto extension{util::extension(std::string_view{icon->value.GetString(), icon->value.GetStringLength()})};

				std::transform(extension.begin(), extension.end(), extension.begin(), [](int c) {return std::tolower(c);}); // This is fine. No, really, if there are any non-ASCII extensions around, we certainly don't support them anyway.

				std::string type;

				if(extension == ".png") {
					type = "png";
				} else if(extension == ".webp") {
					type = "webp";
				} else if(
					extension == ".jpg"
					|| extension == ".jpeg"
				) {
					type = "jpeg";
				} else [[unlikely]] {
					flog::write(util::format("Unknown file type '%s'.", icon->value.GetString()));

					return std::string{};
				}

				const auto data{util::read<std::vector<std::byte> >(std::string{icon->value.GetString(), icon->value.GetStringLength()})};

				if(data.empty()) [[unlikely]] {
					flog::write(util::format("Unable to read '%s'.", icon->value.GetString()));

					return std::string{};
				}

				return
					std::string{"data:image/"}+type+";base64,"
					+util::base64_encode(data.data(), data.size())
				;
			}()};

			if(_icon.empty()) [[unlikely]] {
				assert(cmrc::rc::get_filesystem().exists("www/default_icon.webp"));

				const auto file{cmrc::rc::get_filesystem().open("www/default_icon.webp")};

				_icon = "data:image/webp;base64,"+util::base64_encode(static_cast<const char *>(file.cbegin()), file.size());
			}

			archives.emplace_back(_archive{
				.archive = archive{cache_dir+util::path_separator()+_name+util::path_separator()+"archive.json"}
				, .path = _path
				, .icon = std::move(_icon)
				, .name = _name
			});
		}

		if(const auto _server_listen_ip{document.FindMember("server_listen_ip")}; _server_listen_ip != document.MemberEnd()) {
			if(_server_listen_ip->value.IsString()) {
				server_listen_ip = std::string{_server_listen_ip->value.GetString(), _server_listen_ip->value.GetStringLength()};
			} else {
				flog::write("\"server_listen_ip\" is not a string.");

				return EXIT_FAILURE;
			}
		}

		if(const auto _server_listen_port{document.FindMember("server_listen_port")}; _server_listen_port != document.MemberEnd()) {
			if(_server_listen_port->value.IsNumber()) {
				server_listen_port = _server_listen_port->value.GetInt();
			} else {
				flog::write("\"server_listen_port\" is not a number.");

				return EXIT_FAILURE;
			}
		}
	}

#if 1
	for(const auto & archive: archives) {
		flog::write(
			util::format("archive: {name: '%s', path: '%s'}.size = %zu", archive.name.c_str(), archive.path.c_str(), archive.archive.size())
			, flog::Level::info
		);
	}
#endif

	for(auto & archive: archives) {
		const auto archive_path{cache_dir+util::path_separator()+archive.name};

		if(std::error_code error_code; !std::filesystem::is_directory(util::to_char8_t(archive_path), error_code) || error_code) {
			if(!std::filesystem::create_directories(util::to_char8_t(archive_path), error_code) || error_code) [[unlikely]] {
				flog::write(util::format("!create_directories('%s').", archive_path.c_str()));

				return EXIT_FAILURE;
			}
		}

		std::vector<std::string>
			subs
			, infos
		;

		for(std::error_code error_code; auto && i: std::filesystem::/*recursive_*/directory_iterator(archive.path)) {
			if(!i.is_regular_file(error_code) || error_code) {
				continue;
			}

			const auto path{util::from_u8string(i.path().u8string())};
			const auto filename{util::from_u8string(i.path().filename().u8string())};

			if(filename.ends_with(".json3")) {
				if(std::find_if(
					archive.archive.begin()
					, archive.archive.end()
					, [&path](const auto & x) {return x.subs == path;}
				) == archive.archive.end()) {
					subs.emplace_back(path);
				} else {
					flog::write(util::format("Duplicate subs '%s'.", filename.c_str()), flog::Level::debug);
				}
			} else if(filename.ends_with(".info.json")) {
				if(std::find_if(
					archive.archive.begin()
					, archive.archive.end()
					, [&path](const auto & x) {return x.info == path;}
				) == archive.archive.end()) {
					infos.emplace_back(path);
				} else {
					flog::write(util::format("Duplicate info '%s'.", filename.c_str()), flog::Level::debug);
				}
			} else {
				flog::write(util::format("Unknown file type '%s'.", filename.c_str()), flog::Level::debug);
			}
		}

		flog::write(util::format("subs.size = %zu.", subs.size()), flog::Level::debug);
		flog::write(util::format("infos.size = %zu.", infos.size()), flog::Level::debug);

		struct queue {
			std::ptrdiff_t info;
			std::ptrdiff_t subs;
		};

		std::vector<queue> _queue;

		_queue.reserve(std::max(infos.size(), subs.size()));
		for(const auto & i: infos) {
			const auto j{std::find_if(
				subs.begin()
				, subs.end()
				, [_i{i.substr(0, i.size()-util::strlen(".info.json"))}](const auto & x) {
					return x.find(_i) != x.npos; // FIXME: string(), handle language.
				}
			)};

			if(j == subs.end()) {
				flog::write(util::format("Unable to find subs matching '%s'.", i.c_str()), flog::Level::warning);

				continue;
			}

			_queue.emplace_back(queue{
				.info = &i-infos.data()
				, .subs = std::distance(subs.begin(), j)
			});
		}

		std::mutex mutex;
		std::vector<std::thread> threads;
		const std::size_t threads_size{std::thread::hardware_concurrency()};

		threads.reserve(threads_size);
		for(
			std::size_t i{0}, size{(_queue.size()+(threads_size-1))/threads_size}
			; i < threads_size
			; ++i
		) {
			const auto
				begin{i*size}
				, end{std::min((i+1)*size, _queue.size())}
			;

			threads.emplace_back([&, begin, end] {
				for(std::size_t i{begin}; i < end; ++i) {
					auto info{util::read<std::string>(infos[_queue[i].info])};
					rapidjson::Document _info;
					archive::source source;

					_info.ParseInsitu(info.data());

					if(
						!_info.IsObject()
						|| !source.load(_info.GetObject())
					) {
						flog::write(util::format("Unable to parse '%s'.", infos[_queue[i].info].c_str()), flog::Level::warning);

						continue;
					}

					flog::write(util::format("Generating text/timestamps for '%s'...", subs[_queue[i].subs].c_str()), flog::Level::info);

					if(sub::json3(&source.text.data, &source.timestamps.data, subs[_queue[i].subs])) {
						source.info = std::move(infos[_queue[i].info]);
						source.subs = std::move(subs[_queue[i].subs]);
						source.text.path = archive_path+util::path_separator()+(source.id+".text");
						source.timestamps.path = archive_path+util::path_separator()+(source.id+".timestamps");

						util::write(source.text.path, source.text.data);
						util::write(source.timestamps.path, source.timestamps.data);

						std::lock_guard<std::mutex> lock_guard(mutex);

						archive.archive.append(std::move(source));
					} else {
						flog::write(util::format("Unable to generate text/timestamps for '%s'.", subs[_queue[i].subs].c_str()), flog::Level::warning);
					}
				}
			});
		}

		for(auto & i: threads) {
			i.join();
		}

		if(!_queue.empty()) {
			util::file file{(archive_path+util::path_separator()+"archive.json").c_str(), "w"};

			if(!file) {
				flog::write(util::format("Unable to open '%s'.", (archive_path+util::path_separator()+"archive.json").c_str()));

				return EXIT_FAILURE;
			}

			archive.archive.store(static_cast<std::FILE *>(file));
		}
	}

	httplib::Server server;

#ifndef NDEBUG
	server.set_mount_point("/", "./www"); // TODO: Remove this, probably. But just in case I forget, what this does is it allows the server to read contents of www/ "directly".
#endif // !NDEBUG

	server.Get(".*", [_rc{cmrc::rc::get_filesystem()}](const httplib::Request & request, httplib::Response & response) {
		flog::write(util::format("(%s:%i) GET('%s').", request.remote_addr.c_str(), request.remote_port, request.path.c_str()), flog::Level::info);

		const auto path{"www"+(request.path == "/" ? "/index.html" : request.path)};

		if(!_rc.exists(path)) {
			flog::write(util::format("Unable to open '%s'.", path.c_str()));

			return;
		}

		const auto file{_rc.open(path)};
		std::string type; // We're only interested in contents of ./www so there's (probably) no need for a dedicated MIME lib. TODO: Since we know *exactly* what's what, there are better ways to do this. Well, at least there's no need to tolower the path.

		if(path.ends_with(".css")) {
			type = "text/css";
		} else if(path.ends_with(".html")) {
			type = "text/html";
		} else if(path.ends_with(".js")) {
			type = "text/javascript";
		} else if(path.ends_with(".woff2")) {
			type = "font/woff2";
		} else {
			flog::write(util::format("Unknown file type '%s'.", path.c_str()));

			return;
		}

		response.set_header("Cache-Control", "immutable,max-age="+std::to_string(config::server_max_age)+",public");
		response.set_content(static_cast<const char *>(file.cbegin()), file.size(), type);
	});
	server.Post(".*", [&](const httplib::Request & request, httplib::Response & response) { // FIXME: This is barely readable now :'(.
#ifndef NDEBUG
		if(request.body.find("server.stop()") != request.body.npos) {
			server.stop();

			return;
		}
#endif // !NDEBUG

		flog::write(
			util::format("(%s:%i) POST('%s', '%s').", request.remote_addr.c_str(), request.remote_port, request.path.c_str(), request.body.c_str())
			, flog::Level::info
		);

		if(request.body == "get_archives") {
			/*
			[
				{
					"name": String
					, "icon" : String
				}
			]
			*/

			std::string json{'['};

			for(char separator{' '}; const auto & i: archives) {
				util::strcat(
					&json
					, separator
					, "{\"name\":\""
					, util::json_escape(i.name)
					, "\",\"icon\":\""
					, i.icon
					, "\"}"
				);

				separator = ',';
			}
			json += ']';

			response.set_content(json, "application/json");

			return;
		}

		rapidjson::Document document;

		document.Parse(request.body.data(), request.body.size());

		if(!document.IsObject()) [[unlikely]] {
			flog::write(util::format("(%s:%i) !parse('%s').", request.remote_addr.c_str(), request.remote_port, request.body.c_str()));

			return;
		}

		if(const auto get_archive{document.FindMember("get_archive")}; get_archive != document.MemberEnd()) {
			/*
			{
				"archive": [
					{
						"i": String   // id
						, "u": String // upload_date
						, "t": String // title
					}
				]
				, "version": Number
			}
			*/

			const auto archive{std::find_if(
				archives.begin()
				, archives.end()
				, [name{std::string_view{get_archive->value.GetString(), get_archive->value.GetStringLength()}}](const auto & x) {
					return x.name == name;
				}
			)};

			if(archive == archives.end()) {
				flog::write(util::format("(%s:%i) archive == archives.end().", request.remote_addr.c_str(), request.remote_port));

				return;
			}

			std::string json{"{\"archive\":"};

			for(char separator{'['}; const auto & i: archive->archive) {
				util::strcat(
					&json
					, separator
					, "{\"i\":\""
					, i.id
					, "\""
					, ",\"u\":"
					, std::to_string(i.upload_date)
					, ",\"t\":\""
					, util::json_escape(i.title) // escape()'ing the strings here isn't ideal but this avoids any "unintended consequences" (and doesn't *really* matter).
					, "\"}"
				);

				separator = ',';
			}
			util::strcat(
				&json
				, "],\"version\":"
				, std::to_string(archive->archive.size()) // TODO: Cache the entire JSON string and version (which should be something more reliable than "size").
				, '}'
			);

			response.set_content(json, "application/json");

			return;
		}

		const auto t{std::chrono::high_resolution_clock::now()};

		std::string substr;
		std::remove_cv_t<decltype(config::substr_size_max)> substr_size{0};

		if(
			!document.HasMember("archive")
			|| !document.HasMember("substr")
		) [[unlikely]] {
			flog::write(util::format("(%s:%i) Invalid request.", request.remote_addr.c_str(), request.remote_port));

			return;
		}

		auto archive{archives.end()};

		{
			const auto _archive{document.FindMember("archive")};

			archive = std::find_if(
				archives.begin()
				, archives.end()
				, [name{std::string_view{_archive->value.GetString(), _archive->value.GetStringLength()}}](const auto & x) {
					return x.name == name;
				}
			);

			if(archive == archives.end()) [[unlikely]] {
				flog::write(util::format("(%s:%i) Unable to find archive '%s'.", request.remote_addr.c_str(), request.remote_port, _archive->value.GetString()));

				return;
			}
		}

		{
			const auto _substr{document.FindMember("substr")};

			substr = std::string{_substr->value.GetString(), _substr->value.GetStringLength()};

			if constexpr(config::min_search_size > 0) {
				if(utf8::unchecked::distance(substr.begin(), substr.end()) < config::min_search_size) [[unlikely]] {
					flog::write(util::format("(%s:%i) Search term '%s' is too short (LOL).", request.remote_addr.c_str(), request.remote_port, substr.c_str()));

					response.set_content("{}", "application/json");

					return;
				}
			}

			const auto _substr_size{document.FindMember("substr_size")};

			if(_substr_size != document.MemberEnd()) {
				substr_size = _substr_size->value.GetInt();
			}
		}

		substr_size = std::clamp(
			substr_size
			, std::max(static_cast<std::remove_cvref_t<decltype(config::substr_size_min)> >(substr.size()), config::substr_size_min)
			, config::substr_size_max
		);

		std::size_t count{0};
		std::vector<std::size_t> counts(archive->archive.size(), 0);
		std::string json{"{\"search\":["};
		char separator{' '}; // Has to be a space in case we get 0 results.

		struct page {
			std::ptrdiff_t archive;
			std::size_t begin;
			std::size_t end;
		};

		std::vector<std::size_t> archive_pages(archive->archive.size(), 0);
		std::vector<std::vector<page> > pages{1};
		std::size_t page_length{0};
		std::size_t result_index{0};

		archive->archive.find(
			substr
#ifdef USE_REGEX
			, [&](
#else // !USE_REGEX
			, [&, result_length{utf8::unchecked::distance(substr.begin(), substr.end())}](
#endif // USE_REGEX
				const std::string_view text
				, const std::size_t result_offset
				, [[maybe_unused]] const std::size_t result_size
				, const config::timestamp_type timestamp
				, const archive::source & source
			) {
#ifdef USE_REGEX
				if(result_size > config::substr_size_max) { // FIXME: Using *_size is incorrect but saves cycles.
					return;
				}

				const auto result_length{utf8::unchecked::distance(text.begin()+result_offset, (text.begin()+result_offset)+result_size)};
#endif // USE_REGEX

				/*
				{
					"search": [
						{
							"s": String   // substr
							, "t": Number // timestamp
							, "i": Number // archive index (into an array obtained by POST(get_archive))
						}
					]
					, "archive": [ // Results count for each archive. Needed to scale the bars
						Number
					]
					, "version": Number
					, "pages": [ // Actual pages
						[ // Ranges of results grouped by archive
							{
								"begin": Number // "search" index
								, "end": Number // "search" index
							}
						]
					]
					, "archive_pages": [ // "pages" index. Used to switch to the correct page when clicking on the chart bar
						Number
					]
				}
				*/

				++count;
				++counts[&source-&(*archive->archive.begin())];

				{
					const auto _archive{&source-&(*archive->archive.begin())};

					if(pages.back().empty() || pages.back().back().archive != _archive) {
						if(!pages.back().empty()) {
							pages.back().back().end = result_index;
						}

						pages.back().emplace_back(page{
							.archive = _archive
							, .begin = result_index
							, .end = {}
						});
						archive_pages[_archive] = pages.size()-1;
					}

					++page_length;
					++result_index;

					if(page_length >= config::results_per_page) {
						pages.back().back().end = result_index;

						pages.resize(pages.size()+1);
						page_length = 0;
					}
				}

				util::strcat(&json, separator, "{\"s\":\"");

				{
					const auto prior{[&](auto & i, const auto begin, const std::size_t length) {
						std::size_t size{0};

						for(std::size_t _i{0}; i > begin && _i < length; ++size, ++_i) {
							for(--i; utf8::internal::is_trail(*i); --i) {
							}
						}

						return size;
					}};

					auto
						begin{text.data()+result_offset}
						, end{
#ifdef USE_REGEX
							std::min( // Needed in case we're using a regex and (offset+result_length >= text.size()).
								(text.data()+result_offset)+substr.size()
								, (text.data()+text.size())-1
							)
#else // !USE_REGEX
							(text.data()+result_offset)+substr.size()
#endif // USE_REGEX
						}
					;
					const auto left_length{prior(begin, text.data(), (substr_size-result_length)/2)};

					for(
						std::size_t i{0}
						; end < (text.data()+text.size()) && i < (substr_size-(left_length+result_length))
						; ++i
					) {
						utf8::unchecked::next(end);
					}

					json += std::string_view{begin, static_cast<std::size_t>(end-begin)};
				}

				util::strcat(
					&json
					, "\",\"t\":"
					, std::to_string(timestamp)
					, ",\"i\":"
					, std::to_string(&source-&(*archive->archive.begin()))
					, '}'
				);

				separator = ',';
			}
		);

		if(count == 0) {
			response.set_content("{}", "application/json");

			return;
		}
		util::strcat(
			&json
			, ']'

			, ','
			, "\"archive\":"
		);
		if(count > 0) {
			separator = '[';
			for(const auto & i: archive->archive) {
				util::strcat(
					&json
					, separator
					, std::to_string(counts[&i-&(*archive->archive.begin())])
				);

				separator = ',';
			}
		} else {
			json += '[';
		}
		util::strcat(
			&json
			, ']'

			, ','
			, "\"version\":"
			, std::to_string(archive->archive.size()) // TODO: Cache entire JSON string and version (which should be something more reliable than "size").
		);
		if(!pages.back().empty()) {
			pages.back().back().end = result_index;

			json += ",\"pages\":[";
			for(char separator(' '); const auto & i: pages) {
				util::strcat(&json, separator, '[');
				for(char _separator(' '); const auto & j: i) {
					util::strcat(
						&json
						, _separator
						, "{\"begin\":"
						, std::to_string(j.begin)
						, ",\"end\":"
						, std::to_string(j.end)
						, '}'
					);

					_separator = ',';
				}
				json += ']';

				separator = ',';
			}
			util::strcat(
				&json
				, ']'

				, ",\"archive_pages\":["
			);
			for(char separator(' '); const auto i: archive_pages) {
				util::strcat(&json, separator, std::to_string(i));

				separator = ',';
			}
			json += ']';
		}

		json += '}';

		response.set_content(json, "application/json");

		flog::write(util::format(
			"(%s:%i) %zu results in %.2fms (%.2fMiB)."
			, request.remote_addr.c_str()
			, request.remote_port
			, count
			, static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()-t).count())/double{1'000'000}
			, (static_cast<double>(json.size())/double{1024})/double{1024}
		), flog::Level::info);
	});

	flog::write(util::format("server.listen('%s', '%i').", server_listen_ip.c_str(), server_listen_port), flog::Level::info);

	return server.listen(server_listen_ip, server_listen_port) ? EXIT_SUCCESS : EXIT_FAILURE;
}
