#include "NowPlayingPage.h"

#include "CoverView.h"
#include "ThemeColor.h"

#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <Cursor.h>
#include <Font.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TranslationUtils.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "NowPlayingPage"

namespace {

// [mm:ss.xx] 或 [mm:ss] 时间标签
bool ParseTimestamp(const char* tag, double* seconds)
{
	if (tag == nullptr || tag[0] != '[')
		return false;

	const char* colon = strchr(tag, ':');
	if (colon == nullptr)
		return false;
	const char* close = strchr(colon, ']');
	if (close == nullptr)
		return false;

	const std::string minutes(tag + 1, colon - tag - 1);
	std::string secs(colon + 1, close - colon - 1);
	if (minutes.empty() || secs.empty())
		return false;

	char* stop = nullptr;
	const double m = strtod(minutes.c_str(), &stop);
	if (stop == minutes.c_str())
		return false;

	double s = 0.0;
	const size_t dot = secs.find('.');
	if (dot != std::string::npos) {
		const double fraction = strtod(secs.c_str() + dot, &stop);
		secs.resize(dot);
		s = strtod(secs.c_str(), &stop) + fraction;
	} else {
		s = strtod(secs.c_str(), &stop);
	}

	*seconds = m * 60.0 + s;
	return true;
}

struct RawLine {
	double time = 0.0;
	std::string text;
};

// 一行 LRC 可能有多个时间标签（[00:01][00:05]词），每个标签各出一条
void ParseLrc(const std::string& text, std::vector<RawLine>* out)
{
	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();

		std::string line = text.substr(start, end - start);
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		// 先吃掉行首所有 [..] 标签
		std::vector<double> stamps;
		while (!line.empty() && line[0] == '[') {
			const size_t close = line.find(']');
			if (close == std::string::npos)
				break;
			double seconds = 0.0;
			const std::string tag = line.substr(0, close + 1);
			if (ParseTimestamp(tag.c_str(), &seconds))
				stamps.push_back(seconds);
			line = line.substr(close + 1);
		}

		const size_t firstNonSpace = line.find_first_not_of(" \t");
		if (firstNonSpace == std::string::npos)
			line.clear();
		else if (firstNonSpace > 0)
			line = line.substr(firstNonSpace);

		// 只有时间标签没有词的行不画
		if (!stamps.empty() && !line.empty()) {
			for (double stamp : stamps)
				out->push_back({stamp, line});
		}

		if (end >= text.size())
			break;
		start = end + 1;
	}
}

BString ReadFileText(const char* path)
{
	BString text;
	FILE* file = fopen(path, "r");
	if (file == nullptr)
		return text;

	char buffer[4096];
	while (fgets(buffer, sizeof(buffer), file) != nullptr)
		text << buffer;
	fclose(file);
	return text;
}

} // namespace

// ----------------------------------------------------------------- LinkView

LinkView::LinkView(const char* name, const char* text, uint32 what,
	bool secondary)
	: BControl(name, nullptr, new BMessage(what), B_WILL_DRAW)
	, fText(text)
	, fSecondary(secondary)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());
}

void LinkView::SetText(const char* text)
{
	fText = text;
	Invalidate();
}

void LinkView::SetId(long long id)
{
	fId = id;
}

void LinkView::SetSecondary(bool secondary)
{
	fSecondary = secondary;
	Invalidate();
}

void LinkView::SetBackground(rgb_color color)
{
	SetViewColor(color);
	SetLowColor(color);
}

void LinkView::GetPreferredSize(float* width, float* height)
{
	*width = StringWidth(fText.String()) + 4.0f;
	font_height fh;
	GetFontHeight(&fh);
	*height = fh.ascent + fh.descent + 2.0f;
}

