#include "SongHeader.h"

#include "ListMetrics.h"

#include <Font.h>

using namespace ncm::metrics;

SongHeader::SongHeader()
	: BView("song-header", B_WILL_DRAW)
{
	SetViewColor(RowBackground());
	SetLowColor(RowBackground());
	SetExplicitMinSize(BSize(B_SIZE_UNSET, kHeaderHeight));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, kHeaderHeight));
}

SongHeader::~SongHeader()
{
	delete fFont;
}

void SongHeader::AttachedToWindow()
{
	fFont = new BFont(be_plain_font);
	fFont->SetSize(11.5f);
	SetFont(fFont);
}

void SongHeader::Draw(BRect)
{
	const float width = Bounds().Width();

	SetHighColor(SecondaryText());
	MovePenTo(BPoint(kCoverX, kHeaderHeight - 9.0f));
	DrawString("标题");

	MovePenTo(BPoint(AlbumX(width), kHeaderHeight - 9.0f));
	DrawString("专辑");

	MovePenTo(BPoint(HeartX(width), kHeaderHeight - 9.0f));
	DrawString("喜欢");

	const char* duration = "时长";
	MovePenTo(BPoint(DurationRight(width) - StringWidth(duration),
		kHeaderHeight - 9.0f));
	DrawString(duration);

	SetHighColor((rgb_color){ 232, 232, 232, 255 });
	StrokeLine(BPoint(0, Bounds().bottom), BPoint(width, Bounds().bottom));
}
