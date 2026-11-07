#include "SongItem.h"

#include "ListMetrics.h"

#include <Bitmap.h>
#include <Font.h>
#include <View.h>

#include <cstdio>

using namespace ncm::metrics;

namespace {

BString Fit(BView* owner, const BString& text, float maxWidth)
{
	if (maxWidth <= 8.0f || text.Length() == 0)
		return BString();

	if (owner->StringWidth(text.String()) <= maxWidth)
		return text;

	BString result(text);
	while (result.CountChars() > 1) {
		result.TruncateChars(result.CountChars() - 1);
		BString candidate(result);
		candidate << "\xe2\x80\xa6";
		if (owner->StringWidth(candidate.String()) <= maxWidth)
			return candidate;
	}
	return BString("...");
}

} // namespace

SongItem::SongItem(const ncm::Song& song, int32 index)
	: BListItem()
	, fSong(song)
	, fIndex(index)
{
	SetHeight(RowHeight());
}

SongItem::~SongItem()
{
	delete fCover;
}

void SongItem::SetCover(BBitmap* bitmap)
{
	if (bitmap == fCover)
		return;
	delete fCover;
	fCover = bitmap;
}

void SongItem::SetLiked(bool liked)
{
	fLiked = liked;
}

void SongItem::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);
	SetHeight(RowHeight());
}

void SongItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	(void)complete; // 每行都整体重绘，用不到增量标志

	const float width = owner->Bounds().Width();
	const bool selected = IsSelected();

	owner->PushState();

	owner->SetHighColor(selected ? SelectionBackground() : Background());
	owner->FillRect(frame);

	const rgb_color primary = selected ? SelectedText() : Text();
	const rgb_color secondary = selected ? SelectedText() : SecondaryText();
	const float middle = frame.top + RowHeight() / 2.0f;

	owner->SetFont(be_plain_font);
	owner->SetHighColor(secondary);
	char indexText[16];
	snprintf(indexText, sizeof(indexText), "%02d", static_cast<int>(fIndex + 1));
	owner->MovePenTo(
		BPoint(IndexRight() - owner->StringWidth(indexText), middle + 4.0f));
	owner->DrawString(indexText);

	const float coverSize = CoverSize();
	const BRect coverRect(CoverX(), frame.top + (RowHeight() - coverSize) / 2.0f,
		CoverX() + coverSize, frame.top + (RowHeight() + coverSize) / 2.0f);
	if (fCover != nullptr && fCover->IsValid()) {
		owner->SetDrawingMode(B_OP_ALPHA);
		owner->DrawBitmap(fCover, coverRect);
		owner->SetDrawingMode(B_OP_COPY);
	} else {
		owner->SetHighColor(tint_color(Background(), B_DARKEN_1_TINT));
		owner->FillRect(coverRect);
		owner->SetHighColor(tint_color(Background(), B_DARKEN_2_TINT));
		owner->StrokeRect(coverRect);
	}

	const float textMax = AlbumX(width) - TextX() - 16.0f;

	owner->SetFont(be_bold_font);
	owner->SetHighColor(primary);
	const BString title = Fit(owner, fSong.name.c_str(), textMax);
	owner->MovePenTo(BPoint(TextX(), middle - 2.0f));
	owner->DrawString(title.String());

	owner->SetFont(be_plain_font);
	owner->SetHighColor(secondary);
	BString subtitle(fSong.artist.c_str());
	if (fSong.fee == 1)
		subtitle << "  \xc2\xb7  VIP";
	const BString subtitleFitted = Fit(owner, subtitle, textMax);
	owner->MovePenTo(BPoint(TextX(), middle + 12.0f));
	owner->DrawString(subtitleFitted.String());

	const float albumX = AlbumX(width);
	const BString album = Fit(owner, fSong.album.c_str(),
		HeartX(width) - albumX - 20.0f);
	owner->MovePenTo(BPoint(albumX, middle + 4.0f));
	owner->DrawString(album.String());

	owner->SetHighColor(fLiked ? Accent() : secondary);
	owner->MovePenTo(BPoint(HeartX(width), middle + 5.0f));
	owner->DrawString("\xe2\x99\xa5");

	const long long seconds = fSong.durationMs / 1000;
	char duration[32];
	snprintf(duration, sizeof(duration), "%lld:%02lld", seconds / 60,
		seconds % 60);
	owner->MovePenTo(
		BPoint(DurationRight(width) - owner->StringWidth(duration), middle + 4.0f));
	owner->DrawString(duration);

	owner->PopState();
}
