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

	void ReadyToRun() override
	{
		auto* window = new MainWindow();
		window->Show();

		if (fInitialSearch != nullptr)
			window->SearchFor(fInitialSearch);
	}

private:
	const char* fInitialSearch = nullptr;
};

} // namespace

int main(int argc, char** argv)
{
	NCMApplication application;
	if (argc > 2 && strcmp(argv[1], "--search") == 0)
		application.SetInitialSearch(argv[2]);

	application.Run();
	return 0;
}