void LinkView::Draw(BRect)
{
	font_height fh;
	GetFontHeight(&fh);

	const rgb_color text = fSecondary
		? tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_1_TINT)
		: ui_color(B_PANEL_TEXT_COLOR);
	SetHighColor(text);
	MovePenTo(2.0f, fh.ascent + 1.0f);
	DrawString(fText.String());

	if (fHover) {
		const float width = StringWidth(fText.String());
		SetHighColor(tint_color(text, B_DARKEN_1_TINT));
		StrokeLine(BPoint(2.0f, fh.ascent + 3.0f),
			BPoint(2.0f + width, fh.ascent + 3.0f));
	}
}

void LinkView::MouseDown(BPoint)
{
	if (fId == 0 || Window() == nullptr)
		return;

	if (Message() != nullptr) {
		BMessage message(*Message());
		message.AddInt64("id", fId);
		Window()->PostMessage(&message);
	}
}

void LinkView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	(void)dragMessage;
	const bool hover = transit != B_EXITED_VIEW && Bounds().Contains(where);
	if (hover != fHover) {
		fHover = hover;
		if (fId != 0) {
			BCursor cursor(hover ? B_CURSOR_ID_FOLLOW_LINK
								 : B_CURSOR_ID_SYSTEM_DEFAULT);
			SetViewCursor(&cursor);
		}
		Invalidate();
	}
}

// ----------------------------------------------------------------- LrcView

LrcView::LrcView()
	: BView("lrc", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
{
	// 不透明：歌词每句都会跟着当前行往上滚，透明视图不擦旧位置的字
	fBackground = ui_color(B_PANEL_BACKGROUND_COLOR);
	SetViewColor(fBackground);
	SetLowColor(fBackground);
}

void LrcView::SetBackground(rgb_color color)
{
	fBackground = color;
	SetViewColor(color);
	SetLowColor(color);
	Invalidate();
}

void LrcView::SetLyric(const BString& lrc, const BString& translation,
	const BString& romaji)
{
	fLines.clear();
	fCurrent = -1;

	std::vector<RawLine> raw;
	ParseLrc(std::string(lrc.String()), &raw);
	for (const RawLine& line : raw) {
		Line entry;
		entry.time = line.time;
		entry.text = BString(line.text.c_str());
		fLines.push_back(entry);
	}

	// 翻译 / 罗马音按时间戳就近合并进主歌词行
	const auto merge = [this](const BString& text, bool roman) {
		std::vector<RawLine> extra;
		ParseLrc(std::string(text.String()), &extra);
		if (extra.empty())
			return false;

		size_t cursor = 0;
		bool merged = false;
		for (Line& line : fLines) {
			while (cursor + 1 < extra.size()
				&& extra[cursor + 1].time <= line.time)
				cursor++;

			for (size_t k = cursor; k < extra.size(); k++) {
				const double delta = extra[k].time - line.time;
				if (delta < -0.3)
					continue;
				if (delta > 0.3)
					break;
				if (roman)
					line.roman = BString(extra[k].text.c_str());
				else
					line.translation = BString(extra[k].text.c_str());
				merged = true;
				break;
			}
		}
		return merged;
	};

	fHasTranslation = merge(translation, false);
	fHasRoman = merge(romaji, true);
	fShowTranslation = fHasTranslation;

	Invalidate();
}

void LrcView::SetShowTranslation(bool show)
{
	fShowTranslation = show;
	Invalidate();
}

void LrcView::SetShowRoman(bool show)
{
	fShowRoman = show;
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

	const int32 count = static_cast<int32>(fLines.size());
	font_height fh;
	GetFontHeight(&fh);
	const float lineHeight = (fh.ascent + fh.descent) * 1.9f;
	const float subHeight = lineHeight * 0.78f;

	if (count == 0) {
		SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR),
			B_LIGHTEN_2_TINT));
		MovePenTo(12.0f, Bounds().Height() / 2.0f);
		DrawString(B_TRANSLATE("（没有歌词）"));
		return;
	}

	// 先算每行总高，再把当前行的中心摆到视图中心
	std::vector<float> heights(count, 0.0f);
	for (int32 i = 0; i < count; i++) {
		float height = lineHeight;
		if (fShowTranslation && !fLines[i].translation.IsEmpty())
			height += subHeight;
		if (fShowRoman && !fLines[i].roman.IsEmpty())
			height += subHeight;
		heights[i] = height;
	}

	const int32 anchor = fCurrent >= 0 ? fCurrent : 0;
	float before = 0.0f;
	for (int32 i = 0; i < anchor; i++)
		before += heights[i];

	float y = Bounds().Height() / 2.0f - (before + heights[anchor] / 2.0f);
	const float left = 12.0f;
	const float available = Bounds().Width() - 24.0f;

	for (int32 i = 0; i < count; i++) {
		const bool current = i == fCurrent;
		if (y + heights[i] < -lineHeight
			|| y > Bounds().Height() + lineHeight) {
			y += heights[i];
			continue;
		}

		const rgb_color primary = current
			? ui_color(B_PANEL_TEXT_COLOR)
			: tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_2_TINT);
		const rgb_color secondary = current
			? tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_1_TINT)
			: tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_2_TINT);

		SetFont(current ? be_bold_font : be_plain_font);
		SetHighColor(primary);
		BString text(fLines[i].text);
		TruncateString(&text, B_TRUNCATE_END, available);
		MovePenTo(left, y + fh.ascent + lineHeight * 0.15f);
		DrawString(text);

		float next = y + lineHeight;
		if (fShowTranslation && !fLines[i].translation.IsEmpty()) {
			SetFont(be_plain_font);
			SetHighColor(secondary);
			BString line(fLines[i].translation);
			TruncateString(&line, B_TRUNCATE_END, available);
			MovePenTo(left, next + fh.ascent + subHeight * 0.1f);
			DrawString(line);
			next += subHeight;
		}
		if (fShowRoman && !fLines[i].roman.IsEmpty()) {
			SetFont(be_plain_font);
			SetHighColor(secondary);
			BString line(fLines[i].roman);
			TruncateString(&line, B_TRUNCATE_END, available);
			MovePenTo(left, next + fh.ascent + subHeight * 0.1f);
			DrawString(line);
			next += subHeight;
		}

		y += heights[i];
	}
	SetFont(be_plain_font);
}

