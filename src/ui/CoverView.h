#ifndef NCM_COVERVIEW_H
#define NCM_COVERVIEW_H

#include <Bitmap.h>
#include <View.h>

// 等比缩放居中贴一张位图（封面/头像通用），重绘不丢
class CoverView : public BView {
public:
	CoverView();
	~CoverView() override;

	void SetBitmap(BBitmap* bitmap); // 接管所有权
	void SetPadding(float padding);

	void Draw(BRect updateRect) override;

private:
	BBitmap* fBitmap = nullptr;
	float fPadding = 8.0f;
};

#endif
