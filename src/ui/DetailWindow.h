#ifndef NCM_DETAILWINDOW_H
#define NCM_DETAILWINDOW_H

#include <Messenger.h>
#include <Window.h>

#include <vector>

#include "net/netease.h"

class BListView;
class BStringView;
class CoverView;
class SongListView;

enum {
	kMessageDetailPlay = 'dtpl',   // 双击歌曲：source + index
	kMessageDetailReady = 'dtrd',  // 头部信息 + 歌曲都到了
	kMessageDetailAlbums = 'dtal', // 歌手页的专辑列表到了
	kMessageDetailAlbumOpen = 'dtao', // 在歌手页双击专辑
	kMessageDetailFail = 'dtfl',
	kMessageDetailCovers = 'dtcv', // 列表封面下完了
	kMessagePlaySongs = 'psgs', // 让主窗口拿这批歌当播放队列
};

// 歌手页 / 专辑页：新窗口，头和网易云网页版一致（头像/封面 + 名字 + 信息），
// 下面挂"歌曲 / 专辑"两个 tab，列表直接沿用主窗口的歌曲表外观。
class DetailWindow : public BWindow {
public:
	DetailWindow(BMessenger target, bool artist);

	void LoadArtist(long long id, const char* name);
	void LoadAlbum(long long id, const char* name);

	// 调用方需先 Lock() 本窗口
	const std::vector<ncm::Song>& Songs() const { return fSongs; }

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void _FillSongs();
	void _FillAlbums();

	static int32 _ArtistThread(void* data);
	static int32 _AlbumThread(void* data);

	BMessenger fTarget;
	bool fIsArtist;
	bool fLoaded = false;

	CoverView* fImage = nullptr;
	BStringView* fTitle = nullptr;
	BStringView* fMeta = nullptr;
	BStringView* fDesc = nullptr;

	SongListView* fSongsList = nullptr;
	BListView* fAlbumList = nullptr;

	std::vector<ncm::Song> fSongs;
	std::vector<ncm::AlbumInfo> fAlbums;

	struct Job {
		DetailWindow* window;
		long long id;
	};
};

#endif
