#include "ui/MainWindow.h"

#include <Application.h>

#include <cstdlib>
#include <cstring>

namespace {

class NCMApplication : public BApplication {
public:
	NCMApplication()
		: BApplication("application/x-vnd.HaikuNCM")
	{
	}

	void SetInitialSearch(const char* keyword)
	{
		fInitialSearch = keyword;
	}

	void SetInitialTab(int32 tab) { fInitialTab = tab; }
	void SetShowNowPlaying(bool show) { fShowNowPlaying = show; }
	void SetArtist(long long id) { fArtistId = id; }
	void SetAlbum(long long id) { fAlbumId = id; }

	void SetShowLogin(bool show)
	{
		fShowLogin = show;
	}

	void SetShowPlaylist(bool show)
	{
		fShowPlaylist = show;
	}

	void SetPlayAfterSearch(int32 index)
	{
		fPlayAfterSearch = index;
	}

	void ReadyToRun() override
	{
		auto* window = new MainWindow();
		window->Show();

		if (fInitialTab >= 0)
			window->SelectTab(fInitialTab);

		if (fInitialSearch != nullptr) {
			window->SearchFor(fInitialSearch);
			if (fPlayAfterSearch >= 0)
				window->PlayIndex(fPlayAfterSearch);
		}

		if (fShowNowPlaying)
			window->ShowNowPlaying();
		if (fArtistId > 0)
			window->ShowArtist(fArtistId, "");
		if (fAlbumId > 0)
			window->ShowAlbum(fAlbumId, "");

		if (fShowLogin)
			window->PostMessage('logn');
		if (fShowPlaylist)
			window->PostMessage('plst');
	}

private:
	const char* fInitialSearch = nullptr;
	int32 fPlayAfterSearch = -1;
	int32 fInitialTab = -1;
	long long fArtistId = -1;
	long long fAlbumId = -1;
	bool fShowNowPlaying = false;
	bool fShowLogin = false;
	bool fShowPlaylist = false;
};

} // namespace

int main(int argc, char** argv)
{
	NCMApplication application;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--search") == 0 && i + 1 < argc)
			application.SetInitialSearch(argv[++i]);
		else if (strcmp(argv[i], "--play") == 0 && i + 1 < argc)
			application.SetPlayAfterSearch(atoi(argv[++i]));
		else if (strcmp(argv[i], "--tab") == 0 && i + 1 < argc)
			application.SetInitialTab(atoi(argv[++i]));
		else if (strcmp(argv[i], "--now") == 0)
			application.SetShowNowPlaying(true);
		else if (strcmp(argv[i], "--artist") == 0 && i + 1 < argc)
			application.SetArtist(atoll(argv[++i]));
		else if (strcmp(argv[i], "--album") == 0 && i + 1 < argc)
			application.SetAlbum(atoll(argv[++i]));
		else if (strcmp(argv[i], "--playlist") == 0)
			application.SetShowPlaylist(true);
		else if (strcmp(argv[i], "--login") == 0)
			application.SetShowLogin(true);
	}

	application.Run();
	return 0;
}
