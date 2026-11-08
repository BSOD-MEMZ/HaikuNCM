#include "GenericList.h"

#include "net/cachepaths.h"

#include <Catalog.h>
#include <ColumnTypes.h>
#include <Font.h>
#include <TranslationUtils.h>


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "GenericList"

namespace {

// 一行内容 = 一个字段，行自己持有数据
class GenericField : public BField {
public:
	GenericField(GenericRow* row)
		: fRow(row)
	{
	}

	GenericRow* Row() const { return fRow; }

private:
	GenericRow* fRow;
};

class GenericColumn : public BTitledColumn {
public:
	GenericColumn(const char* title, float width, float min, float max)
		: BTitledColumn(title, width, min, max)
	{
	}

	void DrawField(BField* _field, BRect rect, BView* parent) override
	{
		auto* field = static_cast<GenericField*>(_field);
		const ncm::GenericItem& item = field->Row()->Item();

		// 封面/头像
		const float side = rect.Height() - 8.0f;
		const BRect cover(rect.left + 4.0f, rect.top + 4.0f,
			rect.left + 4.0f + side, rect.top + 4.0f + side);
		BBitmap* thumb = field->Row()->Thumb();
		if (thumb != nullptr && thumb->IsValid()) {
			parent->SetDrawingMode(B_OP_ALPHA);
			parent->DrawBitmap(thumb, thumb->Bounds(), cover,
				B_FILTER_BITMAP_BILINEAR);
			parent->SetDrawingMode(B_OP_COPY);
		} else {
			parent->SetHighColor(tint_color(
				ui_color(B_LIST_BACKGROUND_COLOR), B_DARKEN_1_TINT));
			parent->FillRect(cover);
		}

		const float textX = cover.right + 8.0f;
		const float width = rect.right - textX - 8.0f;
		font_height fh;
		parent->GetFontHeight(&fh);

		parent->SetFont(be_plain_font);
		parent->SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
		BString name(item.name.c_str());
		parent->TruncateString(&name, B_TRUNCATE_END, width);
		parent->MovePenTo(textX, rect.top + fh.ascent + 6.0f);
		parent->DrawString(name);

		if (!item.subtitle.empty()) {
			parent->SetHighColor(tint_color(ui_color(B_LIST_ITEM_TEXT_COLOR),
				B_LIGHTEN_1_TINT));
			BString subtitle(item.subtitle.c_str());
			parent->TruncateString(&subtitle, B_TRUNCATE_END, width);
			parent->MovePenTo(textX,
				rect.top + fh.ascent + fh.descent + 14.0f);
			parent->DrawString(subtitle);
		}
	}
};

} // namespace

GenericRow::GenericRow(const ncm::GenericItem& item)
	: BRow(56.0f) // beta6 的 BRow 没有 SetHeight/Update，只能在构造时定高
	, fItem(item)
{
	SetField(new GenericField(this), 0);
}

GenericRow::~GenericRow()
{
	delete fThumb;
}

void GenericRow::SetThumb(BBitmap* bitmap)
{
	delete fThumb;
	fThumb = bitmap;
	fThumbTried = true;
}

void GenericRow::EnsureThumb()
{
	if (fThumbTried)
		return;
	fThumbTried = true;

	const std::string path = ncm::CacheDir() + "/avatar_"
		+ std::to_string(fItem.id) + ".jpg";
	BBitmap* bitmap = BTranslationUtils::GetBitmapFile(path.c_str());
	if (bitmap != nullptr)
		fThumb = bitmap;
}

GenericListView::GenericListView()
	: BColumnListView("generic", B_NAVIGABLE, B_FANCY_BORDER, true)
{
	const float scale = be_plain_font->Size() / 12.0f;
	AddColumn(new GenericColumn(B_TRANSLATE("内容"), 400 * scale,
		200 * scale, 800 * scale), 0);
	SetSortingEnabled(false);
}

void GenericListView::Fill(const std::vector<ncm::GenericItem>& items)
{
	Clear();
	for (const ncm::GenericItem& item : items)
		AddRow(new GenericRow(item));
	RefreshThumbs();
}

void GenericListView::RefreshThumbs()
{
	const int32 count = CountRows(nullptr);
	for (int32 i = 0; i < count; i++) {
		auto* row = static_cast<GenericRow*>(RowAt(i, nullptr));
		if (row == nullptr)
			continue;
		row->EnsureThumb();
		InvalidateRow(row);
	}
}
