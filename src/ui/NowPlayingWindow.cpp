#include "NowPlayingWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TranslationUtils.h>
#include <Window.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "NowPlayingWindow"

namespace {

// [mm:ss.xx] 或 [mm:ss] 时间标签
bool ParseTimestamp(const BString& tag, double* seconds)
{
	// 形如 [mm:ss.xx] / [mm:ss]
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
	fScrollOffset = 0.0f;

	const std::string text(lrc.String());
	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();

		std::string line = text.substr(start, end - start);
		// 一行可能有多个时间标签，全部展开
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
	const float lineHeight = (fh.ascent + fh.descent) * 1.6f;

	// 没有歌词时给个占位
	if (fLines.empty()) {
		SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		BString empty(B_TRANSLATE("（没有歌词）"));
		MovePenTo(16.0f, Bounds().Height() / 2.0f);
		DrawString(empty);
		return;
	}

	const float centerX = Bounds().left + 16.0f;
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

		rgb_color color = ui_color(B_PANEL_BACKGROUND_COLOR);
		if (weight == 0 && fCurrent >= 0)
			color = ui_color(B_PANEL_TEXT_COLOR);
		else
			color = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
				B_DARKEN_2_TINT);

		if (fCurrent < 0 && i == 0)
			color = ui_color(B_PANEL_TEXT_COLOR);

		SetHighColor(color);
		if (weight == 0 && fCurrent >= 0)
			SetFont(be_bold_font);
		else
			SetFont(be_plain_font);

		BString text(fLines[i].text);
		TruncateString(&text, B_TRUNCATE_END, Bounds().Width() - 32.0f);
		MovePenTo(centerX, y + fh.ascent);
		DrawString(text);
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
	SetExplicitMinSize(BSize(200.0f, 44.0f));
	SetExplicitMaxSize(BSize(260.0f, B_SIZE_UNLIMITED));
}

void NowPlayingBadge::GetPreferredSize(float* width, float* height)
{
	*width = 220.0f;
	*height = 44.0f;
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
	const float coverSize = bounds.Height() - 8.0f;

	if (fCover != nullptr && fCover->IsValid()) {
		SetDrawingMode(B_OP_ALPHA);
		DrawBitmap(fCover, fCover->Bounds(),
			BRect(bounds.left + 4.0f, bounds.top + 4.0f,
				bounds.left + 4.0f + coverSize,
				bounds.top + 4.0f + coverSize),
			B_FILTER_BITMAP_BILINEAR);
		SetDrawingMode(B_OP_COPY);
	} else {
		SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		FillRect(BRect(bounds.left + 4.0f, bounds.top + 4.0f,
			bounds.left + 4.0f + coverSize,
			bounds.top + 4.0f + coverSize));
	}

	const float textX = bounds.left + coverSize + 12.0f;
	const float textWidth = bounds.right - textX - 8.0f;

	BString title(fTitle);
	TruncateString(&title, B_TRUNCATE_END, textWidth);
	BString artist(fArtist);
	TruncateString(&artist, B_TRUNCATE_END, textWidth);

	SetFont(be_bold_font);
	SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
	MovePenTo(textX, bounds.top + 16.0f);
	DrawString(title);

	SetFont(be_plain_font);
	SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR),
		B_LIGHTEN_1_TINT));
	MovePenTo(textX, bounds.top + 32.0f);
	DrawString(artist);
	SetFont(be_plain_font);
}

void NowPlayingBadge::MouseDown(BPoint where)
{
	BControl::MouseDown(where);
	Invoke();
}

// --------------------------------------------------------- NowPlayingWindow

NowPlayingWindow::NowPlayingWindow(BMessenger target)
	: BWindow(BRect(0, 0, 720, 480), "正在播放", B_TITLED_WINDOW,
		  B_AUTO_UPDATE_SIZE_LIMITS)
	, fTarget(target)
{
	fCoverView = new BView("cover-area", B_WILL_DRAW);
	fCoverView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	fCoverView->SetExplicitMinSize(BSize(280, 280));
	fCoverView->SetExplicitMaxSize(BSize(280, 280));

	fLrc = new LrcView();

	fTitle = new BStringView("np-title", "");
	fTitle->SetFont(be_bold_font);
	fArtist = new BStringView("np-artist", "");

	BLayoutBuilder::Group<>(this, B_HORIZONTAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_VERTICAL, 0)
			.Add(fCoverView)
		.End()
		.AddGroup(B_VERTICAL, B_USE_DEFAULT_SPACING)
			.Add(fTitle)
			.Add(fArtist)
			.Add(new BScrollView("lrc-scroll", fLrc, 0, false, true), 1.0f)
		.End();

	CenterOnScreen();
}

void NowPlayingWindow::SetNowPlaying(const BString& title,
	const BString& artist, const BString& coverPath, const BString& lrcPath)
{
	fTitle->SetText(title);
	fArtist->SetText(artist);

	delete fCover;
	fCover = BTranslationUtils::GetBitmapFile(coverPath.String());
	// 居中贴大封面
	if (fCover != nullptr && fCover->IsValid()) {
		BRect bounds(fCoverView->Bounds());
		BRect dest(bounds);
		const float side = std::min(bounds.Width(), bounds.Height()) - 8.0f;
		dest = BRect(bounds.left + (bounds.Width() - side) / 2.0f,
			bounds.top + (bounds.Height() - side) / 2.0f,
			bounds.left + (bounds.Width() + side) / 2.0f,
			bounds.top + (bounds.Height() + side) / 2.0f);
		fCoverView->FillRect(bounds, B_SOLID_LOW);
		fCoverView->SetDrawingMode(B_OP_ALPHA);
		fCoverView->DrawBitmap(fCover, fCover->Bounds(), dest,
			B_FILTER_BITMAP_BILINEAR);
		fCoverView->SetDrawingMode(B_OP_COPY);
		fCoverView->Invalidate();
	} else {
		fCoverView->FillRect(fCoverView->Bounds(), B_SOLID_LOW);
		fCoverView->Invalidate();
	}

	// 歌词
	BString* lrcText = new BString();
	FILE* file = fopen(lrcPath.String(), "r");
	if (file != nullptr) {
		char buffer[2048];
		while (fgets(buffer, sizeof(buffer), file) != nullptr)
			*lrcText << buffer;
		fclose(file);
	}
	fLrc->SetLyric(*lrcText);
	delete lrcText;

	SetPosition(0.0);
}

void NowPlayingWindow::SetPosition(double seconds)
{
	fLrc->SetPosition(seconds);
}

void NowPlayingWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMessageNowPlayingReady:
		{
			const char* title = nullptr;
			const char* artist = nullptr;
			const char* coverPath = nullptr;
			const char* lrcPath = nullptr;
			if (message->FindString("title", &title) == B_OK
				&& message->FindString("artist", &artist) == B_OK
				&& message->FindString("cover", &coverPath) == B_OK
				&& message->FindString("lrc", &lrcPath) == B_OK) {
				SetNowPlaying(title, artist, coverPath, lrcPath);
			}
			return;
		}

		case kMessageNowPlayingTick:
		{
			double position = 0.0;
			if (message->FindDouble("position", &position) == B_OK)
				SetPosition(position);
			return;
		}

		default:
			BWindow::MessageReceived(message);
			return;
	}
}

bool NowPlayingWindow::QuitRequested()
{
	BMessage closed(kMessageNowPlayingClosed);
	fTarget.SendMessage(&closed);
	return true;
}
