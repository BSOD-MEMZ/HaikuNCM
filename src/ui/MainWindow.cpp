#include "MainWindow.h"

#include "net/HttpRangeSource.h"
#include "LoginWindow.h"
#include "PlaylistWindow.h"
#include "SeekSlider.h"
#include "SongRow.h"
#include "TransportButton.h"
#include "audio/Player.h"

#include <Alert.h>

#include <Catalog.h>
#include <Bitmap.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <MessageRunner.h>
#include <ScrollView.h>
#include <Slider.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslationUtils.h>

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <utility>


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MainWindow"

namespace {

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

std::string SessionPath()
{
	return SessionDir() + "/session.txt";
}

void SaveSession(const std::string& cookie, const std::string& nickname)
{
	FILE* file = fopen(SessionPath().c_str(), "w");
	if (file == nullptr)
		return;
	fprintf(file, "%s\n%s\n", nickname.c_str(), cookie.c_str());
	fclose(file);
}

void TrimLine(char* text)
{
	for (char* cursor = text; *cursor != '\0'; cursor++) {
		if (*cursor == '\n' || *cursor == '\r') {
			*cursor = '\0';
			return;
		}
	}
}

bool LoadSession(std::string* cookie, std::string* nickname)
{
	FILE* file = fopen(SessionPath().c_str(), "r");
	if (file == nullptr)
		return false;

	char nicknameBuffer[256] = { 0 };
	char cookieBuffer[2048] = { 0 };
	const bool ok = fgets(nicknameBuffer, sizeof(nicknameBuffer), file) != nullptr
		&& fgets(cookieBuffer, sizeof(cookieBuffer), file) != nullptr;
	fclose(file);
	if (!ok)
		return false;

	TrimLine(nicknameBuffer);
	TrimLine(cookieBuffer);

	*nickname = nicknameBuffer;
	*cookie = cookieBuffer;
	return !cookie->empty();
}

std::string FormatTime(double seconds)
{
	if (seconds < 0.0)
		seconds = 0.0;

	const int total = static_cast<int>(seconds);
	char buffer[16];
	snprintf(buffer, sizeof(buffer), "%d:%02d", total / 60, total % 60);
	return buffer;
}

} // namespace

