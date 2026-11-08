#include "NowPlayingPage.h"

#include "CoverView.h"

#include <Button.h>
#include <Font.h>
#include <Catalog.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TranslationUtils.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "NowPlayingPage"

namespace {

// [mm:ss.xx] 或 [mm:ss] 时间标签
bool ParseTimestamp(const BString& tag, double* seconds)
{
	const std::string text(tag.String());
	if (text.size() < 5 || text[0] != '[')
		return false;

	const size_t colon = text.find(':');
	size_t dot = text.find('.');
	if (dot == std::string::npos)
		dot = text.find(']');
	if (colon == std::string::npos || colon == 0 || dot <= colon)
		return false;

	const std::string minutes = text.substr(1, colon - 1);
	const std::string secs = text.substr(colon + 1, dot - colon - 1);
	std::string fraction;
	if (dot + 1 < text.size() && text[dot] == '.')
		fraction = text.substr(dot + 1, text.find(']', dot) - dot - 1);

	char* end = nullptr;
	const double m = strtod(minutes.c_str(), &end);
	if (end == minutes.c_str())
		return false;
	double s = strtod(secs.c_str(), &end);
	if (end == secs.c_str())
		return false;
	if (!fraction.empty()) {
		while (fraction.size() < 3)
			fraction += "0";
		fraction.resize(3);
		s += strtod(fraction.c_str(), &end) / 1000.0;
	}

	*seconds = m * 60.0 + s;
	return true;
}

} // namespace

// ----------------------------------------------------------------- LrcView

LrcView::LrcView()
	: BView("lrc", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());
}

void LrcView::SetLyric(const BString& lrc)
{
	fLines.clear();
	fCurrent = -1;

	const std::string text(lrc.String());
	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();

		// 一行可能有多个时间标签，全部展开
		std::string line = text.substr(start, end - start);
		size_t bracket = line.find('[');
		double lastTime = -1.0;
		while (bracket == 0) {
			size_t close = line.find(']');
			if (close == std::string::npos)
				break;

			double seconds = 0.0;
			if (ParseTimestamp(BString(line.substr(0, close + 1).c_str()),
				&seconds)) {
				lastTime = seconds;
				std::string rest = line.substr(close + 1);
				const size_t firstNonSpace = rest.find_first_not_of(" \t");
				if (firstNonSpace != std::string::npos)
					rest = rest.substr(firstNonSpace);
				if (!rest.empty() && rest[0] != '[')
					fLines.push_back({seconds, BString(rest.c_str())});
			}
			line = line.substr(close + 1);
			bracket = line.find('[');
		}

		if (lastTime >= 0.0 && !line.empty()) {
			// 兜底：标签后面还有残余文本
			size_t firstNonSpace = line.find_first_not_of(" \t");
			if (firstNonSpace != std::string::npos)
				line = line.substr(firstNonSpace);
			if (!line.empty() && line[0] != '[')
				fLines.push_back({lastTime, BString(line.c_str())});
		}

		if (end >= text.size())
			break;
		start = end + 1;
	}

	// 按时间排序（有些 LRC 不严格有序）
	for (size_t i = 1; i < fLines.size(); i++) {
		Line key = fLines[i];
		size_t j = i;
		while (j > 0 && fLines[j - 1].time > key.time) {
			fLines[j] = fLines[j - 1];
			j--;
		}
		fLines[j] = key;
	}

	fCurrent = -1;
	Invalidate();
}

void LrcView::SetPosition(double seconds)
{
	int32 current = -1;
	for (size_t i = 0; i < fLines.size(); i++) {
		if (fLines[i].time <= seconds)
			current = static_cast<int32>(i);
		else
			break;
	}

	if (current != fCurrent) {
		fCurrent = current;
		Invalidate();
	}
}

void LrcView::FrameResized(float, float)
{
	Invalidate();
}

void LrcView::Draw(BRect)
{
	FillRect(Bounds(), B_SOLID_LOW);

	SetFont(be_plain_font);
	font_height fh;
	GetFontHeight(&fh);
	const float lineHeight = (fh.ascent + fh.descent) * 1.9f;

	if (fLines.empty()) {
		SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		MovePenTo(16.0f, Bounds().Height() / 2.0f);
		DrawString(B_TRANSLATE("（没有歌词）"));
		return;
	}

	const float left = Bounds().left + 8.0f;
	const float centerY = Bounds().Height() / 2.0f;

	// 当前行大致垂直居中，上下行渐隐
	for (size_t i = 0; i < fLines.size(); i++) {
		const float y = centerY
			+ (static_cast<float>(i) - fCurrent) * lineHeight;
		if (y < -lineHeight || y > Bounds().Height() + lineHeight)
			continue;

		const int32 distance = static_cast<int32>(i)
			- (fCurrent < 0 ? 0 : fCurrent);
		const int32 weight = distance < 0 ? -distance : distance;

		const bool active = weight == 0 && fCurrent >= 0;
		if (active)
			SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
		else
			SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
				B_DARKEN_2_TINT));

		if (fCurrent < 0 && i == 0)
			SetHighColor(ui_color(B_PANEL_TEXT_COLOR));

		SetFont(active ? be_bold_font : be_plain_font);

		BString text(fLines[i].text);
		TruncateString(&text, B_TRUNCATE_END, Bounds().Width() - 24.0f);
		MovePenTo(left, y + fh.ascent);
		DrawString(text);
	}
	SetFont(be_plain_font);
}

