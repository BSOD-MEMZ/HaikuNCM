#include "SongRow.h"

#include "MainWindow.h"

#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SongRow"

#include <Bitmap.h>
#include <ControlLook.h>
#include <Font.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <View.h>

#include <cmath>
#include <cstring>

// ---------------------------------------------------------------- SongField

SongField::SongField(const ncm::Song& song)
	: BStringField("")
	, fSong(song)
{
}

void SongField::SetCover(BBitmap* bitmap)
{
	fCover = bitmap;
}

// ----------------------------------------------------------------- SongRow

SongRow::SongRow(const ncm::Song& song)
	: BRow(ceilf(be_plain_font->Size() * 2.0f) + 24.0f)
{
	// 每列各持一份字段（BColumnListView 按列取字段，缺了就是空指针）
	for (int32 column = 0; column < 4; column++)
		SetField(new SongField(song), column);
}

void SongRow::SetDisplayIndex(int32 index)
{
	for (int32 column = 0; column < 4; column++)
		static_cast<SongField*>(const_cast<BField*>(GetField(column)))
			->SetDisplayIndex(index);
}

void SongRow::SetCover(BBitmap* bitmap)
{
	Field()->SetCover(bitmap);
}

// ------------------------------------------------------------ 通用绘制辅助

namespace {

// 行背景（含奇偶斑马纹）由 BColumnListView 原生绘制（tintedLine，1.04 tint），
// 各列只画内容，不要自己铺底色把它盖掉。

BString FitText(BView* parent, const BString& text, float maxWidth)
{
	BString result(text);
	parent->TruncateString(&result, B_TRUNCATE_END, maxWidth);
	return result;
}

} // namespace

// ---------------------------------------------------------- SongTitleColumn

SongTitleColumn::SongTitleColumn(const char* title, float width,
	float minWidth, float maxWidth)
	: BTitledColumn(title, width, minWidth, maxWidth, B_ALIGN_LEFT)
{
	SetWantsEvents(true);
}

void SongTitleColumn::DrawField(BField* _field, BRect rect, BView* parent)
{
	SongField* field = static_cast<SongField*>(_field);

	const float margin = 8.0f;
	const float coverSize = rect.Height() - 12.0f;
	const BRect coverRect(rect.left + margin,
		rect.top + (rect.Height() - coverSize) / 2.0f,
		rect.left + margin + coverSize,
		rect.top + (rect.Height() + coverSize) / 2.0f);

	BBitmap* cover = field->Cover();
	if (cover != nullptr && cover->IsValid()) {
		parent->SetDrawingMode(B_OP_ALPHA);
		parent->DrawBitmap(cover, cover->Bounds(), coverRect,
			B_FILTER_BITMAP_BILINEAR);
		parent->SetDrawingMode(B_OP_COPY);
	} else {
		parent->SetHighColor(tint_color(ui_color(B_LIST_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		parent->FillRect(coverRect);
	}

	const float textX = coverRect.right + margin;
	const float textWidth = rect.right - textX - margin;
	const float lineHeight = (rect.Height() - 8.0f) / 2.0f;
	font_height fh;
	parent->GetFontHeight(&fh);
	const float ascent = fh.ascent;

	// 标题（粗体）
	parent->SetFont(be_bold_font);
	parent->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
	BString title(field->Song().name.c_str());
	title = FitText(parent, title, textWidth);
	float y = rect.top + 4.0f + lineHeight / 2.0f + ascent / 2.0f
		- fh.descent / 2.0f;
	parent->MovePenTo(textX, y);
	parent->DrawString(title);

	// 歌手（次要色，VIP 标记）
	parent->SetFont(be_plain_font);
	parent->SetHighColor(tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR),
		B_LIGHTEN_1_TINT));
	BString subtitle(field->Song().artist.c_str());
	if (field->Song().fee == 1)
		subtitle << "  · VIP";
	subtitle = FitText(parent, subtitle, textWidth);
	y += lineHeight;
	parent->MovePenTo(textX, y);
	parent->DrawString(subtitle);
	parent->SetFont(be_plain_font);
}

int SongTitleColumn::CompareFields(BField* a, BField* b)
{
	return strcasecmp(static_cast<SongField*>(a)->Song().name.c_str(),
		static_cast<SongField*>(b)->Song().name.c_str());
}

// ---------------------------------------------------------- SongAlbumColumn

SongAlbumColumn::SongAlbumColumn(const char* title, float width,
	float minWidth, float maxWidth)
	: BTitledColumn(title, width, minWidth, maxWidth, B_ALIGN_LEFT)
{
	SetWantsEvents(true);
}

void SongAlbumColumn::DrawField(BField* _field, BRect rect, BView* parent)
{
	SongField* field = static_cast<SongField*>(_field);

	const float margin = 8.0f;
	parent->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
	BString album(field->Song().album.c_str());
	album = FitText(parent, album, rect.Width() - 2 * margin);

	font_height fh;
	parent->GetFontHeight(&fh);
	parent->MovePenTo(rect.left + margin,
		rect.top + (rect.Height() - fh.ascent - fh.descent) / 2.0f
			+ fh.ascent);
	parent->DrawString(album);
}

int SongAlbumColumn::CompareFields(BField* a, BField* b)
{
	return strcasecmp(static_cast<SongField*>(a)->Song().album.c_str(),
		static_cast<SongField*>(b)->Song().album.c_str());
}

// ---------------------------------------------------------- SongHeartColumn

SongHeartColumn::SongHeartColumn(const char* title, float width,
	float minWidth, float maxWidth)
	: BTitledColumn(title, width, minWidth, maxWidth, B_ALIGN_CENTER)
{
	SetWantsEvents(true);
}

void SongHeartColumn::DrawField(BField* _field, BRect rect, BView* parent)
{
	SongField* field = static_cast<SongField*>(_field);

	// 点赞状态需要登录接口，登录前统一空心灰
	parent->SetHighColor(tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR),
		B_LIGHTEN_1_TINT));
	font_height fh;
	parent->GetFontHeight(&fh);
	const BString heart("\xe2\x99\xa5");
	const float width = parent->StringWidth(heart.String());
	parent->MovePenTo(rect.left + (rect.Width() - width) / 2.0f,
		rect.top + (rect.Height() - fh.ascent - fh.descent) / 2.0f
			+ fh.ascent);
	parent->DrawString(heart);
}

