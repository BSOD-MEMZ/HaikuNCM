#ifndef NCM_SONG_ITEM_H
#define NCM_SONG_ITEM_H

#include <ListItem.h>

#include "net/netease.h"

class BBitmap;
class BFont;

// 一行歌曲：序号 / 封面 / 标题+歌手 / 专辑 / 喜欢 / 时长。
// 自绘是为了在保留 BListView 原生行为（选中、键盘、滚动）的前提下排成表。
class SongItem : public BListItem {
public:
	SongItem(const ncm::Song& song, int32 index);
	~SongItem() override;

	void DrawItem(BView* owner, BRect frame, bool complete) override;
	void Update(BView* owner, const BFont* font) override;

	void SetCover(BBitmap* bitmap);
	void SetLiked(bool liked);

	const ncm::Song& Song() const { return fSong; }

private:
	ncm::Song fSong;
	int32 fIndex = 0;
	BBitmap* fCover = nullptr;
	bool fLiked = false;
};

#endif