MainWindow::MainWindow()
	: BWindow(BRect(80, 80, 880, 660), "HaikuNCM", B_TITLED_WINDOW,
		  B_QUIT_ON_WINDOW_CLOSE)
{
	fApi = new ncm::NeteaseApi();
	fPlayer = new ncm::Player();

	fMenuBar = new BMenuBar("menubar");
	_BuildMenu();

	fUserMenuBar = new BMenuBar("user-menubar");
	fUserMenu = new BMenu(B_TRANSLATE("未登录"));
	fUserMenu->AddItem(fUserItem);
	BMenuItem* logoutItem = new BMenuItem(B_TRANSLATE("退出登录"),
		new BMessage(kMessageLogout));
	logoutItem->SetEnabled(false);
	fUserMenu->AddItem(logoutItem);
	fLogoutItem = logoutItem;
	fUserMenuBar->AddItem(fUserMenu);
	fUserMenuBar->SetExplicitMaxSize(BSize(B_SIZE_UNSET,
		fMenuBar->MaxSize().height));

	// 登录菜单：照抄 HaikuDepot —— 主菜单栏 + 右侧等高的用户菜单栏
	fUserItem = new BMenuItem(B_TRANSLATE("登录…"), new BMessage(kMessageLogin));

	fSearchField = new BTextControl("search", B_TRANSLATE("搜索"), "",
		new BMessage(kMessageSearch));
	fSearchField->SetModificationMessage(nullptr);
	fSearchField->SetExplicitMinSize(BSize(280, B_SIZE_UNSET));
	fSearchButton = new BButton("search-button", B_TRANSLATE("搜索"),
		new BMessage(kMessageSearch));

	fResults = new SongListView();
	fResults->SetInvocationMessage(new BMessage(kMessagePlay));

	const float scale = be_plain_font->Size() / 12.0f;
	fResults->AddColumn(new SongTitleColumn(B_TRANSLATE("标题"),
		280 * scale, 160 * scale, 600 * scale), 0);
	fResults->AddColumn(new SongAlbumColumn(B_TRANSLATE("专辑"),
		180 * scale, 80 * scale, 500 * scale), 1);
	fResults->AddColumn(new SongHeartColumn(B_TRANSLATE("喜欢"),
		50 * scale, 40 * scale, 80 * scale), 2);
	fResults->AddColumn(new SongDurationColumn(B_TRANSLATE("时长"),
		70 * scale, 50 * scale, 120 * scale), 3);
	fResults->SetSortingEnabled(true);

	const uint32 topBottom = BControlLook::B_TOP_BORDER
		| BControlLook::B_BOTTOM_BORDER;
	fPrevButton = new TransportButton("prev", TransportButton::kPrevious,
		B_TRANSLATE("上一首"), new BMessage(kMessagePrev),
		BControlLook::B_LEFT_BORDER | topBottom);
	fPlayButton = new TransportButton("play", TransportButton::kPlay,
		B_TRANSLATE("播放"), new BMessage(kMessagePlay), topBottom);
	fNextButton = new TransportButton("next", TransportButton::kNext,
		B_TRANSLATE("下一首"), new BMessage(kMessageNext),
		BControlLook::B_RIGHT_BORDER | topBottom);

	fSeek = new SeekSlider("seek", new BMessage(kMessageSeek), 0, 1000);
	fSeek->SetExplicitMinSize(BSize(160, B_SIZE_UNSET));
	fSeek->SetEnabled(false);

	fTimeLabel = new BStringView("time", "0:00 / 0:00");

	fHeartButton = new TransportButton("like", TransportButton::kHeart,
		B_TRANSLATE("喜欢"), new BMessage(kMessageLike));
	fHeartButton->SetExplicitMaxSize(BSize(36, 28));

	fModeButton = new BButton("mode", B_TRANSLATE("顺序"),
		new BMessage(kMessageMode));
	fModeButton->SetExplicitMinSize(BSize(56, B_SIZE_UNSET));
	fModeButton->SetToolTip(B_TRANSLATE("播放模式"));

	fVolume = new BSlider("volume", nullptr, new BMessage(kMessageVolume), 0,
		100, B_HORIZONTAL);
	fVolume->SetValue(100);
	fVolume->SetExplicitMinSize(BSize(90, B_SIZE_UNSET));
	fVolume->SetExplicitMaxSize(BSize(120, B_SIZE_UNSET));
	fVolumeLabel = new BStringView("volume-label", "100%");
	fStatus = new BStringView("status", B_TRANSLATE("输入关键词开始搜索"));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.AddGroup(B_HORIZONTAL, 0)
			.Add(fMenuBar, 1.0f)
			.Add(fUserMenuBar, 0.0f)
		.End()
		.AddGroup(B_VERTICAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_INSETS)
			.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
				.Add(fSearchField)
				.Add(fSearchButton)
			.End()
			.Add(fResults)
			.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
				.AddGroup(B_HORIZONTAL, 0)
					.Add(fPrevButton)
					.Add(fPlayButton)
					.Add(fNextButton)
				.End()
				.Add(fSeek, 1.0f)
				.Add(fTimeLabel)
				.Add(fHeartButton)
				.Add(fModeButton)
				.Add(fVolume)
				.Add(fVolumeLabel)
			.End()
			.Add(fStatus)
		.End();

	SetSizeLimits(720, 4000, 400, 4000);
	ResizeTo(800, 580);
	CenterOnScreen();

	fTicker = new BMessageRunner(BMessenger(this), new BMessage(kMessageTick),
		500000);

	std::string cookie;
	std::string nickname;
	if (LoadSession(&cookie, &nickname) && !cookie.empty()) {
		fApi->SetCookie(cookie);
		BString label(B_TRANSLATE("使用 "));
		label << nickname.c_str();
		fUserMenu->Superitem()->SetLabel(label.String());
	}
}

