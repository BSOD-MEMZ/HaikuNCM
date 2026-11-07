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

enum {
	kMessageSearch = 'srch',
	kMessageSearchDone = 'sdon',
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
	void _StartSearch();
	void _StartPlay(int32 index);
	void _FillResults();
	void _UpdateSelectionLabel();
	void _PostStatus(const BString& text);
	void _PostIfAlive(BMessage* message);

	static int32 _SearchThread(void* data);
	static int32 _PlayThread(void* data);

	BMenuBar* fMenuBar = nullptr;
	BTextControl* fSearchField = nullptr;
	BButton* fSearchButton = nullptr;
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
