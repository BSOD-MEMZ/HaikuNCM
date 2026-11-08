#include "PlaylistWindow.h"

#include "MainWindow.h"

#include <LayoutBuilder.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringItem.h>

#include <cmath>

#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PlaylistWindow"

// ---------------------------------------------------------- PlaylistListView

namespace {
const uint32 kMsgDragRow = 'pldr';
const uint32 kMsgDelete = 'pldl';
const uint32 kMsgMoveUp = 'plmu';
const uint32 kMsgMoveDown = 'plmd';
}

PlaylistListView::PlaylistListView()
	: BListView("queue", B_SINGLE_SELECTION_LIST)
{
	SetInvocationMessage(new BMessage(kMessagePlayIndex));
}

void PlaylistListView::MouseDown(BPoint where)
{
	if (Bounds().Contains(where)) {
		const int32 index = IndexOf(where);
		if (index >= 0) {
			uint32 buttons = 0;
			GetMouse(&where, &buttons, false);

			if (buttons == B_SECONDARY_MOUSE_BUTTON) {
				Select(index, false);
				_ShowContextMenu(where);
				return;
			}

			if (buttons == B_PRIMARY_MOUSE_BUTTON) {
				fDragArmed = true;
				fDragIndex = index;
				fPressPoint = where;
			}
		}
	}

	BListView::MouseDown(where);
}

void PlaylistListView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	if (fDragArmed) {
		const float dx = where.x - fPressPoint.x;
		const float dy = where.y - fPressPoint.y;
		if (sqrtf(dx * dx + dy * dy) > 8.0f) {
			fDragArmed = false;
			_StartDrag(where);
		}
	}

	BListView::MouseMoved(where, transit, dragMessage);
}

void PlaylistListView::_StartDrag(BPoint where)
{
	(void)where;

	BRect itemRect = ItemFrame(fDragIndex);
	itemRect.OffsetTo(B_ORIGIN);

	BMessage drag(kMsgDragRow);
	drag.AddInt32("from", fDragIndex);
	DragMessage(&drag, itemRect, this);
}

int32 PlaylistListView::_IndexAt(BPoint where) const
{
	const int32 index = IndexOf(where);
	return index;
}

void PlaylistListView::MessageReceived(BMessage* message)
{
	if (message->what == kMsgDragRow) {
		// 自拖自接：算出落点行，连同 from 一起交窗口层改队列
		BPoint dropPoint;
		int32 from = -1;
		if (message->FindPoint("be:drop_point", &dropPoint) != B_OK
			|| message->FindInt32("from", &from) != B_OK)
			return;
		ConvertFromScreen(&dropPoint);

		int32 to = IndexOf(dropPoint);
		if (to < 0)
			to = CountItems() - 1;
		message->AddInt32("to", to);
		Window()->PostMessage(message);
		return;
	}

	if (message->what == kMsgDelete || message->what == kMsgMoveUp
		|| message->what == kMsgMoveDown) {
		// 视图事件统一转发给窗口层（窗口持有 fSongs 才能映射成歌 id）
		Window()->PostMessage(message);
		return;
	}

	BListView::MessageReceived(message);
}

void PlaylistListView::KeyDown(const char* bytes, int32 numBytes)
{
	if (numBytes == 1 && bytes[0] == B_DELETE) {
		BMessage deleteMessage(kMsgDelete);
		MessageReceived(&deleteMessage);
		return;
	}

	BListView::KeyDown(bytes, numBytes);
}

void PlaylistListView::_ShowContextMenu(BPoint where)
{
	BPopUpMenu* menu = new BPopUpMenu("playlist", false, false);
	menu->AddItem(new BMenuItem(B_TRANSLATE("上移"), new BMessage(kMsgMoveUp)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("下移"),
		new BMessage(kMsgMoveDown)));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("从队列删除"),
		new BMessage(kMsgDelete)));
	menu->SetTargetForItems(this);

	BPoint screen(where);
	ConvertToScreen(&screen);
	menu->Go(screen, true, false, false);
}

// ------------------------------------------------------------ PlaylistWindow