MainWindow::~MainWindow()
{
	delete fPlayer;
	delete fApi;
}

void MainWindow::_BuildMenu()
{
	BMenu* fileMenu = new BMenu(B_TRANSLATE("文件"));
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("搜索…"),
		new BMessage(kMessageFocusSearch), 'F'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("退出"), new BMessage(B_QUIT_REQUESTED), 'Q'));

	BMenu* musicMenu = new BMenu(B_TRANSLATE("音乐"));
	BMenuItem* myMusic = new BMenuItem(B_TRANSLATE("我的音乐"), new BMessage(kMessageStatus));
	myMusic->SetEnabled(false);
	musicMenu->AddItem(myMusic);
	BMenuItem* recent = new BMenuItem(B_TRANSLATE("最近播放"), new BMessage(kMessageStatus));
	recent->SetEnabled(false);
	musicMenu->AddItem(recent);
	musicMenu->AddSeparatorItem();
	musicMenu->AddItem(new BMenuItem(B_TRANSLATE("播放列表…"),
		new BMessage(kMessagePlaylist), 'L'));

	BMenu* helpMenu = new BMenu(B_TRANSLATE("帮助"));
	helpMenu->AddItem(new BMenuItem(B_TRANSLATE("关于 HaikuNCM"), new BMessage(kMessageAbout)));

	fMenuBar->AddItem(fileMenu);
	fMenuBar->AddItem(musicMenu);
	fMenuBar->AddItem(helpMenu);
}

