#include "MainWindow.h"

#include "ListMetrics.h"
#include "SongHeader.h"
#include "SongItem.h"
#include "audio/Player.h"

#include <Bitmap.h>
#include <Button.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslationUtils.h>

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <utility>

using namespace ncm::metrics;

namespace {

const uint32 kSelectionChanged = 'slch';

BString Prefixed(const char* prefix, const std::string& detail)
{
	BString text(prefix);
	text << detail.c_str();
	return text;
}

std::string EnsureDir(const std::string& path)
{
	mkdir(path.c_str(), 0755);
	return path;
}

std::string SessionDir()
{
	const char* home = getenv("HOME");
	if (home == nullptr)
		home = "/boot/home";
	return EnsureDir(std::string(home) + "/HaikuNCM");
}

std::string CacheDir()
{
	return EnsureDir(SessionDir() + "/cache");
}

std::string CoverDir()
{
	return EnsureDir(CacheDir() + "/covers");
}

std::string CoverPath(long long id)
{
	return CoverDir() + "/" + std::to_string(id) + ".jpg";
}

std::string AudioPath(long long id, const std::string& type)
{
	return CacheDir() + "/" + std::to_string(id) + "."
		+ (type.empty() ? "mp3" : type);
}

BStringView* MakeSidebarEntry(const char* label, bool highlight = false)
{
	auto* view = new BStringView("entry", label);
	BFont font(be_plain_font);
	font.SetSize(12.5f);
	if (highlight)
		font.SetFace(B_BOLD_FACE);
	view->SetFont(&font);
	view->SetHighColor(highlight ? Accent() : PrimaryText());
	view->SetExplicitMinSize(BSize(B_SIZE_UNSET, 30.0f));
	return view;
}

} // namespace

MainWindow::MainWindow()
	: BWindow(BRect(80, 80, 900, 660), "HaikuNCM", B_TITLED_WINDOW,
		  B_QUIT_ON_WINDOW_CLOSE)
{
	fApi = new ncm::NeteaseApi();

	fMenuBar = new BMenuBar("menubar");
	BMenu* fileMenu = new BMenu("文件");
	fileMenu->AddItem(new BMenuItem("退出", new BMessage(B_QUIT_REQUESTED), 'Q'));
	fMenuBar->AddItem(fileMenu);

	fSidebar = new BView("sidebar", B_WILL_DRAW);
	fSidebar->SetViewColor((rgb_color){ 251, 251, 251, 255 });
	fSidebar->SetExplicitMinSize(BSize(168, B_SIZE_UNSET));
	fSidebar->SetExplicitMaxSize(BSize(168, B_SIZE_UNLIMITED));
	_BuildSidebar();

	fSearchField = new BTextControl("search", nullptr, "",
		new BMessage(kMessageSearch));
	fSearchField->SetModificationMessage(nullptr);
	fSearchField->SetExplicitMinSize(BSize(320, B_SIZE_UNSET));
	fSearchButton = new BButton("search-button", "搜索",
		new BMessage(kMessageSearch));

	fHeader = new SongHeader();
	fResults = new BListView("results", B_SINGLE_SELECTION_LIST);
	fResults->SetViewColor(RowBackground());
	fResults->SetSelectionMessage(new BMessage(kSelectionChanged));
	fResults->SetInvocationMessage(new BMessage(kMessagePlay));
	fResults->SetExplicitMinSize(BSize(560, 300));

	fNowPlaying = new BStringView("now-playing", "未播放");
	fPlayButton = new BButton("play-button", "播放", new BMessage(kMessagePlay));
	fStatus = new BStringView("status", "输入关键词开始搜索");

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.AddGroup(B_HORIZONTAL, 0)
			.Add(fSidebar)
			.AddGroup(B_VERTICAL, B_USE_DEFAULT_SPACING)
				.SetInsets(B_USE_WINDOW_INSETS)
				.AddGroup(B_HORIZONTAL, 8.0f)
					.Add(fSearchField)
					.Add(fSearchButton)
				.End()
				.Add(fHeader)
				.Add(new BScrollView("scroll", fResults, 0, false, true))
				.AddGroup(B_HORIZONTAL, 8.0f)
					.Add(fNowPlaying, 1.0f)
					.Add(fPlayButton)
				.End()
				.Add(fStatus)
			.End()
		.End();

	SetSizeLimits(760, 4000, 420, 4000);
	ResizeTo(820, 580);
	CenterOnScreen();
}