PlaylistWindow::PlaylistWindow(BMessenger target)
	: BWindow(BRect(0, 0, 360, 460), "播放列表", B_TITLED_WINDOW,
		  B_AUTO_UPDATE_SIZE_LIMITS)
	, fTarget(target)
	, fCurrent(-1)
{
	fList = new PlaylistListView();
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
	fSongs = songs;

	fList->MakeEmpty();
	for (size_t i = 0; i < songs.size(); i++) {
		BString label;
		if (static_cast<int32>(i) == current)
			label << "\xe2\x96\xb6 "; // ▶
		label << (int32)(i + 1) << ". " << songs[i].name.c_str() << " - "
			  << songs[i].artist.c_str();
		fList->AddItem(new BStringItem(label.String()));
	}

	fCurrent = current;
	if (current >= 0 && current < fList->CountItems()) {
		fList->Select(current, false);
		fList->ScrollToSelection();
	}
}

void PlaylistWindow::_Rebuild()
{
	fList->MakeEmpty();
	for (size_t i = 0; i < fSongs.size(); i++) {
		BString label;
		if (static_cast<int32>(i) == fCurrent)
			label << "â¶ ";
		label << (int32)(i + 1) << ". " << fSongs[i].name.c_str() << " - "
			  << fSongs[i].artist.c_str();
		fList->AddItem(new BStringItem(label.String()));
	}

	if (fCurrent >= 0 && fCurrent < fList->CountItems()) {
		fList->Select(fCurrent, false);
		fList->ScrollToSelection();
	}
}

void PlaylistWindow::_SendOrder()
{
	BMessage reorder(kMessageQueueReorder);
	reorder.AddInt32("count", static_cast<int32>(fSongs.size()));
	for (const ncm::Song& song : fSongs)
		reorder.AddInt64("id", song.id);
	fTarget.SendMessage(&reorder);
}

void PlaylistWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMessagePlayIndex:
		{
			const int32 index = fList->CurrentSelection();
			if (index >= 0) {
				BMessage play(kMessagePlayIndex);
				play.AddInt32("index", index);
				fTarget.SendMessage(&play);
			}
			return;
		}

		case kMsgDragRow:
		{
			int32 from = -1;
			int32 to = -1;
			if (message->FindInt32("from", &from) != B_OK
				|| message->FindInt32("to", &to) != B_OK)
				return;
			// 播放中的行跟着走
			if (fCurrent == from)
				fCurrent = to;
			else if (from < fCurrent && to >= fCurrent)
				fCurrent--;
			else if (from > fCurrent && to <= fCurrent)
				fCurrent++;

			const ncm::Song song = fSongs[from];
			fSongs.erase(fSongs.begin() + from);
			fSongs.insert(fSongs.begin() + to, song);

			_Rebuild();
			_SendOrder();
			return;
		}

		case kMsgDelete:
		{
			const int32 index = fList->CurrentSelection();
			if (index < 0 || index >= static_cast<int32>(fSongs.size()))
				return;

			BMessage out(kMessageQueueDelete);
			out.AddInt32("index", index);
			fTarget.SendMessage(&out);

			fSongs.erase(fSongs.begin() + index);
			if (fCurrent == index)
				fCurrent = -1;
			else if (index < fCurrent)
				fCurrent--;

			_Rebuild();
			return;
		}

		case kMsgMoveUp:
		case kMsgMoveDown:
		{
			const int32 index = fList->CurrentSelection();
			const int32 target = message->what == kMsgMoveUp
				? index - 1 : index + 1;
			if (index < 0 || target < 0
				|| target >= static_cast<int32>(fSongs.size()))
				return;

			std::swap(fSongs[index], fSongs[target]);
			if (fCurrent == index)
				fCurrent = target;
			else if (fCurrent == target)
				fCurrent = index;

			_Rebuild();
			_SendOrder();
			return;
		}

		default:
			BWindow::MessageReceived(message);
			return;
	}
}

bool PlaylistWindow::QuitRequested()
{
	BMessage closed(kMessagePlaylistClosed);
	fTarget.SendMessage(&closed);
	return true;
}