void MainWindow::PlayIndex(int32 index)
{
	// 搜索是异步的，结果没回来前先记下，等 _FillResults 后再播
	fPendingPlay = index;
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
		case kMessageFocusSearch:
			fSearchField->MakeFocus(true);
			return;

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
		{
			if (fBusy)
				return;
			auto* row = static_cast<SongRow*>(fResults->CurrentSelection());
			const int32 target = row != nullptr
				? _IndexOfSong(row->Song().id) : -1;
			if (target < 0)
				return;
			if (fPlayer->IsLoaded() && target == fCurrentIndex)
				_TogglePlayback();
			else
				_StartPlay(target);
			return;
		}

		case kMessagePrev:
		{
			int32 target;
			fLock.Lock();
			target = fCurrentIndex > 0
				? fCurrentIndex - 1 : static_cast<int32>(fSongs.size()) - 1;
			fLock.Unlock();
			if (target >= 0)
				_StartPlay(target);
			return;
		}

		case kMessageNext:
		{
			fLock.Lock();
			const int32 count = static_cast<int32>(fSongs.size());
			fLock.Unlock();
			if (count <= 0)
				return;

			int32 target = 0;
			if (fPlayMode == 1)
				target = fCurrentIndex >= 0 ? fCurrentIndex : 0;
			else if (fPlayMode == 2)
				target = rand() % count;
			else if (fCurrentIndex >= 0 && fCurrentIndex + 1 < count)
				target = fCurrentIndex + 1;
			_StartPlay(target);
			return;
		}

		case kMessagePlayDone:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				fStatus->SetText(text);

			BPositionIO* source = nullptr;
			if (message->FindPointer("source", reinterpret_cast<void**>(&source)) == B_OK
				&& source != nullptr) {
				std::string streamError;
				std::string error;
				if (!fPlayer->LoadStream(source, &streamError)) {
					fStatus->SetText(Prefixed(B_TRANSLATE("解码失败: "), streamError));
				} else {
					fSeek->SetEnabled(true);
					if (fPlayer->Play(&error)) {
						fPlayButton->SetKind(TransportButton::kPause);
						_SyncPlaylist();
					} else {
						fPlayButton->SetKind(TransportButton::kPlay);
						fStatus->SetText(Prefixed(B_TRANSLATE("没有音频输出设备: "), error));
					}
					_UpdateTransport();
				}
				fBusy = false;
				return;
			}

			int32 doneIndex = -1;
			message->FindInt32("index", &doneIndex);
			if (doneIndex >= 0)
				fCurrentIndex = doneIndex;

			const char* path = nullptr;
			if (message->FindString("path", &path) == B_OK) {

				std::string error;
				if (!fPlayer->Load(path, &error)) {
					fStatus->SetText(Prefixed(B_TRANSLATE("解码失败: "), error));
				} else {
					fSeek->SetEnabled(true);
					if (fPlayer->Play(&error)) {
						fPlayButton->SetKind(TransportButton::kPause);

						// fee=1 的歌服务器只给 30s 试听片段，提示一下
						fLock.Lock();
						const bool vipPreview =
							static_cast<size_t>(fCurrentIndex) < fSongs.size()
							&& fSongs[fCurrentIndex].fee == 1;
						fLock.Unlock();
						if (vipPreview) {
							BString note(B_TRANSLATE("正在播放"));
							note << B_TRANSLATE("（VIP 试听 30 秒）");
							fStatus->SetText(note.String());
						}
						_SyncPlaylist();
					} else {
						fPlayButton->SetKind(TransportButton::kPlay);
						fStatus->SetText(Prefixed(B_TRANSLATE("没有音频输出设备: "), error));
					}
					_UpdateTransport();
				}
			}
			fBusy = false;
			return;
		}

		case kMessageStatus:
		{
			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				fStatus->SetText(text);
			return;
		}

		case kMessageSeek:
		{
			if (!fPlayer->IsLoaded())
				return;

			const double target = fSeek->Value() / 1000.0
				* fPlayer->DurationSeconds();
			std::string error;
			if (!fPlayer->Seek(target, &error))
				fStatus->SetText(Prefixed(B_TRANSLATE("定位失败: "), error));
			_UpdateTransport();
			return;
		}

		case kMessageTick:
			if (!fPlayer->IsPlaying())
				return;

			if (fSeekDirty && !fSeek->IsTracking()) {
				fSeekDirty = false;
				const double target = fSeekTarget / 1000.0
					* fPlayer->DurationSeconds();
				std::string seekError;
				if (!fPlayer->Seek(target, &seekError))
					fStatus->SetText(Prefixed(B_TRANSLATE("定位失败: "), seekError));
			}

			if (fPlayer->IsFinished()) {
				// 到曲末：停输出、复位按钮，再按模式决定要不要接下一首
				fPlayer->Stop();
				fPlayButton->SetKind(TransportButton::kPlay);
				fSeek->SetValue(1000);
				fTimeLabel->SetText(BString(FormatTime(fPlayer->DurationSeconds()).c_str()));

				fLock.Lock();
				const bool hasSongs = !fSongs.empty();
				fLock.Unlock();
				if (fPlayMode != 1 && hasSongs)
					PostMessage(kMessageNext);
				return;
			}

			_UpdateTransport();
			return;

		case kMessageVolume:
		{
			const float gain = fVolume->Value() / 100.0f;
			fPlayer->SetGain(gain);
			BString label;
			label << static_cast<int32>(fVolume->Value()) << "%";
			fVolumeLabel->SetText(label.String());
			return;
		}

		case kMessageMode:
			_CyclePlayMode();
			return;

		case kMessageLike:
			_ToggleLike();
			return;

		case kMessagePlaylist:
			_OpenPlaylist();
			return;

		case kMessagePlayNext:
		{
			auto* row = static_cast<SongRow*>(fResults->CurrentSelection());
			if (row == nullptr)
				return;
			const ncm::Song song = row->Song();

			fLock.Lock();
			const bool playing = fCurrentIndex >= 0
				&& static_cast<size_t>(fCurrentIndex) < fSongs.size();
			const int32 insertAt = playing ? fCurrentIndex + 1 : 0;
			fSongs.insert(fSongs.begin() + insertAt, song);
			const long long currentId = playing ? fSongs[fCurrentIndex].id : -1;
			fLock.Unlock();

			if (!playing) {
				_StartPlay(_IndexOfSong(song.id));
				return;
			}

			// 主列表：插到当前行后面，两份顺序保持一致
			const int32 rowCount = fResults->CountRows(nullptr);
			for (int32 i = 0; i < rowCount; i++) {
				auto* r = static_cast<SongRow*>(fResults->RowAt(i, nullptr));
				if (r != nullptr && r->Song().id == currentId) {
					fResults->AddRow(new SongRow(song), i + 1);
					break;
				}
			}

			_SyncPlaylist();
			BString status(B_TRANSLATE("已加入下一首播放: "));
			status << song.name.c_str();
			fStatus->SetText(status.String());
			return;
		}

		case kMessageQueueReorder:
		{
			// 播放列表窗口拖动排序：按歌 id 顺序重建 fSongs
			const int32 count = message->FindInt32("count");
			std::vector<long long> order;
			order.reserve(count);
			for (int32 i = 0; i < count; i++) {
				int64 id = 0;
				message->FindInt64("id", i, &id);
				order.push_back(static_cast<long long>(id));
			}

			fLock.Lock();
			std::vector<ncm::Song> reordered;
			for (long long id : order) {
				for (const ncm::Song& song : fSongs) {
					if (song.id == id) {
						reordered.push_back(song);
						break;
					}
				}
			}
			fSongs.swap(reordered);
			const long long currentId = fCurrentIndex >= 0
				&& static_cast<size_t>(fCurrentIndex) < fSongs.size()
				? fSongs[fCurrentIndex].id : -1;
			fLock.Unlock();

			// 当前曲目索引跟着重排
			fCurrentIndex = _IndexOfSong(currentId);
			return;
		}

		case kMessageQueueDelete:
		{
			int32 deleteIndex = -1;
			if (message->FindInt32("index", &deleteIndex) != B_OK
				|| deleteIndex < 0)
				return;

			fLock.Lock();
			const bool removingCurrent = deleteIndex == fCurrentIndex;
			fSongs.erase(fSongs.begin() + deleteIndex);
			fLock.Unlock();

			if (removingCurrent)
				fCurrentIndex = -1;
			else if (deleteIndex < fCurrentIndex)
				fCurrentIndex--;
			return;
		}

		case kMessagePlayIndex:
		{
			int32 index = -1;
			if (message->FindInt32("index", &index) == B_OK)
				_StartPlay(index);
			return;
		}

		case kMessageLogin:
			(new LoginWindow(BMessenger(this)))->Show();
			return;

		case kMessageLoginDone:
		{
			const char* nickname = nullptr;
			const char* cookie = nullptr;
			if (message->FindString("nickname", &nickname) == B_OK
				&& message->FindString("cookie", &cookie) == B_OK) {
				fApi->SetCookie(cookie);
				SaveSession(cookie, nickname);

				BString label("使用 ");
				label << nickname << " 登录";
				fUserItem->SetLabel(label.String());
				fUserItem->SetEnabled(false);

				BString status("已登录: ");
				status << nickname;
				fStatus->SetText(status.String());
			}
			return;
		}

		case kMessageLogout:
			fApi->SetCookie(std::string());
			remove(SessionPath().c_str());
			fUserMenu->Superitem()->SetLabel(B_TRANSLATE("未登录"));
			fStatus->SetText(B_TRANSLATE("已退出登录"));
			return;

		case kMessageAbout:
		{
			BAlert* alert = new BAlert("关于 HaikuNCM",
				"HaikuNCM 1.0\n\nHaiku 原生第三方网易云音乐客户端。\n"
				"界面与播放全部使用 Haiku 的 Interface Kit / Media Kit。\n\n"
				"协议参考 NeteaseCloudMusicApi，代码为独立实现。",
				"好");
			alert->Go();
			return;
		}

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
		fStatus->SetText(B_TRANSLATE("先输入要搜的歌名"));
		return;
	}

	fKeyword = text;
	fBusy = true;
	fStatus->SetText(B_TRANSLATE("搜索中…"));
	fSearchButton->SetEnabled(false);

	thread_id thread = spawn_thread(_SearchThread, "ncm-search",
		B_NORMAL_PRIORITY, this);
	if (thread < B_OK) {
		fBusy = false;
		fStatus->SetText(B_TRANSLATE("无法创建搜索线程"));
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
		BMessage failure(kMessageStatus);
		failure.AddString("text", Prefixed(B_TRANSLATE("搜索失败: "), error));
		self->_PostIfAlive(&failure);
	}
	return 0;
}

