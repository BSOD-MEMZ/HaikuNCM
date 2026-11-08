#include "AvatarGrid.h"

#include "net/cachepaths.h"

#include <Catalog.h>
#include <ControlLook.h>
#include <Window.h>
#include <Font.h>
#include <TranslationUtils.h>

#include <cmath>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AvatarGrid"

AvatarGridView::AvatarGridView(uint32 clickWhat)
	: BView("avatar-grid", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE
		| B_FRAME_EVENTS)
	, fClickWhat(clickWhat)
{
	SetViewUIColor(B_LIST_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());
}

AvatarGridView::~AvatarGridView()
{
	for (BBitmap* bitmap : fAvatars)
		delete bitmap;
}

void AvatarGridView::SetItems(const std::vector<ncm::GenericItem>& items)
{
	fItems = items;
	RefreshAvatars();
}

void AvatarGridView::RefreshAvatars()
{
	for (BBitmap* bitmap : fAvatars)
		delete bitmap;
	fAvatars.clear();
	fAvatars.reserve(fItems.size());

	for (const ncm::GenericItem& item : fItems) {
		const std::string path = ncm::CacheDir() + "/avatar_"
			+ std::to_string(item.id) + ".jpg";
		BBitmap* bitmap = BTranslationUtils::GetBitmapFile(path.c_str());
		fAvatars.push_back(bitmap); // 可能是 nullptr，画的时候兜底
	}

	_Relayout();
	Invalidate();
}

void AvatarGridView::_Relayout()
{
	const float width = Bounds().Width();
	fColumns = static_cast<int32>(width / fCellWidth);
	if (fColumns < 1)
		fColumns = 1;

	const int32 count = static_cast<int32>(fItems.size());
	const int32 rows = (count + fColumns - 1) / fColumns;
	const float height = rows * fCellHeight + 16.0f;
	SetExplicitMinSize(BSize(0, height));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, height));
}

BRect AvatarGridView::CellFrame(int32 index) const
{
	const int32 row = index / fColumns;
	const int32 column = index % fColumns;
	const float left = column * fCellWidth;
	const float top = row * fCellHeight;
	return BRect(left, top, left + fCellWidth, top + fCellHeight);
}

int32 AvatarGridView::IndexAt(BPoint where) const
{
	if (fColumns <= 0)
		return -1;
	const int32 column = static_cast<int32>(where.x / fCellWidth);
	const int32 row = static_cast<int32>(where.y / fCellHeight);
	if (column < 0 || column >= fColumns || row < 0)
		return -1;
	const int32 index = row * fColumns + column;
	return index >= 0 && index < static_cast<int32>(fItems.size()) ? index : -1;
}

void AvatarGridView::Draw(BRect)
{
	FillRect(Bounds(), B_SOLID_LOW);

	font_height fh;
	GetFontHeight(&fh);

	for (int32 i = 0; i < static_cast<int32>(fItems.size()); i++) {
		const BRect cell = CellFrame(i);
		if (cell.bottom < Bounds().top || cell.top > Bounds().bottom)
			continue;

		const ncm::GenericItem& item = fItems[i];

		// 头像：正方形，尽量大
		const float side = std::min(cell.Width() - 24.0f, 120.0f);
		BRect avatarRect(cell.left + (cell.Width() - side) / 2.0f,
			cell.top + 12.0f, cell.left + (cell.Width() + side) / 2.0f,
			cell.top + 12.0f + side);

		BBitmap* avatar = i < static_cast<int32>(fAvatars.size())
			? fAvatars[i] : nullptr;
		if (avatar != nullptr && avatar->IsValid()) {
			SetDrawingMode(B_OP_ALPHA);
			DrawBitmap(avatar, avatar->Bounds(), avatarRect,
				B_FILTER_BITMAP_BILINEAR);
			SetDrawingMode(B_OP_COPY);
		} else {
			SetHighColor(tint_color(ViewColor(), B_DARKEN_2_TINT));
			FillRect(avatarRect);
		}

		// 名字（粗体）+ 简介（次要色），都居中
		float y = avatarRect.bottom + 10.0f;

		SetFont(be_bold_font);
		SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
		BString name(item.name.c_str());
		TruncateString(&name, B_TRUNCATE_END, cell.Width() - 12.0f);
		const float nameWidth = StringWidth(name.String());
		MovePenTo(cell.left + (cell.Width() - nameWidth) / 2.0f,
			y + fh.ascent);
		DrawString(name);
		y += fh.ascent + fh.descent + 4.0f;

		if (!item.subtitle.empty()) {
			SetFont(be_plain_font);
			SetHighColor(tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR),
				B_LIGHTEN_1_TINT));
			BString subtitle(item.subtitle.c_str());
			TruncateString(&subtitle, B_TRUNCATE_END, cell.Width() - 12.0f);
			const float subWidth = StringWidth(subtitle.String());
			MovePenTo(cell.left + (cell.Width() - subWidth) / 2.0f,
				y + fh.ascent);
			DrawString(subtitle);
		}
	}
	SetFont(be_plain_font);
}

void AvatarGridView::MouseDown(BPoint where)
{
	const int32 index = IndexAt(where);
	if (index < 0)
		return;

	const bigtime_t now = system_time();
	if (fClickWhat != 0 && index == fLastClickIndex
		&& now - fLastClickTime < 500000) {
		// 双击：把这一项的 id 发出去
		BMessage message(fClickWhat);
		message.AddInt64("id", fItems[index].id);
		if (Window() != nullptr)
			Window()->PostMessage(&message);
		fLastClickIndex = -1;
		return;
	}

	fLastClickIndex = index;
	fLastClickTime = now;
	Invalidate();
}

void AvatarGridView::FrameResized(float, float)
{
	_Relayout();
	Invalidate();
}