MainWindow::~MainWindow()
{
	delete fApi;
}

void MainWindow::_BuildSidebar()
{
	auto* title = new BStringView("brand", "HaikuNCM");
	BFont titleFont(be_plain_font);
	titleFont.SetSize(16.0f);
	titleFont.SetFace(B_BOLD_FACE);
	title->SetFont(&titleFont);
	title->SetHighColor(Accent());

	BLayoutBuilder::Group<>(fSidebar, B_VERTICAL, 2.0f)
		.SetInsets(18.0f, 16.0f, 18.0f, 12.0f)
		.Add(title)
		.AddStrut(10.0f)
		.Add(MakeSidebarEntry("推荐", true))
		.Add(MakeSidebarEntry("精选"))
		.Add(MakeSidebarEntry("播客"))
		.Add(MakeSidebarEntry("漫游"))
		.AddStrut(14.0f)
		.Add(MakeSidebarEntry("我的音乐"))
		.Add(MakeSidebarEntry("最近播放"))
		.AddGlue()
		.Add(MakeSidebarEntry("搜索"))
		.End();
}

void MainWindow::SearchFor(const char* keyword)
{
	if (keyword == nullptr)
		return;

	// 可能来自其他线程，控件只能在自己的窗口线程里碰，走消息
	BMessage message(kMessageSearch);
	message.AddString("keyword", keyword);
	PostMessage(&message);
}

void MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMessageSearch:
		{
			const char* keyword = nullptr;
			if (message->FindString("keyword", &keyword) == B_OK
				&& keyword != nullptr)
				fSearchField->SetText(keyword);
			_StartSearch();
			return;
		}

		case kMessageSearchDone:
			_FillResults();
			return;

		case kMessageCoversDone:
			_ApplyCovers();
			return;

		case kMessagePlay:
			_StartPlay(fResults->CurrentSelection());
			return;

		case kMessagePlayDone:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				fStatus->SetText(text);
			fBusy = false;
			break;
		}

		case kMessageStatus:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				fStatus->SetText(text);
			return;
		}

		case kSelectionChanged:
			_UpdateSelectionLabel();
			break;

		default:
			BWindow::MessageReceived(message);
			return;
	}
}

bool MainWindow::QuitRequested()
{
	fQuitting = true;
	return true;
}

void MainWindow::_StartSearch()
{
	if (fBusy)
		return;

	const char* text = fSearchField->Text();
	if (text == nullptr || text[0] == '\0') {
		fStatus->SetText("先输入要搜的歌名");
		return;
	}

	fKeyword = text;
	fBusy = true;
	fStatus->SetText("搜索中…");
	fSearchButton->SetEnabled(false);

	thread_id thread = spawn_thread(_SearchThread, "ncm-search",
		B_NORMAL_PRIORITY, this);
	if (thread < B_OK) {
		fBusy = false;
		fStatus->SetText("无法创建搜索线程");
		return;
	}
	resume_thread(thread);
}

int32 MainWindow::_SearchThread(void* data)
{
	auto* self = static_cast<MainWindow*>(data);

	std::vector<ncm::Song> songs;
	std::string error;
	const BString keyword = self->fKeyword;

	const bool ok = self->fApi->Search(keyword.String(), 30, &songs, &error);

	if (ok) {
		self->fLock.Lock();
		self->fSongs.swap(songs);
		self->fLock.Unlock();

		BMessage done(kMessageSearchDone);
		self->_PostIfAlive(&done);
	} else {
		BMessage failure(kMessagePlayDone);
		failure.AddString("text", Prefixed("搜索失败: ", error));
		self->_PostIfAlive(&failure);
	}
	return 0;
}

void MainWindow::_FillResults()
{
	fResults->MakeEmpty();

	fLock.Lock();
	const size_t count = fSongs.size();
	for (size_t i = 0; i < count; i++)
		fResults->AddItem(new SongItem(fSongs[i], static_cast<int32>(i)));
	fLock.Unlock();

	fBusy = false;
	fSearchButton->SetEnabled(true);

	BString status;
	status << "找到 " << count << " 首（双击播放）";
	fStatus->SetText(status.String());

	_StartCoverDownload();
}

void MainWindow::_StartCoverDownload()
{
	thread_id thread = spawn_thread(_CoverThread, "ncm-covers", B_LOW_PRIORITY,
		this);
	if (thread >= B_OK)
		resume_thread(thread);
}

