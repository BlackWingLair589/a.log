const config_results_chart_colors = [
	"Crimson"
	, "GreenYellow"
	, "Gold"
	, "MediumSlateBlue"
];
const config_results_chart_rtx = 0.5; // Height of the "reflection" (relative to var(--results-chart-height)).

function hms(
	seconds
) {
	const
		h = (seconds/60)/60
		, m = (h % 1)*60
	;

	return String(Math.floor(h)).padStart(2, '0')+':'+String(Math.floor(m)).padStart(2, '0')+':'+String(Math.floor((m % 1)*60)).padStart(2, '0');
}

function strftime(
	time_since_epoch_s
) {
	let date = new Date(time_since_epoch_s*1000);

	return date.getDate()+' '+date.toLocaleString("default", {month: "long"})+' '+date.getFullYear();
}

function year(
	time_since_epoch_s
) {
	return new Date(time_since_epoch_s*1000).getFullYear();
}

function max_line_length(
) {
	let span = document.createElement("span");

	document.body.appendChild(span);

	span.innerHTML = 'z';
	span.style.font = getComputedStyle(document.querySelector("body"))["font"];
	span.style.height = "auto";
	span.style.position = "absolute";
	span.style.width = "auto";

	const width = span.clientWidth;

	document.body.removeChild(span);

	return window.innerWidth/width;
}

function post_json(
	request
	, callback
) {
	let r = new XMLHttpRequest;

	r.onreadystatechange = function() {
		if(
			this.readyState == 4
			&& this.status == 200
		) {
			callback(JSON.parse(this.responseText));
		}
	}

	r.open("POST", "", true);
	r.setRequestHeader("Content-Type", "application/json");
	r.send(request);
}

const count = document.getElementById("count");
const context_image = document.getElementById("context-image");
const context_menu = document.getElementById("context-menu");
const results_pages = document.getElementById("results-pages");
const results_chart_container = document.getElementById("results-chart-container");
const results_chart_background = document.getElementById("results-chart-background")
const results_chart = document.getElementById("results-chart");
const results = document.getElementById("results");
const _search = document.getElementById("_search");

let archive = {archive: [], version: 0};
let archives = [];
let context = "";
let _search_value = _search.value;
let page_current = -1;
let _json = [];

function yt_dlp_cmd(
	index
	, format
	, t_offset
	, t_length
	, yt_dlp_path = "yt-dlp"
) {
	const id = archive["archive"][
		_json["search"][index]["i"] // I regret my life choices.
	]["i"];
	const t = _json["search"][index]["t"];

	return (
		yt_dlp_path
		+" -f "
		+format
		+" '"
		+"https://youtu.be/"
		+id
		+"' --download-sections '*"
		+hms(t+t_offset)
		+'-'
		+hms((t+t_offset)+t_length)
		+"' -o '"
		+id
		+'-'
		+t
		+".%(ext)s'"
	);
}