void MainWindow::_FillResults()
{
	fPlayer->Stop();
	fResults->Clear();
	fCurrentIndex = -1;
	fPlayButton->SetKind(TransportButton::kPlay);
	fSeek->SetValue(0);
	fSeek->SetEnabled(false);
	fTimeLabel->SetText("0:00 / 0:00");

	fLock.Lock();
	const size_t count = fSongs.size();
	for (size_t i = 0; i < count; i++)
		fResults->AddRow(new SongRow(fSongs[i]));
	fLock.Unlock();

	fBusy = false;
	fSearchButton->SetEnabled(true);

	BString status;
	status << "找到 " << count << " 首（双击播放）";
	fStatus->SetText(status.String());

	_SyncPlaylist();
	_StartCoverDownload();

	if (fPendingPlay >= 0) {
		const int32 target = fPendingPlay;
		fPendingPlay = -1;
		_StartPlay(target);
	}
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
	const int32 count = fResults->CountRows(nullptr);
	for (int32 i = 0; i < count; i++) {
		auto* row = static_cast<SongRow*>(fResults->RowAt(i, nullptr));
		if (row == nullptr)
			continue;

		const std::string path = CoverPath(row->Song().id);
		BBitmap* bitmap = BTranslationUtils::GetBitmapFile(path.c_str());
		if (bitmap != nullptr) {
			row->SetCover(bitmap);
			fResults->InvalidateRow(row);
		}
	}
}

