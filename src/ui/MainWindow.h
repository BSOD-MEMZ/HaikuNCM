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
class SeekSlider;
class BStringView;
class BTextControl;
class PlaylistWindow;
class NowPlayingBadge;
class NowPlayingWindow;
class SongListView;
class BTabView;
class SongHeader;
class BMenu;
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
	kMessageStatus = 'stat',
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
	kMessageSearchTab = 'stab',
	kMessageGenericOpen = 'gnop',
	kMessageGenericDone = 'gdon',
	kMessagePlaylistClosed = 'plcl',
	kMessageDetailReady = 'dtrd',
	kMessageUserMenu = 'usrr',
	kMessageShowLogin = 'shlg',
	kMessageDoLogout = 'dlgo',
};

class MainWindow : public BWindow {
public:
	MainWindow();
	~MainWindow() override;

	void SearchFor(const char* keyword);
	void PlayIndex(int32 index);

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void _BuildMenu();
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
	void _SwitchSearchCard();
	void _FillGeneric();
	void _OpenNowPlaying();
	void _StartNowPlayingThread();
	int32 _IndexOfSong(long long id);
	void _UpdateModeLabel();

	static int32 _SearchThread(void* data);
	static int32 _PlayThread(void* data);
	static int32 _CoverThread(void* data);
	static int32 _GenericSearchThread(void* data);
	static int32 _DetailThread(void* data);
	static int32 _NowPlayingThread(void* data);

	BView* fCards[7] = {nullptr, nullptr, nullptr, nullptr, nullptr,
		nullptr, nullptr};

	BMenuBar* fMenuBar = nullptr;
	BMenuBar* fUserMenuBar = nullptr;
	BMenu* fUserMenu = nullptr;
	BMenuItem* fUserItem = nullptr;
	BMenuItem* fLogoutItem = nullptr;

	BTextControl* fSearchField = nullptr;
	BButton* fSearchButton = nullptr;
	SongListView* fResults = nullptr;
	BTabView* fSearchTabs = nullptr;
	BView* fSongCard = nullptr;
	BListView* fGenericList = nullptr;
	std::vector<ncm::GenericItem> fGenericItems;
	int32 fSearchType = 1;
	std::string fCacheDir;

	NowPlayingBadge* fNowBadge = nullptr;
	NowPlayingWindow* fNowPlayingWindow = nullptr;

	TransportButton* fPrevButton = nullptr;
	TransportButton* fPlayButton = nullptr;
	TransportButton* fNextButton = nullptr;
	SeekSlider* fSeek = nullptr;
	BStringView* fTimeLabel = nullptr;
	BSlider* fVolume = nullptr;
	BStringView* fVolumeLabel = nullptr;
	TransportButton* fHeartButton = nullptr;
	BButton* fModeButton = nullptr;
	BStringView* fStatus = nullptr;

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
