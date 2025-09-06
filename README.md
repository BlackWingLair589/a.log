# a.log
![2025-09-07-020654_1920x1080_scrot](https://github.com/user-attachments/assets/a03de5f3-1ed4-44cb-8a7b-955933c7fbf1)
~~Basically~~A shitpost. Allows you to (relatively) quickly search countless hours of your favorite streamer's YouTube VODs in order to find the most wholesome moments you might want to share with the Internet. Now with advanced ray traced lighting. It's not very efficient, but it gets the job done. Probably.

## Dependencies

- CMake
- [ICU](https://icu.unicode.org/)
- (Optional) [RE2](https://github.com/google/re2.git)
- Whatever is in the _third_party_ directory
- ???
- Profit

## Building

(Very optionally) before building, edit config.hpp (the defaults should work fine, unless YouTube raises the limit on VOD length to more than ≈18 hours), then:

```bash
git submodule init && cmake && make install
```
... or something? I dunno.

## Running

The following incantation _should_ download the subtitles of the target video/playlist to the current working directory. What's required for **a.log** to run is the pairs of \*.json3 subtitles and \*.info.json3's. These can probably be obtained some other way, **however** the **a.log**'s code is not particularly robust, so it's recommended to just run the command below:
```bash
yt-dlp --abort-on-error --match-filters '!is_live & requested_subtitles' --skip-download --write-auto-subs --sub-format 'json3' --sub-lang='en' --download-archive 'archive.txt' --print-to-file 'youtube %(id)s' 'archive.txt' --write-info-json 'https://www.youtube.com/whatever'
```

To run the server you'll need a config.json that would look something like this:
```json
{
    "archives": [
        {
            "name": "Maldavius Figtree"
            , "path": "/path/to/subs+infos-downloaded-by-yt-dlp"
            , "icon": "/path/to/a/jpeg-png-webp-icon-with-Maldavius's-face-on-it"
        }
    ]
    , "cache_dir": "/path/to/a/writable/directory/where/a.log/can/put/its/stuff"
    , "server_listen_ip": "127.0.0.1"
    , "server_listen_port": "31337"
}
```

Next, run it:
```bash
a-log /path/to/config.json
```

Finally, open a browser and go to [http://127.0.0.1:31337](http://127.0.0.1:31337) (or whatever port you've configured previously). And then call me an un-wholesome individual when it doesn't work :'(

## FAQuestionsNobodyActuallyAsked

#### _How does it work?_
> Poorly. But seriously, it's (un)surprisingly simple: input subs are "normalized" by removing most punctuation and tolower'ing the whole thing. At the same time we generate "addressable" timestamps (see config.hpp::timestamp_length). Then it's just a matter of performing a simple text search, and using resulting indices to fetch the timestamps. Most of the code is GUI, which is horrifying, but it is what it is™.

#### _What does clicking on the results count/timestamps do?_
> (Attempts to) copy a yt-dlp command that would download clip(s) around a particular/all timestamp(s). The format/offset/duration are hardcoded because it's a pain in the ass to make it configurable, and because I'm very lazy. The list of valid formats **is** available to client(s) though, so it's only a small matter of finishing what I started.

#### _Why client/server?_
> Because at some point I was silly enough to think that using a browser for GUI would be easier than using Qt or one of the IMGUI libraries (It wasn't).

#### _Does it **actually** use ray tracing?_
> No.

#### _Your code sucks?_
> Yes.

#### _What is a "Maldavius Figtree"?_
> The only known carrier of the **Second Puberty Gene**.