function pages_set(
	index
) {
	if(page_current == index) {
		return;
	}

	if(_json["pages"].length > 1) {
		results_pages.children.item(Math.max(0, page_current)).style.filter = ""; // Because we need to handle the initial -1.
		results_pages.children.item(index).style.filter = "brightness(175%)";
	}

	page_current = index;

	results.style.display = "none"; // Supposedly this should reduce the amount of "layout reflows". Doesn't seem like it does, but whatever.
	results.innerHTML = "";

	_json["pages"][index].forEach(i => {
		const archive_index = _json["search"][i["begin"]]["i"]; // Why do I do this to myself?
		const video_id = archive["archive"][archive_index]["i"];

		{
			let tr = document.createElement("tr");
			let th = document.createElement("th"); // FIXME: The code below is my best attempt at making this shit look exactly how I want it to, and at this point I don't really give a fuck whether it's efficient or not. The "FIXME" is here because despite all my efforts I couldn't get it to stay on a single line in case the element "overflows". My best guess is that it has something to do with "colSpan" being set, but I've wasted enough time on this garbage already.

			th.colSpan = 2;

			{
				let upload_date = document.createElement("span");

				upload_date.className = "upload-date";
				upload_date.id = video_id;
				upload_date.innerHTML = strftime(archive["archive"][archive_index]["u"]);

				th.appendChild(upload_date);
			} {
				let uarr = document.createElement("a");

				uarr.className = "uarr";
				uarr.href = "#";
				uarr.innerHTML = "&uarr;";
				uarr.target = "_self";

				th.appendChild(uarr);
			} {
				let title = document.createElement("span");

				title.className = "title";
				title.innerHTML = archive["archive"][archive_index]["t"];

				th.appendChild(title);
			} {
				let id = document.createElement("span");

				id.className = "id";
				id.innerHTML = archive["archive"][archive_index]["i"];

				th.appendChild(id);
			}

			tr.appendChild(th);
			results.appendChild(tr);
		}

		for(let j = i["begin"]; j < i["end"]; ++j) {
			let substr = _json["search"][j]["s"];

			{
				// FIXME?: This entire block looks (and *is*) fucking horrible. There *has to be* a better way of doing this. This is also a(n even bigger) waste of resources in case we're not using regexs.

				let s = new Set;

				for(const i of substr.matchAll(_search_value)) {
					for(const j of i) {
						s.add(j);

						break; // FIXME: ??????
					}
				}

				for(const i of s) {
					substr = substr.replaceAll(i, "<mark>"+i+"</mark>");
				}
			}

			substr = substr.replaceAll(" ", "&nbsp;"); // This is extremely retarded, but unless we do this we (might) get (some) misaligned text. "white-space:pre" doesn't completely fix it either.

			let result = document.createElement("tr");

			{
				let td = document.createElement("td");

				if(window.isSecureContext) {
					let a = document.createElement("a");

					a.className = "timestamp";
					a.href = "#";
					a.innerHTML = hms(_json["search"][j]["t"]);
					a.onclick = function() {
						navigator.clipboard.writeText(yt_dlp_cmd(j, "134+140", -5, 15)); // TODO: Configurable args.

						return false;
					};
					a.target = "_self";

					td.appendChild(a);
				} else {
					let _td = document.createElement("td");

					_td.className = "timestamp";
					_td.innerHTML = hms(_json["search"][j]["t"]);

					td.appendChild(_td);
				}

				result.appendChild(td);
			} {
				let a = document.createElement("a");

				a.addEventListener("click", function(e) {
					_json["search"][j]["visited"] = true; // Has to be done to keep track of visited links through "page flips" (and in private mode).

					a.style.textDecoration = "line-through";
				});
				a.className = "result-a";
				a.href = "https://youtu.be/"+video_id+"?t="+_json["search"][j]["t"];
				a.innerHTML = substr;
				a.rel = "noreferrer";
				if(_json["search"][j]["visited"] == true) {
					a.style.textDecoration = "line-through";
				}

				let td = document.createElement("td");

				td.className = "result-td";

				td.appendChild(a);
				result.appendChild(td);
			}

			results.appendChild(result);
		}
	});

	results.style.display = "";
}

function clear_results(
) {
	count.innerHTML = "";
	results_pages.innerHTML = "";
	results_pages.style.display = "";
	results_chart_container.style.display = "";
	results_chart_background.innerHTML = "";
	results_chart.innerHTML = "";
	results.innerHTML = "";
}

function parse(
	json
) {
	_json = json;
	page_current = -1;

	clear_results();

	if(!Object.hasOwn(json, "search")) {
		return;
	} else if(archive["version"] != json["version"]) {
		post_json(
			JSON.stringify({get_archive: context})
			, function(response) {
				archive = response;

				parse(json);
			}
		);

		return;
	}

	if(_json["pages"].length > 1) {
		results_pages.style.display = "flex";

		const padding = String(1+_json["pages"].length).length;

		for(let i = 0; i < _json["pages"].length; ++i) {
			let page = document.createElement("a");

			page.innerHTML = String(i+1).padStart(padding, 0);
			page.href = "#";
			page.target = "_self";
			page.className = "result-a";
			page.style = "padding-left:0.25rem;padding-right:0.25rem;display:inline";
			page.onclick = function() {
				pages_set(i);

				return false;
			}

			results_pages.appendChild(page);
		}
	} else {
		results_pages.style.display = "";
	}

	if(json["search"].length > 0) {
		if(archive["archive"].length > 1) { // Because there's no point otherwise. That, and it turns into a fucking flashbang with *.length == 1.
			results_chart_container.style.display = "block";
		}

		const width = 100/archive["archive"].length;

		let max_count = 0;

		json["archive"].forEach(i => {
			max_count = Math.max(max_count, i);
		});

		json["archive"].forEach((i, index) => {
			const color = config_results_chart_colors[year(archive["archive"][index].u) % config_results_chart_colors.length];

			let bar_bg = document.createElement("div");
			let bar_fg = document.createElement("div");

			bar_bg.className = bar_fg.className = "bar";
			bar_fg.title =
				String(json["archive"][index])+" • "+strftime(archive["archive"][index]["u"])+" • "+archive["archive"][index]["i"]+'\n'
				+archive["archive"][index]["t"]
			;
			bar_bg.style = bar_fg.style = "float:right;position:relative;width:"+String(width)+"%;height:100%";

			results_chart_background.appendChild(bar_bg);
			results_chart.appendChild(bar_fg);

			if(i == 0) {
				return;
			}

			const height = i/max_count; // [0, 1]
			const brightness = (90-40)*height+40; // [40%, 90%]

			{
				let _bar = document.createElement("div");

				_bar.style =
					"position:absolute;bottom:0;width:100%;"
					+"height:"+String(height*100)+"%;"
					+"background:"+color+";"
					+"filter:brightness("+String(150)+"%) blur(var(--results-chart-background-blur))"
				;

				bar_bg.appendChild(_bar);
			} {
				let rtx = document.createElement("div");

				rtx.style =
					"position:absolute;bottom:0;width:100%;"
					+"height:"+String((height*config_results_chart_rtx)*100)+"%;"
					+"background:"+color+";"
					+"filter:brightness("+String(brightness)+"%) blur(0.05rem);"
					+"opacity:30%;"
					+"transform:translate(0, 100%)"
				;

				bar_fg.appendChild(rtx); // I'd like to keep this in bar_bg, however this has to be a bar_fg element if we want the mouseover rtx (without JS anyway).
			}

			{
				let _bar = document.createElement("div");

				_bar.style =
					"position:absolute;bottom:0;width:100%;"
					+"height:"+String(height*100)+"%;"
					+"background:"+color+";"
					+"filter:brightness("+String(brightness)+"%)"
				;

				bar_fg.appendChild(_bar);
			} {
				let _bar = document.createElement("a"); // This must(?) be separate in order to make the entire column clickable and not just the "bar" itself.

				_bar.href = "#"+archive["archive"][index]["i"];
				_bar.style = "position:relative;display:block;width:100%;height:100%";
				_bar.target = "_self";
				_bar.onclick = function() {
					pages_set(_json["archive_pages"][index]);

					return true; // TODO/FIXME: I'm assuming pages_set will (always) exec first which might or might not be true.
				};

				bar_fg.appendChild(_bar);
			}
		});
	}

	{
		const _count = json["search"].length;

		count.innerHTML = _count > 0 ? String(_count) : "";
	}

	pages_set(0);
}

