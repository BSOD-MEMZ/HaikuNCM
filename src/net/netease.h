#ifndef NCM_NETEASE_H
#define NCM_NETEASE_H

#include <string>
#include <vector>

namespace ncm {

struct Song {
	long long id = 0;
	std::string name;
	std::string artist;
	std::string album;
	std::string coverUrl;
	long long durationMs = 0;
	int fee = -1; // 0: free, 1: VIP (30s trial when anonymous), 8: free low quality
};

struct AudioUrl {
	std::string url;
	std::string type;
	int bitrate = 0;
	long long size = 0;
};

class NeteaseApi {
public:
	// POST /weapi/search/get
	bool Search(const std::string& keyword, int limit, std::vector<Song>* songs,
		std::string* error);

	// POST interface3 /eapi/song/enhance/player/url/v1
	bool SongUrl(long long id, const std::string& level, AudioUrl* audio,
		std::string* error);

	bool DownloadAudio(const AudioUrl& audio, const std::string& path,
		std::string* error);

	bool DownloadCover(const std::string& url, const std::string& path,
		std::string* error);
};

} // namespace ncm

#endif