// ---------------------------------------------------------- NowPlayingBadge

NowPlayingBadge::NowPlayingBadge()
	: BControl("now-playing-badge", nullptr, new BMessage('nowp'),
		  B_WILL_DRAW)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());
	SetExplicitMinSize(BSize(220.0f, 56.0f));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 56.0f));
}

void NowPlayingBadge::GetPreferredSize(float* width, float* height)
{
	*width = 220.0f;
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
		SetHighColor(tint_color(ViewColor(), B_DARKEN_2_TINT));
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
	fTheme = ui_color(B_PANEL_BACKGROUND_COLOR);
	SetViewColor(fTheme);
	SetLowColor(fTheme);

	fCover = new CoverView();
	fCover->SetPadding(0.0f);
	fCover->SetExplicitMinSize(BSize(300, 300));
	fCover->SetExplicitMaxSize(BSize(300, 300));

	BFont titleFont(be_bold_font);
	titleFont.SetSize(be_bold_font->Size() * 1.6f);
	fTitle = new BStringView("np-title", "");
	fTitle->SetFont(&titleFont);
	fTitle->SetExplicitMinSize(BSize(0, 26));

	fArtist = new LinkView("np-artist", "", kMessageOpenArtist, false);
	fAlbum = new LinkView("np-album", "", kMessageOpenAlbum, true);

	fLrc = new LrcView();
	fLrc->SetExplicitMinSize(BSize(200, 140));

	fTransToggle = new BCheckBox("trans", B_TRANSLATE("译"),
		new BMessage(kMessageToggleTrans));
	fRomaToggle = new BCheckBox("roma", B_TRANSLATE("音"),
		new BMessage(kMessageToggleRoma));
	fTransToggle->SetTarget(this);
	fRomaToggle->SetTarget(this);

	fInfo = new BGroupView("np-info", B_VERTICAL, B_USE_SMALL_SPACING);
	BLayoutBuilder::Group<>(static_cast<BGroupView*>(fInfo), B_VERTICAL,
		B_USE_SMALL_SPACING)
		.Add(fTitle, 0.0f)
		.Add(fArtist, 0.0f)
		.Add(fAlbum, 0.0f)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING, 0.0f)
			.AddGlue()
			.Add(fTransToggle)
			.Add(fRomaToggle)
			.End()
		.Add(fLrc, 1.0f)
		.End();

	BButton* back = new BButton("np-back", B_TRANSLATE("← 返回列表"),
		new BMessage(kMessageNowPlayingClose));

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING, 0.0f)
			.Add(back)
			.AddGlue()
			.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING, 1.0f)
			.Add(fCover)
			.Add(fInfo, 1.0f)
			.End()
		.End();

	_RefreshToggles();
}

