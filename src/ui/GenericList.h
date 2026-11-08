#ifndef NCM_GENERICLIST_H
#define NCM_GENERICLIST_H

#include <ColumnListView.h>
#include <Bitmap.h>

#include <vector>

#include "net/netease.h"

// 歌单 / 专辑 / 声音搜索结果：和单曲页同款表格（封面 + 名称 + 副标题）
class GenericRow : public BRow {
public:
	GenericRow(const ncm::GenericItem& item);
	~GenericRow() override;

	const ncm::GenericItem& Item() const { return fItem; }
	BBitmap* Thumb() { return fThumb; }
	void SetThumb(BBitmap* bitmap); // 接管所有权
	void EnsureThumb();             // 从缓存懒加载

private:
	ncm::GenericItem fItem;
	BBitmap* fThumb = nullptr;
	bool fThumbTried = false;
};

class GenericListView : public BColumnListView {
public:
	GenericListView();

	void Fill(const std::vector<ncm::GenericItem>& items);
	void RefreshThumbs(); // 缓存里有新封面就刷一遍
};

#endif