int SongHeartColumn::CompareFields(BField*, BField*)
{
	return 0; // 登录接入前无排序意义
}

// ------------------------------------------------------- SongDurationColumn

SongDurationColumn::SongDurationColumn(const char* title, float width,
	float minWidth, float maxWidth)
	: BTitledColumn(title, width, minWidth, maxWidth, B_ALIGN_RIGHT)
{
	SetWantsEvents(true);
}

void SongDurationColumn::DrawField(BField* _field, BRect rect, BView* parent)
{
	SongField* field = static_cast<SongField*>(_field);

	const long long seconds = field->Song().durationMs / 1000;
	char text[32];
	snprintf(text, sizeof(text), "%lld:%02lld", seconds / 60, seconds % 60);

	parent->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
	font_height fh;
	parent->GetFontHeight(&fh);
	const float width = parent->StringWidth(text);
	parent->MovePenTo(rect.right - 8.0f - width,
		rect.top + (rect.Height() - fh.ascent - fh.descent) / 2.0f
			+ fh.ascent);
	parent->DrawString(text);
}

int SongDurationColumn::CompareFields(BField* a, BField* b)
{
	const long long d1 = static_cast<SongField*>(a)->Song().durationMs;
	const long long d2 = static_cast<SongField*>(b)->Song().durationMs;
	if (d1 > d2)
		return 1;
	if (d1 < d2)
		return -1;
	return 0;
}

// ------------------------------------------------------------- SongListView

SongListView::SongListView()
	: BColumnListView("songs", B_NAVIGABLE, B_FANCY_BORDER, true)
{
}

void SongListView::MouseDown(BPoint where)
{
	BColumnListView::MouseDown(where);

	uint32 buttons = 0;
	BPoint cursor = where;
	GetMouse(&cursor, &buttons, false);
	if (buttons == B_SECONDARY_MOUSE_BUTTON) {
		BRow* row = RowAt(where);
		if (row != nullptr) {
			DeselectAll();
			AddToSelection(row);
			_ShowContextMenu(where);
		}
	}
}

void SongListView::_ShowContextMenu(BPoint where)
{
	auto* row = static_cast<SongRow*>(CurrentSelection());
	if (row == nullptr)
		return;

	BPopUpMenu* menu = new BPopUpMenu("song", false, false);
	menu->AddItem(new BMenuItem(B_TRANSLATE("播放"),
		new BMessage(kMessagePlay)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("下一首播放"),
		new BMessage(kMessagePlayNext)));
	menu->SetTargetForItems(Window());

	BPoint screen(where);
	ConvertToScreen(&screen);
	menu->Go(screen, true, true, true);
}
