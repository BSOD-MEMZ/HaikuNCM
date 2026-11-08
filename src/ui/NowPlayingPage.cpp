#include "NowPlayingPage.h"

#include "CoverView.h"

#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <Cursor.h>
#include <Font.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TabView.h>
#include <TextView.h>
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

		// 只有时间标签没有词的行不画（旧实现在这里重复塞了一次）
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

// 背景模糊：按块求均值降采样到 small x small，再跑两轮 3x3 盒式模糊，
// 绘制时双线性放大。Haiku 没有现成的模糊 API，这个观感够用。
BBitmap* MakeBlurredCover(const BBitmap* source, int32 small = 36)
{
	if (source == nullptr || !source->IsValid())
		return nullptr;

	const color_space space = source->ColorSpace();
	if (space != B_RGB32 && space != B_RGBA32 && space != B_RGB24)
		return nullptr;

	const BRect bounds = source->Bounds();
	const int32 width = static_cast<int32>(bounds.Width()) + 1;
	const int32 height = static_cast<int32>(bounds.Height()) + 1;
	if (width <= 0 || height <= 0 || small <= 0)
		return nullptr;

	const uchar* pixels = static_cast<const uchar*>(source->Bits());
	const int32 rowBytes = source->BytesPerRow();
	const int32 pixelSize = space == B_RGB24 ? 3 : 4;

	std::vector<float> red(small * small, 0.0f);
	std::vector<float> green(small * small, 0.0f);
	std::vector<float> blue(small * small, 0.0f);
	std::vector<int> count(small * small, 0);

	for (int32 y = 0; y < height; y++) {
		const uchar* row = pixels + y * rowBytes;
		const int32 by = y * small / height;
		for (int32 x = 0; x < width; x++) {
			const uchar* pixel = row + x * pixelSize;
			const int32 index = by * small + (x * small / width);
			// 小端下 B_RGB32 内存顺序是 B,G,R,A
			blue[index] += pixel[0];
			green[index] += pixel[1];
			red[index] += pixel[2];
			count[index]++;
		}
	}

	for (int32 i = 0; i < small * small; i++) {
		if (count[i] == 0)
			continue;
		red[i] /= count[i];
		green[i] /= count[i];
		blue[i] /= count[i];
	}

	const auto blur = [small](std::vector<float>& plane) {
		std::vector<float> out(plane.size(), 0.0f);
		for (int32 y = 0; y < small; y++) {
			for (int32 x = 0; x < small; x++) {
				float sum = 0.0f;
				int32 n = 0;
				for (int32 dy = -1; dy <= 1; dy++) {
					for (int32 dx = -1; dx <= 1; dx++) {
						const int32 xx = x + dx;
						const int32 yy = y + dy;
						if (xx < 0 || yy < 0 || xx >= small || yy >= small)
							continue;
						sum += plane[yy * small + xx];
						n++;
					}
				}
				out[y * small + x] = sum / n;
			}
		}
		plane.swap(out);
	};

	for (int32 pass = 0; pass < 2; pass++) {
		blur(red);
		blur(green);
		blur(blue);
	}

	BBitmap* result = new BBitmap(BRect(0, 0, small - 1, small - 1),
		B_RGB32);
	if (!result->IsValid()) {
		delete result;
		return nullptr;
	}

	uchar* destination = static_cast<uchar*>(result->Bits());
	const int32 destinationRow = result->BytesPerRow();
	// 往面板色方向压一档，歌词直接压在背景上也读得清
	const rgb_color wash = ui_color(B_PANEL_BACKGROUND_COLOR);
	const float keep = 0.55f;
	const float mix = 1.0f - keep;
	for (int32 y = 0; y < small; y++) {
		uchar* row = destination + y * destinationRow;
		for (int32 x = 0; x < small; x++) {
			uchar* pixel = row + x * 4;
			pixel[0] = static_cast<uchar>(blue[y * small + x] * keep
				+ wash.blue * mix);
			pixel[1] = static_cast<uchar>(green[y * small + x] * keep
				+ wash.green * mix);
			pixel[2] = static_cast<uchar>(red[y * small + x] * keep
				+ wash.red * mix);
			pixel[3] = 255;
		}
	}
	return result;
}

} // namespace

// ----------------------------------------------------------------- LinkView