document.getElementById("search").addEventListener("submit", (e) => {
	e.preventDefault();

	const __search_value = _search.value.toLowerCase(); // Server expects a lowercase string.

	if(_search_value != __search_value) {
		_search_value = __search_value;

		post_json(
			JSON.stringify({
				archive: context
				, substr: _search_value
				, substr_size: Math.ceil(max_line_length()-"00:00:00".length)
			})
			, parse
		);
	}

	_search.select();
});

function store_results(
) {
	if(!window.isSecureContext) {
		return;
	}

	let data = "";

	{
		let prev_id = -1;
		let prev_cmd = ""; // That's unfortunate but we probably don't want dups here.

		for(let i = 0; i < _json["search"].length; ++i) {
			const curr_id = _json["search"][i]["i"];
			const curr_cmd = yt_dlp_cmd(i, "134+140", -5, 15)+'\n'; // TODO: Configurable args.

			if(curr_id != prev_id) {
				prev_id = curr_id;

				data += "\n\n# "+archive["archive"][curr_id]["t"]+'\n';
			}

			if(curr_cmd != prev_cmd) {
				prev_cmd = curr_cmd;

				data += "# "+_json["search"][i]["s"]+'\n';
				data += "# https://youtu.be/"+archive["archive"][curr_id]["i"]+"?t="+_json["search"][i]["t"]+'\n';
				data += curr_cmd+'\n';
			}
		}
	}

	navigator.clipboard.writeText(data); // TODO: Don't fuck with user's clipboard, save to a file insead (if that's even reasonable without some "framework" bullshit).
}

function context_menu_toggle(
) {
	context_menu.classList.toggle("context-menu-show");
}

function context_menu_hide(
) {
	context_menu.classList.remove("context-menu-show");
}

function context_menu_set(
) {
	context_menu.innerHTML = "";

	for(let i = 0; i < archives.length; ++i) {
		if(archives[i]["name"] == context) {
			context_image.src = archives[i]["icon"];

			break;
		}
	}

	archives.forEach(i => {
		let tr = document.createElement("tr");

		{
			let td = document.createElement("td");
			let img = document.createElement("img");

			img.src = i["icon"];
			img.style = "height:1.5rem;margin-right:0.5rem";

			td.appendChild(img);
			tr.appendChild(td);
		} {
			let td = document.createElement("td");
			let a = document.createElement("a");

			a.className = "result-a";
			a.href = "#";
			a.innerHTML = i["name"];
			a.onclick = function() {
				{
					clear_results();

					_search.value = _search_value = "";

					_search.focus();
				}

				context = i["name"];
				context_image.src = i["icon"];

				return false;
			};
			a.style = "height:1.5rem;display:block;margin-right:0.5rem";
			a.target = "_self";

			td.appendChild(a);
			tr.appendChild(td);
		}

		context_menu.appendChild(tr);
	});
}

document.addEventListener("DOMContentLoaded", function() {
	post_json(
		"get_archives"
		, function(response) {
			archives = response;

			context = archives[0]["name"]; // FIXME: Yes, this will fail if archives.length == 0. Nobody cares though.
			context_menu_set();
		}
	);
});

window.onclick = function(event) {
  if(!event.target.matches("#context-image")) {
	  context_menu_hide();
  }
}