void NowPlayingView::SetThemeColor(rgb_color color)
{
	fTheme = color;
	SetViewColor(color);
	SetLowColor(color);
	if (fInfo != nullptr) {
		fInfo->SetViewColor(color);
		fInfo->SetLowColor(color);
	}
	if (fLrc != nullptr)
		fLrc->SetBackground(color);
	if (fArtist != nullptr)
		fArtist->SetBackground(color);
	if (fAlbum != nullptr)
		fAlbum->SetBackground(color);
	Invalidate();
}

void NowPlayingView::SetSong(const BString& title, const BString& artist,
	long long artistId, const BString& album, long long albumId,
	const BString& coverPath, const BString& lyricPath,
	const BString& translationPath, const BString& romajiPath)
{
	fTitle->SetText(title);
	fArtist->SetText(artist.IsEmpty() ? B_TRANSLATE("未知歌手") : artist.String());
	fArtist->SetId(artistId);
	fAlbum->SetText(album.IsEmpty() ? B_TRANSLATE("未知专辑") : album.String());
	fAlbum->SetId(albumId);
	fCover->SetBitmap(BTranslationUtils::GetBitmapFile(coverPath.String()));

	_LoadLyric(lyricPath, translationPath, romajiPath);

	// 文字宽度变了 BStringView 不会自己让父布局重排，不重排就会留下旧残影
	InvalidateLayout(true);
	Invalidate();
	if (Window() != nullptr)
		Window()->UpdateIfNeeded();
}

void NowPlayingView::_LoadLyric(const BString& lyricPath,
	const BString& translationPath, const BString& romajiPath)
{
	fLrc->SetLyric(ReadFileText(lyricPath.String()),
		ReadFileText(translationPath.String()),
		ReadFileText(romajiPath.String()));
	_RefreshToggles();
}

void NowPlayingView::_RefreshToggles()
{
	if (fLrc->HasTranslation())
		fTransToggle->Show();
	else
		fTransToggle->Hide();

	if (fLrc->HasRoman())
		fRomaToggle->Show();
	else
		fRomaToggle->Hide();

	fTransToggle->SetValue(fLrc->HasTranslation() ? B_CONTROL_ON
												 : B_CONTROL_OFF);
	fRomaToggle->SetValue(B_CONTROL_OFF);
}

void NowPlayingView::SetPosition(double seconds)
{
	fLrc->SetPosition(seconds);
}

void NowPlayingView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMessageToggleTrans:
			fLrc->SetShowTranslation(fTransToggle->Value() == B_CONTROL_ON);
			Invalidate();
			return;

		case kMessageToggleRoma:
			fLrc->SetShowRoman(fRomaToggle->Value() == B_CONTROL_ON);
			Invalidate();
			return;

		default:
			break;
	}
	BView::MessageReceived(message);
}
