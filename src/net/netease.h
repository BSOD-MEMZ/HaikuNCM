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
	long long artistId = 0; // 点歌手名跳转用
	long long albumId = 0;  // 点专辑名跳转用
	int fee = -1; // 0: free, 1: VIP (30s trial when anonymous), 8: free low quality
};

struct GenericItem {
	long long id = 0;
	std::string name;
	std::string subtitle;
	std::string imageUrl; // 歌手头像 / 用户头像
};

struct ArtistInfo {
	long long id = 0;
	std::string name;
	std::string alias;
	std::string avatarUrl;
	std::string briefDesc;
	int albumCount = 0;
	int songCount = 0;
};

struct Comment {
	std::string nickname;
	std::string content;
	int likedCount = 0;
};

struct PlaylistInfo {
	long long id = 0;
	std::string name;
	int trackCount = 0;
};

struct AlbumInfo {
	long long id = 0;
	std::string name;
	std::string artist;
	std::string coverUrl;
	std::string publishTime;
	std::string description;
	int songCount = 0;
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

	// 聚合搜索：type 1 单曲 / 1000 歌单 / 10 专辑 / 100 歌手 / 1002 用户
	// / 1006 歌词 / 1009 声音
	bool SearchGeneric(int type, const std::string& keyword, int limit,
		std::vector<GenericItem>* items, std::string* error);

	bool FetchPlaylistSongs(long long id, std::vector<Song>* songs,
		std::string* error);
	// 歌单页：封面 + 名字 + 创建者 + 全部歌曲
	bool FetchPlaylistPage(long long id, std::string* name,
		std::string* coverUrl, std::string* creator,
		std::vector<Song>* songs, std::string* error);
	bool FetchAlbumSongs(long long id, std::vector<Song>* songs,
		std::string* error);
	bool FetchArtistSongs(long long id, std::vector<Song>* songs,
		std::string* error);
	bool FetchLyric(long long id, std::string* lrc, std::string* translation,
		std::string* romaji, std::string* error);
	bool FetchComments(long long songId, int limit, int offset,
		std::vector<Comment>* comments, std::string* error);

	// 登录态相关：账号信息 / 我喜欢的音乐 / 我的歌单
	bool FetchAccount(long long* userId, std::string* nickname,
		std::string* error);
	bool FetchLikedIds(long long userId, std::vector<long long>* ids,
		std::string* error);
	bool FetchUserPlaylists(long long userId,
		std::vector<PlaylistInfo>* playlists, std::string* error);

	// 歌手页 / 专辑页
	bool FetchArtist(long long id, ArtistInfo* info,
		std::vector<Song>* hotSongs, std::string* error);
	bool FetchArtistAlbums(long long id, std::vector<AlbumInfo>* albums,
		std::string* error);
	bool FetchAlbumDetail(long long id, AlbumInfo* info,
		std::vector<Song>* songs, std::string* error);

	// 短信验证码登录（网易云风控下比账密可靠）
	bool SendSmsCaptcha(const std::string& phone, std::string* error);
	bool LoginWithCaptcha(const std::string& phone, const std::string& captcha,
		std::string* nickname, std::string* cookie, std::string* error);

	// 扫码登录：取 key -> 画二维码 -> 轮询
	bool QrKey(std::string* key, std::string* error);
	// 返回 800 过期 / 801 等待 / 802 待确认 / 803 成功
	bool QrPoll(const std::string& key, int* code, std::string* nickname,
		std::string* cookie, std::string* error);
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
