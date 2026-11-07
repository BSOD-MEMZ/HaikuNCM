#ifndef NCM_SONG_HEADER_H
#define NCM_SONG_HEADER_H

#include <View.h>

// 表头：标题 / 专辑 / 喜欢 / 时长，列坐标与 SongItem 共用 ListMetrics。
class SongHeader : public BView {
public:
	SongHeader();

	void Draw(BRect updateRect) override;
	void AttachedToWindow() override;
};

#endif
