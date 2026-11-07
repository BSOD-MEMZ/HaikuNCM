#ifndef NCM_SONG_ITEM_H
#define NCM_SONG_ITEM_H

#include <ListItem.h>
#include <String.h>

#include "net/netease.h"

class BBitmap;
class BFont;

// One row of the song table: index, cover, title + subtitle, album, heart,
// duration. Rendered by hand so the layout matches the NetEase list closely.
class SongItem : public BListItem {
public:
	SongItem(const ncm::Song& song, int32 index);
	~SongItem() override;

	void DrawItem(BView* owner, BRect frame, bool complete) override;
	void Update(BView* owner, const BFont* font) override;

	void SetCover(BBitmap* bitmap);
	const ncm::Song& Song() const { return fSong; }

private:
	ncm::Song fSong;
	int32 fIndex = 0;
	BBitmap* fCover = nullptr;
};

#endif
