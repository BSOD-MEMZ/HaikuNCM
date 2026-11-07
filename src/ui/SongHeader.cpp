#include "SongHeader.h"

#include "ListMetrics.h"

#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SongHeader"

using namespace ncm::metrics;

SongHeader::SongHeader()
	: BView("song-header", B_WILL_DRAW)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	SetLowColor(Background());
	SetExplicitMinSize(BSize(B_SIZE_UNSET, HeaderHeight()));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, HeaderHeight()));
}

void SongHeader::AttachedToWindow()
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
	SetFont(be_plain_font);
}

void SongHeader::Draw(BRect)
{
	const float width = Bounds().Width();
	const float baseline = Bounds().Height() - 8.0f;

	SetHighColor(SecondaryText());
	MovePenTo(BPoint(CoverX(), baseline));
	DrawString(B_TRANSLATE("标题"));

	MovePenTo(BPoint(AlbumX(width), baseline));
	DrawString(B_TRANSLATE("专辑"));

	MovePenTo(BPoint(HeartX(width), baseline));
	DrawString(B_TRANSLATE("喜欢"));

	const char* duration = B_TRANSLATE("时长");
	MovePenTo(BPoint(DurationRight(width) - StringWidth(duration), baseline));
	DrawString(duration);
}