// ---------------------------------------------------------------- CoverView
// 实现搬到 CoverView.cpp（歌手页/专辑页也要用）

// ---------------------------------------------------------- NowPlayingBadge

NowPlayingBadge::NowPlayingBadge()
	: BControl("now-playing-badge", nullptr, new BMessage('nowp'),
		  B_WILL_DRAW)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());
	SetExplicitMinSize(BSize(300.0f, 56.0f));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 56.0f));
}

void NowPlayingBadge::GetPreferredSize(float* width, float* height)
{
	*width = 300.0f;
	*height = 56.0f;
}

void NowPlayingBadge::SetInfo(const BString& title, const BString& artist,
	BBitmap* cover)
{
	SetCover(cover);
	fTitle = title;
	fArtist = artist;
	Invalidate();
}

void NowPlayingBadge::SetCover(BBitmap* cover)
{
	delete fCover;
	fCover = cover;
}

void NowPlayingBadge::Draw(BRect)
{
	const BRect bounds = Bounds();
	const float coverSize = bounds.Height() - 12.0f;
	const BRect coverRect(bounds.left, bounds.top
			+ (bounds.Height() - coverSize) / 2.0f,
		bounds.left + coverSize,
		bounds.top + (bounds.Height() + coverSize) / 2.0f);

	if (fCover != nullptr && fCover->IsValid()) {
		SetDrawingMode(B_OP_ALPHA);
		DrawBitmap(fCover, fCover->Bounds(), coverRect,
			B_FILTER_BITMAP_BILINEAR);
		SetDrawingMode(B_OP_COPY);
	} else {
		SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		FillRect(coverRect);
	}

	const float textX = coverRect.right + 8.0f;
	const float textWidth = bounds.right - textX - 8.0f;

	BString title(fTitle);
	if (title.IsEmpty())
		title = B_TRANSLATE("没有正在播放的歌曲");
	TruncateString(&title, B_TRUNCATE_END, textWidth);
	BString artist(fArtist);
	TruncateString(&artist, B_TRUNCATE_END, textWidth);

	font_height fh;
	GetFontHeight(&fh);

	SetFont(be_bold_font);
	SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
	MovePenTo(textX, bounds.top + bounds.Height() / 2.0f - 2.0f);
	DrawString(title);

	SetFont(be_plain_font);
	SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR),
		B_LIGHTEN_1_TINT));
	MovePenTo(textX, bounds.top + bounds.Height() / 2.0f + fh.ascent + 2.0f);
	DrawString(artist);
	SetFont(be_plain_font);
}

void NowPlayingBadge::MouseDown(BPoint where)
{
	BControl::MouseDown(where);
	Invoke();
}

// ----------------------------------------------------------- NowPlayingView

NowPlayingView::NowPlayingView()
	: BView("now-playing", B_WILL_DRAW)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());

	fCover = new CoverView();
	fCover->SetExplicitMinSize(BSize(280, 280));
	fCover->SetExplicitMaxSize(BSize(280, 280));

	{
		BFont font(be_bold_font);
		font.SetSize(be_bold_font->Size() * 1.6f);
		fTitle = new BStringView("np-title", "");
		fTitle->SetFont(&font);
	}
	fTitle->SetExplicitMinSize(BSize(0, 26));

	fArtist = new BStringView("np-artist", "");

	fLrc = new LrcView();
	fLrc->SetExplicitMinSize(BSize(200, 120));

	BButton* back = new BButton("np-back", B_TRANSLATE("← 返回列表"),
		new BMessage(kMessageNowPlayingClose));

	// 右栏：曲名 / 歌手 / 滚动歌词（官方播放页就是封面在左、歌词在右）
	BGroupView* info = new BGroupView("np-info", B_VERTICAL,
		B_USE_SMALL_SPACING);
	BLayoutBuilder::Group<>(info, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(fTitle)
		.Add(fArtist)
		.Add(fLrc, 1.0f)
		.End();

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING, 0.0f)
			.Add(back)
			.AddGlue()
			.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING, 1.0f)
			.Add(fCover)
			.Add(info, 1.0f)
			.End()
		.End();
}

void NowPlayingView::SetNowPlaying(const BString& title,
	const BString& artist, const BString& coverPath, const BString& lrcPath)
{
	fTitle->SetText(title);
	fArtist->SetText(artist);
	fCover->SetBitmap(BTranslationUtils::GetBitmapFile(coverPath.String()));

	BString lyric;
	FILE* file = fopen(lrcPath.String(), "r");
	if (file != nullptr) {
		char buffer[2048];
		while (fgets(buffer, sizeof(buffer), file) != nullptr)
			lyric << buffer;
		fclose(file);
	}
	fLrc->SetLyric(lyric);
	fLrc->SetPosition(0.0);
}

void NowPlayingView::SetPosition(double seconds)
{
	fLrc->SetPosition(seconds);
}