void MainWindow::_StartPlay(int32 index)
{
	if (fBusy)
		return;

	fLock.Lock();
	const bool valid = index >= 0 && static_cast<size_t>(index) < fSongs.size();
	fLock.Unlock();

	if (!valid) {
		fStatus->SetText(B_TRANSLATE("先在列表里选一首歌"));
		return;
	}

	{
		fLock.Lock();
		const long long songId = fSongs[index].id;
		fLock.Unlock();

		fResults->DeselectAll();
		const int32 rowCount = fResults->CountRows(nullptr);
		for (int32 i = 0; i < rowCount; i++) {
			auto* row = static_cast<SongRow*>(fResults->RowAt(i, nullptr));
			if (row != nullptr && row->Song().id == songId) {
				fResults->AddToSelection(row);
				fResults->ScrollTo(row);
				break;
			}
		}
	}
	fBusy = true;
	fStatus->SetText(B_TRANSLATE("正在获取播放地址…"));

	auto* payload = new std::pair<MainWindow*, int32>(this, index);
	thread_id thread = spawn_thread(_PlayThread, "ncm-play", B_NORMAL_PRIORITY,
		payload);
	if (thread < B_OK) {
		delete payload;
		fBusy = false;
		fStatus->SetText(B_TRANSLATE("无法创建播放线程"));
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
		result.AddString("text", Prefixed(B_TRANSLATE("取流失败: "), error));
		self->_PostIfAlive(&result);
		return 0;
	}

	const std::string path = AudioPath(song.id, audio.type);
	struct stat info;
	const bool cached = stat(path.c_str(), &info) == 0
		&& info.st_size == audio.size;

	if (cached) {
		result.AddString("path", path.c_str());
		self->_PostIfAlive(&result);
		return 0;
	}

	// 流式优先：BMediaFile 直接吃 HTTP Range 数据源，边下边播
	auto* source = new HttpRangeSource(audio.url, audio.size, &error);
	if (source->Ok()) {
		result.AddPointer("source", source);
		self->_PostIfAlive(&result);
		return 0;
	}
	delete source;

	// 回退：CDN 不支持 Range（少见）就整曲下载
	if (!self->fApi->DownloadAudio(audio, path, &error)) {
		result.AddString("text", Prefixed(B_TRANSLATE("下载失败: "), error));
		self->_PostIfAlive(&result);
		return 0;
	}

	result.AddInt32("index", index);
	result.AddString("path", path.c_str());
	self->_PostIfAlive(&result);
	return 0;
}

