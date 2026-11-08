#ifndef NCM_NOWPLAYING_PAGE_H
#define NCM_NOWPLAYING_PAGE_H

#include <Bitmap.h>
#include <Control.h>
#include <String.h>
#include <View.h>

#include <vector>

class BCheckBox;
class BStringView;
class BTabView;
class BTextView;
class CoverView;

enum {
	kMessageNowPlayingReady = 'nprd', // 曲目信息/封面/歌词/评论都落盘了
	kMessageNowPlayingClose = 'npcx', // 播放页的返回按钮
	kMessageOpenArtist = 'opat',      // 点歌手名 -> 歌手页（id）
	kMessageOpenAlbum = 'opal',       // 点专辑名 -> 专辑页（id）
	kMessageToggleTrans = 'ttrn',
	kMessageToggleRoma = 'trom',
};

// 可点击的文字（歌手名 / 专辑名），hover 加下划线
class LinkView : public BControl {
public:
	LinkView(const char* name, const char* text, uint32 what, bool secondary);

	void SetText(const char* text);
	void SetId(long long id);
	void SetSecondary(bool secondary);

	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	void MouseMoved(BPoint where, uint32 transit,
		const BMessage* dragMessage) override;
	void GetPreferredSize(float* width, float* height) override;

private:
	BString fText;
	long long fId = 0;
	bool fSecondary = false;
	bool fHover = false;
};

// LRC 歌词视图：原文 / 翻译 / 罗马音三层，当前行居中高亮
class LrcView : public BView {
public:
	LrcView();

	void SetLyric(const BString& lrc, const BString& translation,
		const BString& romaji);
	void SetShowTranslation(bool show);
	void SetShowRoman(bool show);
	void SetPosition(double seconds);

	bool HasTranslation() const { return fHasTranslation; }
	bool HasRoman() const { return fHasRoman; }

	void Draw(BRect updateRect) override;
	void FrameResized(float width, float height) override;

private:
	struct Line {
		double time = 0.0;
		BString text;
		BString translation;
		BString roman;
	};

	std::vector<Line> fLines;
	bool fHasTranslation = false;
	bool fHasRoman = false;
	bool fShowTranslation = true;
	bool fShowRoman = false;
	int32 fCurrent = -1;
};

// 底部徽章：小封面 + 曲名/歌手，点击在列表/播放页之间切换
class NowPlayingBadge : public BControl {
public:
	NowPlayingBadge();

	void SetInfo(const BString& title, const BString& artist,
		BBitmap* cover);
	void SetCover(BBitmap* cover); // 接管所有权

	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	void GetPreferredSize(float* width, float* height) override;

private:
	BString fTitle;
	BString fArtist;
	BBitmap* fCover = nullptr;
};

// 播放页：嵌在主窗口内容区（把歌曲列表换掉），底部播放控件始终保留。
// 背景是模糊过的大封面（Haiku 没有现成的模糊 API，本地做降采样 + box blur）。
class NowPlayingView : public BView {
public:
	NowPlayingView();

	void SetSong(const BString& title, const BString& artist,
		long long artistId, const BString& album, long long albumId,
		const BString& coverPath, const BString& lyricPath,
		const BString& translationPath, const BString& romajiPath,
		const BString& commentsPath);
	void SetPosition(double seconds);
	void SelectTab(int32 index); // 0 歌词 / 1 评论

	void MessageReceived(BMessage* message) override;
	void Draw(BRect updateRect) override;
	void FrameResized(float width, float height) override;

private:
	void _ApplyBackdrop();
	void _LoadLyric(const BString& lyricPath, const BString& translationPath,
		const BString& romajiPath);
	void _LoadComments(const BString& commentsPath);
	void _RefreshToggles();

	CoverView* fCover = nullptr;
	BStringView* fTitle = nullptr;
	LinkView* fArtist = nullptr;
	LinkView* fAlbum = nullptr;
	LrcView* fLrc = nullptr;
	BTextView* fComments = nullptr;
	BCheckBox* fTransToggle = nullptr;
	BCheckBox* fRomaToggle = nullptr;
	BView* fToggleRow = nullptr;
	BTabView* fTabs = nullptr;

	BBitmap* fBackdrop = nullptr;
};

#endif
