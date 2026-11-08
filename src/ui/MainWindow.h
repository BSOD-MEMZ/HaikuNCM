#ifndef NCM_MAINWINDOW_H
#define NCM_MAINWINDOW_H

#include <Locker.h>
#include <String.h>
#include <Window.h>

#include <string>
#include <vector>

#include "net/netease.h"

class BButton;
class BListView;
class BMenu;
class BMenuItem;
class BMenuBar;
class BMessageRunner;
class BSlider;
class BStringView;
class BTabView;
class BTextControl;
class BView;
class BCardView;
class PlaylistWindow;
class NowPlayingBadge;
class NowPlayingView;
class SeekSlider;
class SongListView;
class TransportButton;

namespace ncm {
class Player;
}

enum {
	kMessageSearch = 'srch',
	kMessageSearchDone = 'sdon',
	kMessageCoversDone = 'cver',
	kMessagePlay = 'play',
	kMessagePlayNext = 'plnx',
	kMessageQueueReorder = 'qreo',
	kMessageQueueDelete = 'qdel',
	kMessagePlayDone = 'pldn',
	kMessageFocusSearch = 'fsrc',
	kMessageLogin = 'logn',
	kMessageLogout = 'lout',
	kMessageAbout = 'abot',
	kMessageSeek = 'seek',
	kMessagePrev = 'prev',
	kMessageNext = 'next',
	kMessageTick = 'tick',
	kMessageVolume = 'volm',
	kMessageMode = 'mode',
	kMessageLike = 'like',
	kMessagePlaylist = 'plst',
	kMessagePlayIndex = 'plix',
	kMessageGenericOpen = 'gnop',
	kMessageGenericDone = 'gdon',
	kMessagePlaylistClosed = 'plcl',
	kMessageShowLogin = 'shlg',
	kMessageToggleNowPlaying = 'nowp',
};

class MainWindow : public BWindow {
public:
	MainWindow();
	~MainWindow() override;

	void SearchFor(const char* keyword);
	void PlayIndex(int32 index);
	void SelectTab(int32 tab);
	void ShowNowPlaying();
	void ShowArtist(long long id, const char* name);
	void ShowAlbum(long long id, const char* name);

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void _BuildMenu();
	void _BuildContent();
	void _BuildTransport();
	void _StartSearch();
	void _StartPlay(int32 index);
	void _FillResults();
	void _ApplyCovers();
	void _UpdateTransport();
	void _PostIfAlive(BMessage* message);
	void _StartCoverDownload();
	void _TogglePlayback();
	void _CyclePlayMode();
	void _ToggleLike();
	void _OpenPlaylist();
	void _SyncPlaylist();
	void _SyncTab();
	void _SwitchToTab(int32 tab);
	void _FetchGeneric(int32 slot);
	void _FillGeneric(int32 slot);
	void _ClearGeneric();
	void _ToggleNowPlaying();
	void _ShowNowPlayingPage(bool show);
	void _StartNowPlayingThread();
	void _Alert(const BString& text);
	int32 _IndexOfSong(long long id);
	void _UpdateModeLabel();

	static int32 _SearchThread(void* data);
	static int32 _PlayThread(void* data);
	static int32 _CoverThread(void* data);
	static int32 _GenericSearchThread(void* data);
	static int32 _DetailThread(void* data);
	static int32 _NowPlayingThread(void* data);

	// 菜单
	BMenuBar* fMenuBar = nullptr;
	BMenuBar* fUserMenuBar = nullptr;
	BMenu* fUserMenu = nullptr;
	BMenuItem* fUserItem = nullptr;
	BMenuItem* fLogoutItem = nullptr;

	// 搜索 + 内容区
	BTextControl* fSearchField = nullptr;
	BButton* fSearchButton = nullptr;
	BTabView* fSearchTabs = nullptr;
	BCardView* fContentHost = nullptr;
	SongListView* fResults = nullptr;
	BListView* fGenericLists[5] = {nullptr, nullptr, nullptr, nullptr,
		nullptr};
	std::vector<ncm::GenericItem> fGenericItems[5];
	bool fGenericReady[5] = {false, false, false, false, false};
	bool fGenericLoading[5] = {false, false, false, false, false};
	int32 fTabIndex = 0;
	int32 fSearchType = 1;
	bool fSongReady = false;

	// 播放页（内容区切换，不再单开窗口）
	NowPlayingView* fNowView = nullptr;
	NowPlayingBadge* fNowBadge = nullptr;
	bool fShowingNow = false;

	// 底部播放底板
	BView* fTransport = nullptr;
	TransportButton* fPrevButton = nullptr;
	TransportButton* fPlayButton = nullptr;
	TransportButton* fNextButton = nullptr;
	SeekSlider* fSeek = nullptr;
	BStringView* fTimeLabel = nullptr;
	BSlider* fVolume = nullptr;
	TransportButton* fHeartButton = nullptr;
	BButton* fModeButton = nullptr;

	ncm::Player* fPlayer = nullptr;
	BMessageRunner* fTicker = nullptr;
	int32 fCurrentIndex = -1;
	int32 fPlayMode = 0; // 0 顺序 1 单曲 2 随机
	bool fLiked = false;
	int32 fPendingPlay = -1;
	bool fSeekDirty = false;
	int32 fSeekTarget = 0;

	BLocker fLock;
	std::vector<ncm::Song> fSongs;
	ncm::NeteaseApi* fApi = nullptr;
	BString fKeyword;

	PlaylistWindow* fPlaylistWindow = nullptr;

	volatile bool fQuitting = false;
	volatile bool fBusy = false;
};

#endif
