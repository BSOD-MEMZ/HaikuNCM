#include "DetailWindow.h"

#include "CoverView.h"
#include "NowPlayingPage.h"
#include "SongRow.h"
#include "net/cachepaths.h"

#include <Alert.h>
#include <Catalog.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TabView.h>
#include <TranslationUtils.h>

#include <sys/stat.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "DetailWindow"

namespace {

BString Prefixed(const char* prefix, const std::string& detail)
{
	BString text(prefix);
	text << detail.c_str();
	return text;
}

// 简介塞进单行 BStringView：换行折成空格，太长就截断
BString Flatten(const std::string& text, size_t limit = 100)
{
	std::string flat;
	flat.reserve(text.size());
	for (char c : text) {
		if (c == '\r')
			continue;
		flat += (c == '\n') ? ' ' : c;
	}
	if (flat.size() > limit) {
		// 别把 UTF-8 多字节字符截断
		size_t cut = limit;
		while (cut > 0 && (flat[cut] & 0xC0) == 0x80)
			cut--;
		flat.resize(cut);
		flat += "…";
	}
	return BString(flat.c_str());
}

} // namespace

DetailWindow::DetailWindow(BMessenger target, bool artist)
	: BWindow(BRect(0, 0, 760, 560),
		  artist ? B_TRANSLATE("歌手") : B_TRANSLATE("专辑"), B_TITLED_WINDOW,
		  B_QUIT_ON_WINDOW_CLOSE)
	, fTarget(target)
	, fIsArtist(artist)
{
	fImage = new CoverView();
	fImage->SetExplicitMinSize(BSize(120, 120));
	fImage->SetExplicitMaxSize(BSize(120, 120));

	fTitle = new BStringView("detail-title", "");
	{
		BFont font(be_bold_font);
		font.SetSize(be_bold_font->Size() * 1.5f);
		fTitle->SetFont(&font);
	}
	fMeta = new BStringView("detail-meta", "");
	fDesc = new BStringView("detail-desc", "");

	BGroupView* info = new BGroupView("detail-info", B_VERTICAL,
		B_USE_SMALL_SPACING);
	BLayoutBuilder::Group<>(info, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(fTitle)
		.Add(fMeta)
		.Add(fDesc)
		.AddGlue()
		.End();
	info->SetExplicitMinSize(BSize(300, 120));

	fSongsList = new SongListView();
	AddSongColumns(fSongsList);
	fSongsList->SetInvocationMessage(new BMessage(kMessageDetailPlay));
	fSongsList->SetExplicitMinSize(BSize(320, 200));

	// 歌手页：歌曲/专辑两个 tab（无边框，tab 直接贴着列表）
	// 专辑页只有歌曲，套 tab 反而多余
	BView* content = nullptr;
	BTabView* tabs = nullptr;

	BGroupView* songsCard = new BGroupView("detail-songs", B_VERTICAL, 0.0f);
	BLayoutBuilder::Group<>(songsCard, B_VERTICAL, 0.0f)
		.Add(fSongsList, 1.0f)
		.End();

	if (fIsArtist) {
		tabs = new BTabView("detail-tabs", B_WIDTH_FROM_LABEL);
		tabs->SetBorder(B_NO_BORDER);
		BTab* songsTab = new BTab();
		tabs->AddTab(songsCard, songsTab);
		songsTab->SetLabel(B_TRANSLATE("歌曲"));
		content = tabs;
	} else {
		content = songsCard;
	}

	if (fIsArtist) {
		fAlbumList = new BListView("detail-albums", B_SINGLE_SELECTION_LIST);
		fAlbumList->SetInvocationMessage(new BMessage(kMessageDetailAlbumOpen));
		BGroupView* albumsCard = new BGroupView("detail-albums", B_VERTICAL,
			0.0f);
		BLayoutBuilder::Group<>(albumsCard, B_VERTICAL, 0.0f)
			.Add(fAlbumList, 1.0f)
			.End();
		BTab* albumsTab = new BTab();
		tabs->AddTab(albumsCard, albumsTab);
		albumsTab->SetLabel(B_TRANSLATE("专辑"));
	}

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fImage)
			.Add(info, 1.0f)
			.End()
		.Add(content, 1.0f)
		.End();

	ResizeTo(760, 560);
	CenterOnScreen();
}

void DetailWindow::LoadArtist(long long id, const char* name)
{
	SetTitle(name != nullptr && name[0] != '\0' ? name
											   : B_TRANSLATE("歌手"));
	if (fLoaded)
		return;
	fLoaded = true;

	auto* job = new Job{this, id};
	thread_id thread = spawn_thread(_ArtistThread, "ncm-artist",
		B_NORMAL_PRIORITY, job);
	if (thread < B_OK) {
		delete job;
		return;
	}
	resume_thread(thread);
}

