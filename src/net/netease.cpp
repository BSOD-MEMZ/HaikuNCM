#include "netease.h"

#include "crypto.h"
#include "http.h"

#include <nlohmann/json.hpp>

namespace ncm {

namespace {

using json = nlohmann::json;

const char* kSearchUrl = "https://music.163.com/weapi/search/get";
const char* kSongUrlEndpoint =
	"https://interface3.music.163.com/eapi/song/enhance/player/url/v1";
const char* kSongUrlPath = "/api/song/enhance/player/url/v1";

const std::vector<std::string>& ApiHeaders()
{
	static const std::vector<std::string> headers = {
		"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
		"AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
		"Referer: https://music.163.com",
		"Cookie: os=pc; appver=8.9.70; channel=netease",
		"Content-Type: application/x-www-form-urlencoded",
	};
	return headers;
}

const std::vector<std::string>& DownloadHeaders()
{
	static const std::vector<std::string> headers = {
		"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
		"AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
		"Cookie: os=pc; appver=8.9.70; channel=netease",
	};
	return headers;
}

std::string PickString(const json& object, const char* first, const char* second)
{
	if (object.contains(first) && object[first].is_string())
		return object[first].get<std::string>();
	if (second != nullptr && object.contains(second) && object[second].is_string())
		return object[second].get<std::string>();
	return std::string();
}

} // namespace

bool NeteaseApi::Search(const std::string& keyword, int limit,
	std::vector<Song>* songs, std::string* error)
{
	json payload = {
		{"s", keyword},
		{"type", 1},
		{"limit", limit},
		{"offset", 0},
	};

	const WeapiForm form = Weapi(payload.dump());
	const std::string body = "params=" + UrlEncode(form.params)
		+ "&encSecKey=" + UrlEncode(form.encSecKey);

	const Response response = PostForm(kSearchUrl, body, ApiHeaders());
	if (!response.Ok()) {
		if (error != nullptr)
			*error = response.error.empty()
				? "HTTP " + std::to_string(response.status)
				: response.error;
		return false;
	}

	json document;
	try {
		document = json::parse(response.body);
	} catch (const std::exception& exception) {
		if (error != nullptr)
			*error = std::string("JSON parse failed: ") + exception.what();
		return false;
	}

	if (!document.contains("code") || document["code"].get<int>() != 200) {
		if (error != nullptr)
			*error = "API code " + document.value("code", -1);
		return false;
	}

	songs->clear();
	if (!document.contains("result") || !document["result"].contains("songs"))
		return true;

	for (const json& item : document["result"]["songs"]) {
		Song song;
		song.id = item.value("id", 0LL);
		song.name = PickString(item, "name", nullptr);
		song.fee = item.value("fee", -1);
		song.durationMs = item.contains("dt") ? item.value("dt", 0LL)
											  : item.value("duration", 0LL);

		const char* artistsKey = item.contains("ar") ? "ar" : "artists";
		if (item.contains(artistsKey) && item[artistsKey].is_array()
			&& !item[artistsKey].empty()) {
			song.artist = item[artistsKey][0].value("name", "");
		}

		const char* albumKey = item.contains("al") ? "al" : "album";
		if (item.contains(albumKey) && item[albumKey].is_object())
			song.album = item[albumKey].value("name", "");

		songs->push_back(song);
	}
	return true;
}

bool NeteaseApi::SongUrl(long long id, const std::string& level,
	AudioUrl* audio, std::string* error)
{
	json payload = {
		{"ids", "[" + std::to_string(id) + "]"},
		{"level", level},
		{"encodeType", "flac"},
	};

	const std::string params = EapiParams(kSongUrlPath, payload.dump());
	const Response response = PostForm(kSongUrlEndpoint,
		"params=" + UrlEncode(params), ApiHeaders());
	if (!response.Ok()) {
		if (error != nullptr)
			*error = response.error.empty()
				? "HTTP " + std::to_string(response.status)
				: response.error;
		return false;
	}

	json document;
	try {
		document = json::parse(response.body);
	} catch (const std::exception& exception) {
		if (error != nullptr)
			*error = std::string("JSON parse failed: ") + exception.what();
		return false;
	}

	if (!document.contains("data") || !document["data"].is_array()
		|| document["data"].empty()) {
		if (error != nullptr)
			*error = "no data (code " + std::to_string(document.value("code", -1)) + ")";
		return false;
	}

	const json& item = document["data"][0];
	if (!item.contains("url") || !item["url"].is_string()
		|| item["url"].get<std::string>().empty()) {
		if (error != nullptr)
			*error = "no url, code " + std::to_string(item.value("code", -1));
		return false;
	}

	audio->url = item["url"].get<std::string>();
	audio->type = item.value("type", "");
	audio->bitrate = item.value("br", 0);
	audio->size = item.value("size", 0LL);
	return true;
}

bool NeteaseApi::DownloadAudio(const AudioUrl& audio, const std::string& path,
	std::string* error)
{
	return DownloadToFile(audio.url, path, DownloadHeaders(), error);
}

} // namespace ncm
