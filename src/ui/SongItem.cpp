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
		candidate << "…";
		if (owner->StringWidth(candidate.String()) <= maxWidth)
			return candidate;
	}
	return BString("…");
}

} // namespace

SongItem::SongItem(const ncm::Song& song, int32 index)
	: BListItem()
	, fSong(song)
	, fIndex(index)
{
	SetHeight(kRowHeight);
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

void SongItem::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);
	SetHeight(kRowHeight);
}

void SongItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	const float width = owner->Bounds().Width();

	owner->PushState();

	if (IsSelected())
		owner->SetHighColor(SelectionColor());
	else
		owner->SetHighColor(RowBackground());
	owner->FillRect(frame);

	owner->SetHighColor((rgb_color){ 238, 238, 238, 255 });
	owner->StrokeLine(BPoint(frame.left, frame.bottom),
		BPoint(frame.right, frame.bottom));

	BFont font(be_plain_font);
	font.SetSize(12.0f);
	owner->SetFont(&font);
	owner->SetHighColor(SecondaryText());

	char indexText[8];
	snprintf(indexText, sizeof(indexText), "%02d", static_cast<int>(fIndex + 1));
	owner->MovePenTo(
		BPoint(kIndexRight - owner->StringWidth(indexText), frame.top + 24.0f));
	owner->DrawString(indexText);

	const BRect coverRect(kCoverX, frame.top + 9.0f, kCoverX + kCoverSize,
		frame.top + 9.0f + kCoverSize);
	if (fCover != nullptr && fCover->IsValid()) {
		owner->SetDrawingMode(B_OP_ALPHA);
		owner->DrawBitmap(fCover, coverRect);
		owner->SetDrawingMode(B_OP_COPY);
	} else {
		owner->SetHighColor((rgb_color){ 228, 228, 228, 255 });
		owner->FillRoundRect(coverRect, 4.0f, 4.0f);
	}

	const float textMax = AlbumX(width) - kTextX - 16.0f;

	font.SetSize(13.5f);
	font.SetFace(B_BOLD_FACE);
	owner->SetFont(&font);
	owner->SetHighColor(PrimaryText());
	const BString title = Fit(owner, fSong.name.c_str(), textMax);
	owner->MovePenTo(BPoint(kTextX, frame.top + 25.0f));
	owner->DrawString(title.String());

	font.SetSize(11.5f);
	font.SetFace(B_REGULAR_FACE);
	owner->SetFont(&font);
	owner->SetHighColor(SecondaryText());

	BString subtitle(fSong.artist.c_str());
	if (fSong.fee == 1)
		subtitle << "  ·  VIP";
	const BString subtitleFitted = Fit(owner, subtitle, textMax);
	owner->MovePenTo(BPoint(kTextX, frame.top + 43.0f));
	owner->DrawString(subtitleFitted.String());

	const float albumX = AlbumX(width);
	const float albumMax = HeartX(width) - albumX - 20.0f;
	const BString album = Fit(owner, fSong.album.c_str(), albumMax);
	owner->MovePenTo(BPoint(albumX, frame.top + 34.0f));
	owner->DrawString(album.String());

	BFont heartFont(be_plain_font);
	heartFont.SetSize(15.0f);
	owner->SetFont(&heartFont);
	owner->SetHighColor(Accent());
	owner->MovePenTo(BPoint(HeartX(width), frame.top + 35.0f));
	owner->DrawString("♥");

	const long long seconds = fSong.durationMs / 1000;
	char duration[16];
	snprintf(duration, sizeof(duration), "%lld:%02lld", seconds / 60,
		seconds % 60);
	owner->SetFont(&font);
	owner->SetHighColor(SecondaryText());
	owner->MovePenTo(BPoint(DurationRight(width) - owner->StringWidth(duration),
		frame.top + 34.0f));
	owner->DrawString(duration);

	owner->PopState();
}
