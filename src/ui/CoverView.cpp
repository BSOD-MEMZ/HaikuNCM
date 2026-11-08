#include "CoverView.h"

#include <algorithm>

CoverView::CoverView()
	: BView("cover", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());
}

CoverView::~CoverView()
{
	delete fBitmap;
}

void CoverView::SetBitmap(BBitmap* bitmap)
{
	delete fBitmap;
	fBitmap = bitmap;
	Invalidate();
}

void CoverView::SetPadding(float padding)
{
	fPadding = padding;
}

void CoverView::Draw(BRect)
{
	// 底色由 SetViewColor 决定：普通页面是面板色，播放页设成透明
	if (fBitmap == nullptr || !fBitmap->IsValid())
		return;

	// 等比缩放居中
	const float box = std::min(Bounds().Width(), Bounds().Height());
	const float side = box - fPadding * 2.0f;
	const BSize size = fBitmap->Bounds().Size();
	float width = side;
	float height = side;
	if (size.width > 0.0f && size.height > 0.0f) {
		if (size.width > size.height)
			height = side * size.height / size.width;
		else
			width = side * size.width / size.height;
	}

	const BRect bounds(Bounds());
	const BPoint center(bounds.left + bounds.Width() / 2.0f,
		bounds.top + bounds.Height() / 2.0f);
	const BRect dest(center.x - width / 2.0f, center.y - height / 2.0f,
		center.x + width / 2.0f, center.y + height / 2.0f);

	SetDrawingMode(B_OP_ALPHA);
	DrawBitmap(fBitmap, fBitmap->Bounds(), dest, B_FILTER_BITMAP_BILINEAR);
	SetDrawingMode(B_OP_COPY);
}
