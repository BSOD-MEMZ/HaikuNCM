#ifndef NCM_AVATAR_GRID_H
#define NCM_AVATAR_GRID_H

#include <Bitmap.h>
#include <String.h>
#include <View.h>

#include <vector>

#include "net/netease.h"

// 歌手 / 用户搜索结果的头像网格：官方网页版就是一圈圆头像 + 名字 + 简介。
// 单击选中，双击发 clickWhat 消息（带 id）。
class AvatarGridView : public BView {
public:
	AvatarGridView(uint32 clickWhat);
	~AvatarGridView() override;

	void SetItems(const std::vector<ncm::GenericItem>& items);
	void RefreshAvatars(); // 从缓存目录把下好的头像读进来

	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	void FrameResized(float width, float height) override;

private:
	BRect CellFrame(int32 index) const;
	int32 IndexAt(BPoint where) const;
	void _Relayout();

	std::vector<ncm::GenericItem> fItems;
	std::vector<BBitmap*> fAvatars;
	uint32 fClickWhat;
	float fCellWidth = 170.0f;
	float fCellHeight = 200.0f;
	int32 fColumns = 1;
	int32 fLastClickIndex = -1;
	bigtime_t fLastClickTime = 0;
};

#endif
