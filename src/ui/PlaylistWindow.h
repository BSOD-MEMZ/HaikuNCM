#ifndef NCM_PLAYLIST_WINDOW_H
#define NCM_PLAYLIST_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include <vector>

#include "net/netease.h"

class BListView;

enum {
	kMessagePlayIndex = 'plix',
};

// 当前播放队列（就是这次搜索的结果），双击切歌。
class PlaylistWindow : public BWindow {
public:
	PlaylistWindow(BMessenger target);

	void Update(const std::vector<ncm::Song>& songs, int32 current);

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	BMessenger fTarget;
	BListView* fList = nullptr;
	int32 fCurrent = -1;
};

#endif
