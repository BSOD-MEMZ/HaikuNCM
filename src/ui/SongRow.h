#ifndef NCM_SONG_ROW_H
#define NCM_SONG_ROW_H

#include <ColumnListView.h>
#include <ColumnTypes.h>

#include "net/netease.h"

class SongListView;

// 一行歌曲。BColumnListView 的行模型：BRow 持 BField，列负责画。
class SongField : public BStringField {
public:
	SongField(const ncm::Song& song);

	void SetCover(BBitmap* bitmap);
	BBitmap* Cover() const { return fCover; }
	void SetDisplayIndex(int32 index) { fDisplayIndex = index; }
	int32 DisplayIndex() const { return fDisplayIndex; }

	const ncm::Song& Song() const { return fSong; }

private:
	ncm::Song fSong;
	BBitmap* fCover = nullptr;
	int32 fDisplayIndex = 0;
};

class SongRow : public BRow {
public:
	SongRow(const ncm::Song& song);

	void SetCover(BBitmap* bitmap);
	void SetDisplayIndex(int32 index);
	SongField* Field() const
	{
		return static_cast<SongField*>(const_cast<BField*>(GetField(0)));
	}
	const ncm::Song& Song() const { return Field()->Song(); }
};

// 标题列：封面 + 粗体标题 + 歌手双行
class SongTitleColumn : public BTitledColumn {
public:
	SongTitleColumn(const char* title, float width, float minWidth,
		float maxWidth);

	void DrawField(BField* field, BRect rect, BView* parent) override;
	int CompareFields(BField* a, BField* b) override;
};

class SongAlbumColumn : public BTitledColumn {
public:
	SongAlbumColumn(const char* title, float width, float minWidth,
		float maxWidth);

	void DrawField(BField* field, BRect rect, BView* parent) override;
	int CompareFields(BField* a, BField* b) override;
};

// 喜欢列：心形符号（点赞态需要登录，登录前统一空心）
class SongHeartColumn : public BTitledColumn {
public:
	SongHeartColumn(const char* title, float width, float minWidth,
		float maxWidth);

	void DrawField(BField* field, BRect rect, BView* parent) override;
	int CompareFields(BField* a, BField* b) override;
};

class SongDurationColumn : public BTitledColumn {
public:
	SongDurationColumn(const char* title, float width, float minWidth,
		float maxWidth);

	void DrawField(BField* field, BRect rect, BView* parent) override;
	int CompareFields(BField* a, BField* b) override;
};

// 主列表：斑马纹 + 右键菜单（播放 / 下一首播放）
class SongListView : public BColumnListView {
public:
	SongListView();

	void MouseDown(BPoint where) override;

private:
	void _PaintZebra();
	void _ShowContextMenu(BPoint where);
};

#endif