void MainWindow::_TogglePlayback()
{
	if (!fPlayer->IsLoaded())
		return;

	std::string error;
	if (fPlayer->IsPlaying()) {
		fPlayer->Stop();
		fPlayButton->SetKind(TransportButton::kPlay);
		return;
	}

	if (fPlayer->Play(&error))
		fPlayButton->SetKind(TransportButton::kPause);
	else
		fStatus->SetText(Prefixed(B_TRANSLATE("没有音频输出设备: "), error));
}

void MainWindow::_CyclePlayMode()
{
	fPlayMode = (fPlayMode + 1) % 3;
	_UpdateModeLabel();
}

void MainWindow::_UpdateModeLabel()
{
	static const char* labels[3] = {
		B_TRANSLATE("顺序"),
		B_TRANSLATE("单曲"),
		B_TRANSLATE("随机"),
	};
	fModeButton->SetLabel(labels[fPlayMode]);
}

void MainWindow::_ToggleLike()
{
	if (fCurrentIndex < 0) {
		fStatus->SetText(B_TRANSLATE("先播放一首歌"));
		return;
	}

	fLock.Lock();
	const long long id = static_cast<size_t>(fCurrentIndex) < fSongs.size()
		? fSongs[fCurrentIndex].id
		: 0;
	fLock.Unlock();
	if (id == 0)
		return;

	const bool target = !fLiked;
	std::string error;
	if (!fApi->LikeSong(id, target, &error)) {
		fStatus->SetText(Prefixed(B_TRANSLATE("喜欢失败: "), error));
		return;
	}

	fLiked = target;
	fHeartButton->SetKind(fLiked ? TransportButton::kHeartFilled
								 : TransportButton::kHeart);
	fHeartButton->Invalidate();
	fStatus->SetText(fLiked ? B_TRANSLATE("已加入我喜欢的音乐")
							: B_TRANSLATE("已从我喜欢的音乐移除"));
}

void MainWindow::_OpenPlaylist()
{
	fLock.Lock();
	std::vector<ncm::Song> songs = fSongs;
	fLock.Unlock();

	if (fPlaylistWindow == nullptr)
		fPlaylistWindow = new PlaylistWindow(BMessenger(this));
	fPlaylistWindow->Update(songs, fCurrentIndex);
	fPlaylistWindow->Show();
	fPlaylistWindow->Activate();
}

int32 MainWindow::_IndexOfSong(long long id)
{
	if (id < 0)
		return -1;
	fLock.Lock();
	for (size_t i = 0; i < fSongs.size(); i++) {
		if (fSongs[i].id == id) {
			fLock.Unlock();
			return static_cast<int32>(i);
		}
	}
	fLock.Unlock();
	return -1;
}

void MainWindow::_SyncPlaylist()
{
	if (fPlaylistWindow == nullptr)
		return;

	fLock.Lock();
	std::vector<ncm::Song> songs = fSongs;
	fLock.Unlock();

	fPlaylistWindow->Update(songs, fCurrentIndex);
}

void MainWindow::_UpdateTransport()
{
	if (!fPlayer->IsLoaded()) {
		fTimeLabel->SetText("0:00 / 0:00");
		return;
	}

	const double duration = fPlayer->DurationSeconds();
	const double position = fPlayer->PositionSeconds();

	if (!fSeek->IsTracking()) {
		const int32 value = duration > 0.0
			? static_cast<int32>(position / duration * 1000.0)
			: 0;
		fSeek->SetValue(value);
	}

	BString label(FormatTime(position).c_str());
	label << " / " << FormatTime(duration).c_str();
	fTimeLabel->SetText(label.String());
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
