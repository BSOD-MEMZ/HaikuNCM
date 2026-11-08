#include "ui/MainWindow.h"

#include <Application.h>

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

	void SetShowLogin(bool show)
	{
		fShowLogin = show;
	}

	void SetPlayAfterSearch(int32 index)
	{
		fPlayAfterSearch = index;
	}

	void ReadyToRun() override
	{
		auto* window = new MainWindow();
		window->Show();

		if (fInitialSearch != nullptr) {
			window->SearchFor(fInitialSearch);
			if (fPlayAfterSearch >= 0)
				window->PlayIndex(fPlayAfterSearch);
		}

		if (fShowLogin)
			window->PostMessage('logn');
	}

private:
	const char* fInitialSearch = nullptr;
	int32 fPlayAfterSearch = -1;
	bool fShowLogin = false;
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
		else if (strcmp(argv[i], "--login") == 0)
			application.SetShowLogin(true);
	}

	application.Run();
	return 0;
}