LinkView::LinkView(const char* name, const char* text, uint32 what,
	bool secondary)
	: BControl(name, nullptr, new BMessage(what), B_WILL_DRAW)
	, fText(text)
	, fSecondary(secondary)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	SetLowColor(B_TRANSPARENT_COLOR);
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
	// 不能设透明：歌词每句都会跟着当前行往上滚，透明视图不擦底色，
	// 旧位置的字会一直留着 —— 这就是"歌词重复显示"的来源
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());
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
	: BView("now-playing", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());

	fCover = new CoverView();
	fCover->SetViewColor(B_TRANSPARENT_COLOR); // 让模糊背景透出来
	fCover->SetLowColor(B_TRANSPARENT_COLOR);
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

	fComments = new BTextView("comments");
	fComments->MakeEditable(false);
	fComments->SetWordWrap(true);
	fComments->SetInsets(8.0f, 8.0f, 8.0f, 8.0f);
	fComments->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);

	BGroupView* lyricCard = new BGroupView("lyric-card", B_VERTICAL, 0.0f);
	lyricCard->SetViewColor(B_TRANSPARENT_COLOR);
	BLayoutBuilder::Group<>(lyricCard, B_VERTICAL, 0.0f)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING, 0.0f)
			.SetInsets(0, 2, 4, 0)
			.AddGlue()
			.Add(fTransToggle)
			.Add(fRomaToggle)
			.End()
		.Add(fLrc, 1.0f)
		.End();

	auto* tabs = new BTabView("np-tabs", B_WIDTH_FROM_LABEL);
	tabs->SetBorder(B_NO_BORDER);
	tabs->ContainerView()->SetViewColor(B_TRANSPARENT_COLOR);
	fTabs = tabs;

	BTab* lyricTab = new BTab();
	tabs->AddTab(lyricCard, lyricTab);
	lyricTab->SetLabel(B_TRANSLATE("歌词"));

	BScrollView* commentScroll = new BScrollView("comments-scroll", fComments,
		0, false, true);
	BTab* commentTab = new BTab();
	tabs->AddTab(commentScroll, commentTab);
	commentTab->SetLabel(B_TRANSLATE("评论"));

	BButton* back = new BButton("np-back", B_TRANSLATE("← 返回列表"),
		new BMessage(kMessageNowPlayingClose));

	BGroupView* info = new BGroupView("np-info", B_VERTICAL,
		B_USE_SMALL_SPACING);
	info->SetViewColor(B_TRANSPARENT_COLOR);
	// 三行文字必须 weight 0：给 1 的话竖向会平分剩余空间，文字被拉高，
	// 等歌词加载完再缩回去 —— 布局动过、旧位置的像素没人擦，屏幕上就重影了
	BLayoutBuilder::Group<>(info, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(fTitle, 0.0f)
		.Add(fArtist, 0.0f)
		.Add(fAlbum, 0.0f)
		.Add(tabs, 1.0f)
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

	_RefreshToggles();
}

void NowPlayingView::SetSong(const BString& title, const BString& artist,
	long long artistId, const BString& album, long long albumId,
	const BString& coverPath, const BString& lyricPath,
	const BString& translationPath, const BString& romajiPath,
	const BString& commentsPath)
{
	fTitle->SetText(title);
	fArtist->SetText(artist.IsEmpty() ? B_TRANSLATE("未知歌手") : artist.String());
	fArtist->SetId(artistId);
	fAlbum->SetText(album.IsEmpty() ? B_TRANSLATE("未知专辑") : album.String());
	fAlbum->SetId(albumId);

	BBitmap* cover = BTranslationUtils::GetBitmapFile(coverPath.String());
	delete fBackdrop;
	fBackdrop = MakeBlurredCover(cover);
	fCover->SetBitmap(cover); // 接管所有权
	_ApplyBackdrop();

	_LoadLyric(lyricPath, translationPath, romajiPath);
	_LoadComments(commentsPath);

	// BStringView 的宽度是跟着文字走的，而 SetText 不会让父布局重排；
	// 不手动重排 + 整体重绘，屏幕上就会留下按旧尺寸画的残影（tab 条/歌词重一份）
	InvalidateLayout(true);
	Invalidate();
	if (Window() != nullptr)
		Window()->UpdateIfNeeded();
}

void NowPlayingView::_ApplyBackdrop()
{
	if (fBackdrop == nullptr || !fBackdrop->IsValid())
		return;

	// 背景用视图位图（app_server 自己合成），比在自己的 Draw 里半透明画一层
	// 稳得多：窗口缩小/子视图移动时不会留下没擦掉的旧像素
	SetViewBitmap(fBackdrop, fBackdrop->Bounds(), Bounds(),
		B_FOLLOW_ALL_SIDES, B_FILTER_BITMAP_BILINEAR);
	if (fLrc != nullptr) {
		fLrc->SetViewBitmap(fBackdrop, fBackdrop->Bounds(), fLrc->Bounds(),
			B_FOLLOW_ALL_SIDES, B_FILTER_BITMAP_BILINEAR);
		fLrc->Invalidate();
	}
	Invalidate();
}

void NowPlayingView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	_ApplyBackdrop();
}

void NowPlayingView::_LoadLyric(const BString& lyricPath,
	const BString& translationPath, const BString& romajiPath)
{
	fLrc->SetLyric(ReadFileText(lyricPath.String()),
		ReadFileText(translationPath.String()),
		ReadFileText(romajiPath.String()));
	_RefreshToggles();
}

void NowPlayingView::_LoadComments(const BString& commentsPath)
{
	const BString text = ReadFileText(commentsPath.String());
	fComments->SetText(text.IsEmpty() ? B_TRANSLATE("（没有评论）")
									 : text.String());
	fComments->ScrollTo(0.0f, 0.0f);
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

void NowPlayingView::SelectTab(int32 index)
{
	if (fTabs != nullptr && index >= 0 && index < fTabs->CountTabs())
		fTabs->Select(index);
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

void NowPlayingView::Draw(BRect)
{
	// 背景由 _ApplyBackdrop() 设的视图位图提供，这里不需要画东西
}
