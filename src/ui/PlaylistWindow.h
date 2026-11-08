#ifndef NCM_PLAYLIST_WINDOW_H
#define NCM_PLAYLIST_WINDOW_H

#include <ListView.h>
#include <Messenger.h>
#include <Window.h>

#include <vector>

#include "net/netease.h"

class BPopUpMenu;

// 播放队列列表：拖动排序（自拖自接）、右键删除/上移/下移、Del 键删除。
// 变更通过消息发回主窗口改 fSongs，本窗口只管视图。
class PlaylistListView : public BListView {
public:
	PlaylistListView();

	void MessageReceived(BMessage* message) override;
	void MouseDown(BPoint where) override;
	void MouseMoved(BPoint where, uint32 transit,
		const BMessage* dragMessage) override;
	void KeyDown(const char* bytes, int32 numBytes) override;

private:
	void _StartDrag(BPoint where);
	void _ShowContextMenu(BPoint where);
	int32 _IndexAt(BPoint where) const;

	bool fDragArmed = false;
	int32 fDragIndex = -1;
	BPoint fPressPoint;
};

// 播放列表窗口：显示当前队列，双击切歌。
class PlaylistWindow : public BWindow {
public:
	PlaylistWindow(BMessenger target);

	void Update(const std::vector<ncm::Song>& songs, int32 current);

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void _Rebuild();
	void _SendOrder();

	BMessenger fTarget;
	PlaylistListView* fList = nullptr;
	int32 fCurrent = -1;
	std::vector<ncm::Song> fSongs;
};

#endif