int32 MainWindow::_CoverThread(void* data)
{
	auto* self = static_cast<MainWindow*>(data);

	std::vector<ncm::Song> songs;
	self->fLock.Lock();
	songs = self->fSongs;
	self->fLock.Unlock();

	for (const ncm::Song& song : songs) {
		if (song.coverUrl.empty())
			continue;

		const std::string path = CoverPath(song.id);
		struct stat info;
		if (stat(path.c_str(), &info) == 0 && info.st_size > 1024)
			continue;

		std::string error;
		self->fApi->DownloadCover(song.coverUrl, path, &error);
	}

	BMessage done(kMessageCoversDone);
	self->_PostIfAlive(&done);
	return 0;
}

void MainWindow::_ApplyCovers()
{
	const int32 count = fResults->CountItems();
	for (int32 i = 0; i < count; i++) {
		auto* item = dynamic_cast<SongItem*>(fResults->ItemAt(i));
		if (item == nullptr)
			continue;

		const std::string path = CoverPath(item->Song().id);
		BBitmap* bitmap = BTranslationUtils::GetBitmapFile(path.c_str());
		if (bitmap != nullptr)
			item->SetCover(bitmap);
	}

	fResults->Invalidate();
}

void MainWindow::_UpdateSelectionLabel()
{
	const int32 index = fResults->CurrentSelection();
	if (index < 0) {
		fNowPlaying->SetText("未播放");
		return;
	}

	fLock.Lock();
	if (static_cast<size_t>(index) < fSongs.size()) {
		const ncm::Song& song = fSongs[index];
		BString text;
		text << song.name.c_str() << "  ·  " << song.artist.c_str();
		fNowPlaying->SetText(text.String());
	}
	fLock.Unlock();
}

void MainWindow::_StartPlay(int32 index)
{
	if (fBusy)
		return;

	fLock.Lock();
	const bool valid = index >= 0 && static_cast<size_t>(index) < fSongs.size();
	fLock.Unlock();

	if (!valid) {
		fStatus->SetText("先在列表里选一首歌");
		return;
	}

	fBusy = true;
	fStatus->SetText("正在获取播放地址…");

	auto* payload = new std::pair<MainWindow*, int32>(this, index);
	thread_id thread = spawn_thread(_PlayThread, "ncm-play", B_NORMAL_PRIORITY,
		payload);
	if (thread < B_OK) {
		delete payload;
		fBusy = false;
		fStatus->SetText("无法创建播放线程");
		return;
	}
	resume_thread(thread);
}

int32 MainWindow::_PlayThread(void* data)
{
	auto* payload = static_cast<std::pair<MainWindow*, int32>*>(data);
	MainWindow* self = payload->first;
	const int32 index = payload->second;
	delete payload;

	self->fLock.Lock();
	const ncm::Song song = self->fSongs[index];
	self->fLock.Unlock();

	BMessage result(kMessagePlayDone);

	ncm::AudioUrl audio;
	std::string error;
	if (!self->fApi->SongUrl(song.id, "standard", &audio, &error)) {
		result.AddString("text", Prefixed("取流失败: ", error));
		self->_PostIfAlive(&result);
		return 0;
	}

	const std::string path = AudioPath(song.id, audio.type);
	struct stat info;
	const bool cached = stat(path.c_str(), &info) == 0
		&& info.st_size == audio.size;
	if (!cached) {
		if (!self->fApi->DownloadAudio(audio, path, &error)) {
			result.AddString("text", Prefixed("下载失败: ", error));
			self->_PostIfAlive(&result);
			return 0;
		}
	}

	ncm::Player player;
	if (!player.Load(path, &error)) {
		result.AddString("text", Prefixed("解码失败: ", error));
		self->_PostIfAlive(&result);
		return 0;
	}

	const std::string description = player.Description();
	if (!player.Play(&error)) {
		BString text;
		text << song.name.c_str() << " 已就绪（" << description.c_str()
			 << "），但" << error.c_str();
		result.AddString("text", text.String());
		self->_PostIfAlive(&result);
		return 0;
	}

	BString text;
	text << "正在播放: " << song.name.c_str() << "（" << description.c_str()
		 << "）";
	result.AddString("text", text.String());
	self->_PostIfAlive(&result);

	snooze(static_cast<bigtime_t>(300 * 1000 * 1000));
	return 0;
}

void MainWindow::_PostIfAlive(BMessage* message)
{
	if (fQuitting)
		return;

	if (Lock()) {
		PostMessage(message);
		Unlock();
	}
}
