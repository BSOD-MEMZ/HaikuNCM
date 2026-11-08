#include "PlaylistWindow.h"

#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringItem.h>

PlaylistWindow::PlaylistWindow(BMessenger target)
	: BWindow(BRect(0, 0, 360, 460), "播放列表", B_TITLED_WINDOW,
		  B_AUTO_UPDATE_SIZE_LIMITS)
	, fTarget(target)
	, fCurrent(-1)
{
	fList = new BListView("playlist", B_SINGLE_SELECTION_LIST);
	fList->SetInvocationMessage(new BMessage(kMessagePlayIndex));
	fList->SetExplicitMinSize(BSize(320, 380));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(new BScrollView("scroll", fList, 0, false, true))
		.End();

	CenterOnScreen();
}

void PlaylistWindow::Update(const std::vector<ncm::Song>& songs,
	int32 current)
{
	const bool wasEmpty = fList->CountItems() == 0;

	fList->MakeEmpty();
	for (size_t i = 0; i < songs.size(); i++) {
		BString label;
		if (static_cast<int32>(i) == current)
			label << "â¶ "; // ▶
		label << (int32)(i + 1) << ". " << songs[i].name.c_str() << " - "
			  << songs[i].artist.c_str();
		fList->AddItem(new BStringItem(label.String()));
	}

	fCurrent = current;
	if (current >= 0 && current < fList->CountItems()) {
		fList->Select(current, false);
		fList->ScrollToSelection();
	} else if (wasEmpty) {
		fList->Select(0, false);
	}
}

void PlaylistWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMessagePlayIndex) {
		const int32 index = fList->CurrentSelection();
		if (index >= 0) {
			BMessage play(kMessagePlayIndex);
			play.AddInt32("index", index);
			fTarget.SendMessage(&play);
		}
		return;
	}

	BWindow::MessageReceived(message);
}

bool PlaylistWindow::QuitRequested()
{
	return true;
}