void DetailWindow::LoadAlbum(long long id, const char* name)
{
	SetTitle(name != nullptr && name[0] != '\0' ? name
											   : B_TRANSLATE("专辑"));
	if (fLoaded)
		return;
	fLoaded = true;

	auto* job = new Job{this, id};
	thread_id thread = spawn_thread(_AlbumThread, "ncm-album",
		B_NORMAL_PRIORITY, job);
	if (thread < B_OK) {
		delete job;
		return;
	}
	resume_thread(thread);
}

int32 DetailWindow::_ArtistThread(void* data)
{
	auto* job = static_cast<Job*>(data);
	DetailWindow* window = job->window;
	const long long id = job->id;
	delete job;

	ncm::NeteaseApi api;
	std::string error;

	ncm::ArtistInfo info;
	std::vector<ncm::Song> songs;
	if (!api.FetchArtist(id, &info, &songs, &error)) {
		BMessage failure(kMessageDetailFail);
		failure.AddString("text",
			Prefixed(B_TRANSLATE("载入歌手失败: "), error).String());
		if (window->Lock()) {
			window->PostMessage(&failure);
			window->Unlock();
		}
		return 0;
	}

	// 头像落盘（缓存命中就别再下）
	if (!info.avatarUrl.empty()) {
		const std::string path = ncm::CacheDir() + "/artist_"
			+ std::to_string(id) + ".jpg";
		struct stat st;
		if (stat(path.c_str(), &st) != 0 || st.st_size < 1024)
			api.DownloadCover(info.avatarUrl + "?param=240y240", path,
				&error);
	}

	std::vector<ncm::AlbumInfo> albums;
	api.FetchArtistAlbums(id, &albums, &error);

	if (window->Lock()) {
		window->fSongs = songs;
		window->fAlbums = albums;
		window->Unlock();
	}

	BMessage ready(kMessageDetailReady);
	ready.AddString("title", info.name.c_str());
	BString meta;
	if (!info.alias.empty())
		meta << info.alias.c_str() << "    ";
	if (info.songCount > 0)
		meta << info.songCount << B_TRANSLATE(" 首单曲    ");
	if (info.albumCount > 0)
		meta << info.albumCount << B_TRANSLATE(" 张专辑");
	ready.AddString("meta", meta.String());
	ready.AddString("desc", Flatten(info.briefDesc).String());
	if (!info.avatarUrl.empty()) {
		const std::string path = ncm::CacheDir() + "/artist_"
			+ std::to_string(id) + ".jpg";
		ready.AddString("image", path.c_str());
	}

	BMessage albumsDone(kMessageDetailAlbums);
	if (window->Lock()) {
		window->PostMessage(&ready);
		window->PostMessage(&albumsDone);
		window->Unlock();
	}

	// 列表先出来，封面慢慢补（和主列表共用同一份缓存）
	for (const ncm::Song& song : songs) {
		if (song.coverUrl.empty())
			continue;
		const std::string path = ncm::CoverPath(song.id);
		struct stat st;
		if (stat(path.c_str(), &st) == 0 && st.st_size > 1024)
			continue;
		api.DownloadCover(song.coverUrl + "?param=120y120", path, &error);
	}

	BMessage covers(kMessageDetailCovers);
	if (window->Lock()) {
		window->PostMessage(&covers);
		window->Unlock();
	}
	return 0;
}

int32 DetailWindow::_AlbumThread(void* data)
{
	auto* job = static_cast<Job*>(data);
	DetailWindow* window = job->window;
	const long long id = job->id;
	delete job;

	ncm::NeteaseApi api;
	std::string error;

	ncm::AlbumInfo info;
	std::vector<ncm::Song> songs;
	if (!api.FetchAlbumDetail(id, &info, &songs, &error)) {
		BMessage failure(kMessageDetailFail);
		failure.AddString("text",
			Prefixed(B_TRANSLATE("载入专辑失败: "), error).String());
		if (window->Lock()) {
			window->PostMessage(&failure);
			window->Unlock();
		}
		return 0;
	}

	if (!info.coverUrl.empty()) {
		const std::string path = ncm::CacheDir() + "/album_"
			+ std::to_string(id) + ".jpg";
		struct stat st;
		if (stat(path.c_str(), &st) != 0 || st.st_size < 1024)
			api.DownloadCover(info.coverUrl + "?param=300y300", path,
				&error);
	}

	if (window->Lock()) {
		window->fSongs = songs;
		window->Unlock();
	}

	BMessage ready(kMessageDetailReady);
	ready.AddString("title", info.name.c_str());
	BString meta(info.artist.c_str());
	if (!info.publishTime.empty())
		meta << B_TRANSLATE("    发行 ") << info.publishTime.c_str();
	if (info.songCount > 0)
		meta << B_TRANSLATE("    ") << info.songCount
			<< B_TRANSLATE(" 首");
	ready.AddString("meta", meta.String());
	ready.AddString("desc", Flatten(info.description).String());
	if (!info.coverUrl.empty()) {
		const std::string path = ncm::CacheDir() + "/album_"
			+ std::to_string(id) + ".jpg";
		ready.AddString("image", path.c_str());
	}

	if (window->Lock()) {
		window->PostMessage(&ready);
		window->Unlock();
	}

	for (const ncm::Song& song : songs) {
		if (song.coverUrl.empty())
			continue;
		const std::string path = ncm::CoverPath(song.id);
		struct stat st;
		if (stat(path.c_str(), &st) == 0 && st.st_size > 1024)
			continue;
		api.DownloadCover(song.coverUrl + "?param=120y120", path, &error);
	}

	BMessage covers(kMessageDetailCovers);
	if (window->Lock()) {
		window->PostMessage(&covers);
		window->Unlock();
	}
	return 0;
}

void DetailWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMessageDetailReady:
		{
			const char* title = nullptr;
			const char* meta = nullptr;
			const char* desc = nullptr;
			const char* image = nullptr;
			if (message->FindString("title", &title) == B_OK
				&& title[0] != '\0')
				fTitle->SetText(title);
			if (message->FindString("meta", &meta) == B_OK)
				fMeta->SetText(meta);
			if (message->FindString("desc", &desc) == B_OK)
				fDesc->SetText(desc);
			if (message->FindString("image", &image) == B_OK)
				fImage->SetBitmap(BTranslationUtils::GetBitmapFile(image));

			_FillSongs();
			return;
		}

		case kMessageDetailAlbums:
			_FillAlbums();
			return;

		case kMessageOpenArtist:
		case kMessageOpenAlbum:
		{
			// 页面里的列表点歌手/专辑名，继续往下开新页
			int64 id = 0;
			if (message->FindInt64("id", &id) != B_OK || id <= 0)
				return;
			auto* window = new DetailWindow(fTarget,
				message->what == kMessageOpenArtist);
			if (message->what == kMessageOpenArtist)
				window->LoadArtist(id, "");
			else
				window->LoadAlbum(id, "");
			window->Show();
			return;
		}

		case kMessageDetailCovers:
		{
			const int32 count = fSongsList->CountRows(nullptr);
			for (int32 i = 0; i < count; i++) {
				auto* row = static_cast<SongRow*>(fSongsList->RowAt(i, nullptr));
				if (row == nullptr)
					continue;
				BBitmap* bitmap = BTranslationUtils::GetBitmapFile(
					ncm::CoverPath(row->Song().id).c_str());
				if (bitmap != nullptr) {
					row->SetCover(bitmap);
					fSongsList->InvalidateRow(row);
				}
			}
			return;
		}

		case kMessageDetailPlay:
		{
			if (fSongsList == nullptr)
				return;
			const int32 index = fSongsList->CountRows(nullptr) > 0
				? static_cast<int32>(fSongsList->IndexOf(
					fSongsList->CurrentSelection()))
				: -1;
			if (index < 0)
				return;

			// 让主窗口把这批歌当队列，并播放选中的那首
			BMessage play(kMessagePlaySongs);
			play.AddPointer("source", this);
			play.AddInt32("index", index);
			fTarget.SendMessage(&play);
			return;
		}

		case kMessageDetailAlbumOpen:
		{
			if (fAlbumList == nullptr)
				return;
			const int32 index = fAlbumList->CurrentSelection();
			if (index < 0 || index >= static_cast<int32>(fAlbums.size()))
				return;

			auto* window = new DetailWindow(fTarget, false);
			window->LoadAlbum(fAlbums[index].id, fAlbums[index].name.c_str());
			window->Show();
			return;
		}

		case kMessageDetailFail:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK) {
				BAlert* alert = new BAlert(B_TRANSLATE("HaikuNCM"), text,
					B_TRANSLATE("好"), nullptr, nullptr, B_WIDTH_AS_USUAL,
					B_WARNING_ALERT);
				alert->Go();
			}
			return;
		}

		default:
			break;
	}

	BWindow::MessageReceived(message);
}

bool DetailWindow::QuitRequested()
{
	return true;
}

void DetailWindow::_FillSongs()
{
	fSongsList->Clear();
	for (const ncm::Song& song : fSongs)
		fSongsList->AddRow(new SongRow(song));
}

void DetailWindow::_FillAlbums()
{
	if (fAlbumList == nullptr)
		return;

	fAlbumList->MakeEmpty();
	for (const ncm::AlbumInfo& album : fAlbums) {
		BString label(album.name.c_str());
		if (!album.publishTime.empty())
			label << "  ·  " << album.publishTime.c_str();
		fAlbumList->AddItem(new BStringItem(label.String()));
	}
}
