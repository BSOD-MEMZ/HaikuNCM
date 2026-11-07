#ifndef NCM_MAINWINDOW_H
#define NCM_MAINWINDOW_H

#include <Locker.h>
#include <String.h>
#include <Window.h>

#include <vector>

#include "net/netease.h"

class BButton;
class BListView;
class BMenuBar;
class BStringView;
class BTextControl;
class BView;
class SongHeader;

enum {
	kMessageSearch = 'srch',
	kMessageSearchDone = 'sdon',
	kMessageCoversDone = 'cver',
	kMessagePlay = 'play',
	kMessagePlayDone = 'pldn',
	kMessageStatus = 'stat',
};

class MainWindow : public BWindow {
public:
	MainWindow();
	~MainWindow() override;

	void SearchFor(const char* keyword);

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void _BuildSidebar();
	void _StartSearch();
	void _StartPlay(int32 index);
	void _FillResults();
	void _ApplyCovers();
	void _UpdateSelectionLabel();
	void _PostIfAlive(BMessage* message);
	void _StartCoverDownload();

	static int32 _SearchThread(void* data);
	static int32 _PlayThread(void* data);
	static int32 _CoverThread(void* data);

	BMenuBar* fMenuBar = nullptr;
	BView* fSidebar = nullptr;
	BTextControl* fSearchField = nullptr;
	BButton* fSearchButton = nullptr;
	SongHeader* fHeader = nullptr;
	BListView* fResults = nullptr;
	BStringView* fNowPlaying = nullptr;
	BButton* fPlayButton = nullptr;
	BStringView* fStatus = nullptr;

	BLocker fLock;
	std::vector<ncm::Song> fSongs;
	ncm::NeteaseApi* fApi = nullptr;
	BString fKeyword;

	volatile bool fQuitting = false;
	volatile bool fBusy = false;
};

#endif
