#ifndef NCM_SONG_HEADER_H
#define NCM_SONG_HEADER_H

#include <View.h>

class BFont;

// Column titles drawn above the song table. Kept in a plain BView so the
// metrics stay in sync with SongItem.
class SongHeader : public BView {
public:
	SongHeader();
	~SongHeader() override;

	void Draw(BRect updateRect) override;
	void AttachedToWindow() override;

private:
	BFont* fFont = nullptr;
};

#endif
