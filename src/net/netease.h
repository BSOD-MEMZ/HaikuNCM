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
	// 登录：手机号 + 密码（密码在内部做 MD5），成功后保存 Cookie 供后续请求使用
	bool Login(const std::string& phone, const std::string& password,
		std::string* nickname, std::string* cookie, std::string* error);
	bool LikeSong(long long id, bool like, std::string* error);
	bool IsLoggedIn() const;
	void SetCookie(const std::string& cookie);
	std::string Cookie() const;

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
