#ifndef NCM_MAINWINDOW_H
#define NCM_MAINWINDOW_H

#include <Locker.h>
#include <String.h>
#include <Window.h>

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
class SongHeader;
class UserMenuItem;
class TransportButton;

namespace ncm {
class Player;
}

enum {
	kMessageSearch = 'srch',
	kMessageSearchDone = 'sdon',
	kMessageCoversDone = 'cver',
	kMessagePlay = 'play',
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
	void _UpdateSelectionLabel();
	void _UpdateTransport();
	void _PostIfAlive(BMessage* message);
	void _StartCoverDownload();
	void _TogglePlayback();
	void _CyclePlayMode();
	void _ToggleLike();
	void _OpenPlaylist();
	void _UpdateModeLabel();

	static int32 _SearchThread(void* data);
	static int32 _PlayThread(void* data);
	static int32 _CoverThread(void* data);

	BMenuBar* fMenuBar = nullptr;
	UserMenuItem* fUserItem = nullptr;

	BTextControl* fSearchField = nullptr;
	BButton* fSearchButton = nullptr;
	SongHeader* fHeader = nullptr;
	BListView* fResults = nullptr;

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

	volatile bool fQuitting = false;
	volatile bool fBusy = false;
};

#endif
