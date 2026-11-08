#include "MainWindow.h"

#include "AvatarGrid.h"
#include "net/cachepaths.h"
#include "net/HttpRangeSource.h"
#include "DetailWindow.h"
#include "LoginWindow.h"
#include "CommentsWindow.h"
#include "NowPlayingPage.h"
#include "ThemeColor.h"
#include "PlaylistWindow.h"
#include "SeekSlider.h"
#include "SongRow.h"
#include "TransportButton.h"
#include "audio/Player.h"

#include <Alert.h>
#include <Bitmap.h>
#include <Button.h>
#include <CardView.h>
#include <Catalog.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <ScrollView.h>
#include <Slider.h>
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <TranslationUtils.h>

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <utility>


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MainWindow"

namespace {

using ncm::AudioPath;
using ncm::BigCoverPath;
using ncm::CommentsPath;
using ncm::RomajiPath;
using ncm::TranslationPath;
using ncm::CacheDir;
using ncm::CoverPath;
using ncm::LyricPath;
using ncm::SessionPath;

// tab 顺序与网易云聚合搜索 type 一一对应
const int32 kTabCount = 6;
const int32 kTabTypes[kTabCount] = {1, 1000, 10, 100, 1002, 1009};

BString Prefixed(const char* prefix, const std::string& detail)
{
	BString text(prefix);
	text << detail.c_str();
	return text;
}

void SaveSession(const std::string& cookie, const std::string& nickname,
	long long userId)
{
	FILE* file = fopen(SessionPath().c_str(), "w");
	if (file == nullptr)
		return;
	fprintf(file, "%s\n%s\n%lld\n", nickname.c_str(), cookie.c_str(),
		userId);
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

bool LoadSession(std::string* cookie, std::string* nickname,
	long long* userId)
{
	FILE* file = fopen(SessionPath().c_str(), "r");
	if (file == nullptr)
		return false;

	char nicknameBuffer[256] = { 0 };
	char cookieBuffer[2048] = { 0 };
	char uidBuffer[64] = { 0 };
	const bool ok = fgets(nicknameBuffer, sizeof(nicknameBuffer), file) != nullptr
		&& fgets(cookieBuffer, sizeof(cookieBuffer), file) != nullptr;
	fgets(uidBuffer, sizeof(uidBuffer), file); // 老版本没有第三行
	fclose(file);
	if (!ok)
		return false;

	TrimLine(nicknameBuffer);
	TrimLine(cookieBuffer);
	TrimLine(uidBuffer);

	*nickname = nicknameBuffer;
	*cookie = cookieBuffer;
	if (userId != nullptr)
		*userId = atoll(uidBuffer);
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

struct GenericJob {
	MainWindow* window;
	int32 slot;
	BString keyword;
};

// 播放任务：音质在起播那一刻定下来，线程里不再读窗口成员
struct PlayJob {
	MainWindow* window;
	int32 index;
	std::string level;
};

} // namespace

MainWindow::MainWindow()
	: BWindow(BRect(70, 60, 930, 700), "HaikuNCM", B_TITLED_WINDOW,
		  B_QUIT_ON_WINDOW_CLOSE)
{
	fApi = new ncm::NeteaseApi();
	fPlayer = new ncm::Player();

	fMenuBar = new BMenuBar("menubar");
	_BuildMenu();
	fUserMenuBar = new BMenuBar("user-menubar");
	fUserItem = new BMenuItem(B_TRANSLATE("登录…"), new BMessage(kMessageLogin));
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
	fSearchField = new BTextControl("search", B_TRANSLATE("搜索"), "",
		new BMessage(kMessageSearch));
	fSearchField->SetModificationMessage(nullptr);
	fSearchField->SetExplicitMinSize(BSize(280, B_SIZE_UNSET));
	fSearchButton = new BButton("search-button", B_TRANSLATE("搜索"),
		new BMessage(kMessageSearch));

	_BuildContent();
	_BuildTransport();

	// 布局权重：只有内容区（和里面的列表）吃多余空间，
	// 菜单栏行、搜索行、底部底板都是 0，窗口拉大才不会被它们抢走位置
	fSearchRow = new BGroupView("search-row", B_HORIZONTAL,
		B_USE_DEFAULT_SPACING);
	BLayoutBuilder::Group<>(fSearchRow, B_HORIZONTAL, B_USE_DEFAULT_SPACING)
		.Add(fSearchField)
		.Add(fSearchButton)
		.End();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.AddGroup(B_HORIZONTAL, 0, 0.0f)
			.Add(fMenuBar, 1.0f)
			.Add(fUserMenuBar, 0.0f)
			.End()
		.AddGroup(B_VERTICAL, B_USE_DEFAULT_SPACING, 1.0f)
			.SetInsets(B_USE_WINDOW_INSETS)
			.Add(fSearchRow, 0.0f)
			.Add(fContentHost, 1.0f)
			.End()
		.Add(fTransport, 0.0f)
		.End();

	SetSizeLimits(760, 4000, 420, 4000);
	ResizeTo(860, 640);
	CenterOnScreen();

	fTicker = new BMessageRunner(BMessenger(this), new BMessage(kMessageTick),
		500000);

	std::string cookie;
	std::string nickname;
	if (LoadSession(&cookie, &nickname, &fUserId) && !cookie.empty()) {
		fApi->SetCookie(cookie);
		BString label(B_TRANSLATE("使用 "));
		label << nickname.c_str();
		fUserMenu->Superitem()->SetLabel(label.String());
		fLogoutItem->SetEnabled(true);
		// 红心状态和歌单要联网拿，后台跑
		_FetchAccountInfo();
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
	BMenuItem* nowPlaying = new BMenuItem(B_TRANSLATE("正在播放"),
		new BMessage(kMessageToggleNowPlaying), 'P');
	musicMenu->AddItem(nowPlaying);
	musicMenu->AddSeparatorItem();

	fMyPlaylistMenu = new BMenu(B_TRANSLATE("我的歌单"));
	_RebuildPlaylistMenu();
	musicMenu->AddItem(fMyPlaylistMenu);

	fQualityMenu = new BMenu(B_TRANSLATE("音质"));
	_BuildQualityMenu(fQualityMenu);
	musicMenu->AddItem(fQualityMenu);

	musicMenu->AddSeparatorItem();
	musicMenu->AddItem(new BMenuItem(B_TRANSLATE("播放列表…"),
		new BMessage(kMessagePlaylist), 'L'));

	BMenu* helpMenu = new BMenu(B_TRANSLATE("帮助"));
	helpMenu->AddItem(new BMenuItem(B_TRANSLATE("关于 HaikuNCM"), new BMessage(kMessageAbout)));

	fMenuBar->AddItem(fileMenu);
	fMenuBar->AddItem(musicMenu);
	fMenuBar->AddItem(helpMenu);
}

void MainWindow::_BuildContent()
{
	// 目录 tab 直接压在列表上（HaikuDepot 的做法：无边框 BTabView），
	// 不再额外套一层"完整 tab 容器"，避免无谓留白。
	// 注意：显式传 flags 会顶掉 B_WILL_DRAW，tab 就整个画不出来
	fSearchTabs = new BTabView("search-tabs", B_WIDTH_FROM_LABEL);
	fSearchTabs->SetBorder(B_NO_BORDER);
	fSearchTabs->SetTabHeight(fSearchTabs->TabHeight() - 2.0f);
	fSearchTabs->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	fSearchTabs->SetExplicitMinSize(BSize(320, 200));

	fResults = new SongListView();
	fResults->SetInvocationMessage(new BMessage(kMessagePlay));
	AddSongColumns(fResults);

	BGroupView* songCard = new BGroupView("songs", B_VERTICAL, 0.0f);
	BLayoutBuilder::Group<>(songCard, B_VERTICAL, 0.0f)
		.Add(fResults, 1.0f)
		.End();
	BTab* songTab = new BTab();
	fSearchTabs->AddTab(songCard, songTab);
	songTab->SetLabel(B_TRANSLATE("单曲"));

	static const char* kTabNames[kTabCount] = {
		B_TRANSLATE("单曲"), B_TRANSLATE("歌单"), B_TRANSLATE("专辑"),
		B_TRANSLATE("歌手"), B_TRANSLATE("用户"), B_TRANSLATE("声音"),
	};

	for (int32 i = 1; i < kTabCount; i++) {
		const bool isGrid = i == 3 || i == 4; // 歌手 / 用户
		BView* content = nullptr;
		if (isGrid) {
			// 歌手双击开歌手页；用户没有详情页，只展示
			auto* grid = new AvatarGridView(
				i == 3 ? kMessageOpenArtist : 0);
			content = new BScrollView("grid-scroll", grid, 0, false, true);
			fAvatarGrids[i - 1 - 2] = grid;
		} else {
			// 第三个参数是 flags：别把 B_PLAIN_BORDER（值为 0）当 flags 传进去，
			// 那样 B_WILL_DRAW 被清掉，列表整个不画东西
			BListView* list = new BListView("generic",
				B_SINGLE_SELECTION_LIST,
				B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE);
			list->SetViewUIColor(B_LIST_BACKGROUND_COLOR);
			list->SetInvocationMessage(new BMessage(kMessageGenericOpen));
			fGenericLists[i - 1] = list;
			content = list;
		}

		BGroupView* card = new BGroupView("card", B_VERTICAL, 0.0f);
		BLayoutBuilder::Group<>(card, B_VERTICAL, 0.0f)
			.Add(content, 1.0f)
			.End();
		BTab* tab = new BTab();
		fSearchTabs->AddTab(card, tab);
		tab->SetLabel(kTabNames[i]);
	}

	fNowView = new NowPlayingView();

	fContentHost = new BCardView();
	fContentHost->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	fContentHost->CardLayout()->AddView(fSearchTabs);
	fContentHost->CardLayout()->AddView(fNowView);
	fContentHost->CardLayout()->SetVisibleItem(0);

	fTabIndex = 0;
	fSearchType = kTabTypes[0];
}

void MainWindow::_BuildTransport()
{
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
	fSeek->SetExplicitMinSize(BSize(120, B_SIZE_UNSET));
	fSeek->SetEnabled(false);

	fTimeLabel = new BStringView("time", "0:00 / 0:00");
	fTimeLabel->SetExplicitMinSize(BSize(76, B_SIZE_UNSET));

	fNowBadge = new NowPlayingBadge();

	fHeartButton = new TransportButton("like", TransportButton::kHeart,
		B_TRANSLATE("喜欢"), new BMessage(kMessageLike));
	fHeartButton->SetToolTip(B_TRANSLATE("喜欢"));

	fCommentButton = new TransportButton("comments", TransportButton::kComment,
		B_TRANSLATE("评论"), new BMessage(kMessageShowComments));
	fCommentButton->SetToolTip(B_TRANSLATE("查看评论"));

	fModeButton = new BButton("mode", B_TRANSLATE("顺序"),
		new BMessage(kMessageMode));
	fModeButton->SetExplicitMinSize(BSize(56, B_SIZE_UNSET));
	fModeButton->SetToolTip(B_TRANSLATE("播放模式"));

	fVolume = new BSlider("volume", nullptr, new BMessage(kMessageVolume), 0,
		100, B_HORIZONTAL);
	fVolume->SetValue(100);
	fVolume->SetExplicitMinSize(BSize(80, B_SIZE_UNSET));
	fVolume->SetExplicitMaxSize(BSize(110, B_SIZE_UNSET));

	// 左：封面曲名 + 红心 + 评论；中：上一首/播放/下一首；右：时间/模式/音量。
	// 左右两块宽度一致，中间那组才真正居中。
	BGroupView* left = new BGroupView("transport-left", B_HORIZONTAL,
		B_USE_SMALL_SPACING);
	BLayoutBuilder::Group<>(left, B_HORIZONTAL, B_USE_SMALL_SPACING)
		.Add(fNowBadge, 1.0f)
		.Add(fHeartButton, 0.0f)
		.Add(fCommentButton, 0.0f)
		.End();
	left->SetExplicitMinSize(BSize(300, 56));
	left->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 56));

	BGroupView* center = new BGroupView("transport-center", B_HORIZONTAL,
		0.0f);
	BLayoutBuilder::Group<>(center, B_HORIZONTAL, 0.0f)
		.Add(fPrevButton)
		.Add(fPlayButton)
		.Add(fNextButton)
		.End();
	center->SetExplicitMaxSize(BSize(B_SIZE_UNSET, 56));

	BGroupView* right = new BGroupView("transport-right", B_HORIZONTAL,
		B_USE_SMALL_SPACING);
	BLayoutBuilder::Group<>(right, B_HORIZONTAL, B_USE_SMALL_SPACING)
		.AddGlue()
		.Add(fTimeLabel)
		.Add(fModeButton)
		.Add(fVolume)
		.End();
	right->SetExplicitMinSize(BSize(300, 56));
	right->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 56));

	// 进度条压在底板最顶部，横贯整宽
	fTransport = new BGroupView("transport", B_VERTICAL, 0.0f);
	fTransport->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(fTransport, B_VERTICAL, 0.0f)
		.Add(fSeek)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_INSETS, 2, B_USE_WINDOW_INSETS, 6)
			.Add(left, 1.0f)
			.Add(center, 0.0f)
			.Add(right, 1.0f)
			.End()
		.End();
	fTransportLeft = left;
	fTransportRight = right;
}

void MainWindow::PlayIndex(int32 index)
{
	// 搜索是异步的，结果没回来前先记下，等 _FillResults 后再播
	fPendingPlay = index;
}

void MainWindow::SelectTab(int32 tab)
{
	if (fSearchTabs == nullptr || tab < 0 || tab >= fSearchTabs->CountTabs())
		return;

	// BTabView::Select 内部会 Hide()/Show() 子视图，必须在窗口锁里调
	if (!Lock())
		return;
	fSearchTabs->Select(tab);
	_SwitchToTab(tab);
	Unlock();
}

void MainWindow::ShowNowPlaying()
{
	if (!Lock())
		return;
	_ShowNowPlayingPage(true);
	Unlock();
}

void MainWindow::ShowArtist(long long id, const char* name)
{
	auto* detail = new DetailWindow(BMessenger(this), kDetailArtist);
	detail->LoadArtist(id, name);
	detail->Show();
}

void MainWindow::ShowAlbum(long long id, const char* name)
{
	auto* detail = new DetailWindow(BMessenger(this), kDetailAlbum);
	detail->LoadAlbum(id, name);
	detail->Show();
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

		case kMessageGenericDone:
		{
			int32 slot = -1;
			message->FindInt32("slot", &slot);

			const char* error = nullptr;
			if (message->FindString("error", &error) == B_OK) {
				fBusy = false;
				fSearchButton->SetEnabled(true);
				if (slot >= 0 && slot < kTabCount - 1)
					fGenericLoading[slot] = false;
				_Alert(Prefixed(B_TRANSLATE("搜索失败: "), error));
				return;
			}
			if (slot < 0 || slot >= kTabCount - 1)
				return;
			fGenericLoading[slot] = false;

			fLock.Lock();
			std::vector<ncm::GenericItem> items = fGenericItems[slot];
			fLock.Unlock();
			fGenericReady[slot] = true;

			// 歌手 / 用户是头像网格，其它是普通列表
			if (slot >= 2 && fAvatarGrids[slot - 2] != nullptr) {
				fAvatarGrids[slot - 2]->SetItems(items);
				_FetchAvatars(slot);
				return;
			}

			BListView* list = fGenericLists[slot];
			if (list != nullptr) {
				list->MakeEmpty();
				for (const ncm::GenericItem& item : items) {
					BString label(item.name.c_str());
					if (!item.subtitle.empty())
						label << "  ·  " << item.subtitle.c_str();
					list->AddItem(new BStringItem(label.String()));
				}
			}
			return;
		}

		case kMessageGenericOpen:
		{
			const int32 slot = fTabIndex - 1;
			if (slot < 0 || slot >= kTabCount - 1)
				return;
			BListView* list = slot >= 0 && slot < kTabCount - 1
				? fGenericLists[slot] : nullptr;
			if (list == nullptr)
				return;
			const int32 index = list->CurrentSelection();
			if (index < 0
				|| index >= static_cast<int32>(fGenericItems[slot].size()))
				return;

			const ncm::GenericItem item = fGenericItems[slot][index];

			// 歌单 / 专辑 / 歌手：都开独立页面窗口，里面是同一套歌曲表
			int detailKind = -1;
			if (fSearchType == 1000)
				detailKind = kDetailPlaylist;
			else if (fSearchType == 10)
				detailKind = kDetailAlbum;
			else if (fSearchType == 100)
				detailKind = kDetailArtist;

			if (detailKind < 0) {
				_Alert(B_TRANSLATE("这个类目只做展示"));
				return;
			}

			auto* detail = new DetailWindow(BMessenger(this), detailKind);
			if (detailKind == kDetailPlaylist)
				detail->LoadPlaylist(item.id, item.name.c_str());
			else if (detailKind == kDetailAlbum)
				detail->LoadAlbum(item.id, item.name.c_str());
			else
				detail->LoadArtist(item.id, item.name.c_str());
			detail->Show();
			return;
		}

		case kMessagePlaySongs:
		{
			// 歌手页/专辑页双击歌曲：把这批歌接过来当队列
			void* source = nullptr;
			int32 index = 0;
			if (message->FindPointer("source", &source) != B_OK
				|| source == nullptr)
				return;
			message->FindInt32("index", &index);

			auto* window = static_cast<DetailWindow*>(source);
			std::vector<ncm::Song> songs;
			if (window->Lock()) {
				songs = window->Songs();
				window->Unlock();
			}
			if (songs.empty() || index < 0
				|| index >= static_cast<int32>(songs.size()))
				return;

			fLock.Lock();
			fSongs.swap(songs);
			std::vector<ncm::Song> copy = fSongs;
			fLock.Unlock();

			// 主列表同步成这份队列，播放页/列表才是一致的
			fResults->Clear();
			for (const ncm::Song& song : copy)
				fResults->AddRow(new SongRow(song));
			fSongReady = true;

			fSearchTabs->Select(0);
			fTabIndex = 0;
			fSearchType = kTabTypes[0];
			if (fShowingNow)
				_ShowNowPlayingPage(false);

			_SyncPlaylist();
			_StartCoverDownload();
			_StartPlay(index);
			return;
		}

		case kMessageDebugTab:
		{
			int32 index = 0;
			if (message->FindInt32("index", &index) != B_OK)
				return;
			fSearchTabs->Select(index);
			return;
		}

		case kMessageDebugResize:
		{
			int32 width = 0;
			int32 height = 0;
			message->FindInt32("width", &width);
			message->FindInt32("height", &height);
			ResizeBy(width, height);
			return;
		}

		case kMessageToggleNowPlaying:
			_ToggleNowPlaying();
			return;

		case kMessageNowPlayingClose:
			_ShowNowPlayingPage(false);
			return;

		case kMessageNowPlayingReady:
		{
			int64 id = 0;
			if (message->FindInt64("id", &id) != B_OK)
				return;

			ncm::Song song;
			fLock.Lock();
			for (const ncm::Song& candidate : fSongs) {
				if (candidate.id == id) {
					song = candidate;
					break;
				}
			}
			fLock.Unlock();
			if (song.id == 0)
				return;

			_PushNowPlaying(song);
			return;
		}

		case kMessageShowComments:
		{
			fLock.Lock();
			const bool valid = fCurrentIndex >= 0
				&& static_cast<size_t>(fCurrentIndex) < fSongs.size();
			const ncm::Song song = valid ? fSongs[fCurrentIndex] : ncm::Song();
			fLock.Unlock();

			if (song.id == 0)
				return;
			if (fCommentsWindow != nullptr) {
				fCommentsWindow->Activate();
				return;
			}
			fCommentsWindow = new CommentsWindow(BMessenger(this),
				song.id, song.name.c_str());
			fCommentsWindow->Show();
			return;
		}

		case kMessageCommentsClosed:
			fCommentsWindow = nullptr;
			return;

		case kMessageAvatarsDone:
		{
			int32 slot = -1;
			if (message->FindInt32("slot", &slot) == B_OK && slot >= 2
				&& slot < kTabCount - 1 && fAvatarGrids[slot - 2] != nullptr)
				fAvatarGrids[slot - 2]->RefreshAvatars();
			return;
		}

		case kMessageOpenArtist:
		case kMessageOpenAlbum:
		{
			int64 id = 0;
			if (message->FindInt64("id", &id) != B_OK || id <= 0)
				return;
			if (message->what == kMessageOpenArtist)
				ShowArtist(id, "");
			else
				ShowAlbum(id, "");
			return;
		}

		case kMessagePlaylistClosed:
			fPlaylistWindow = nullptr;
			return;

		case kMessageSearchDone:
		{
			const char* error = nullptr;
			if (message->FindString("error", &error) == B_OK) {
				fBusy = false;
				fSearchButton->SetEnabled(true);
				_Alert(Prefixed(B_TRANSLATE("搜索失败: "), error));
				return;
			}

			// 从歌单/专辑/歌手载入的结果，切回单曲页展示
			bool fromDetail = false;
			message->FindBool("fromDetail", &fromDetail);
			if (fromDetail) {
				fSearchTabs->Select(0);
				fTabIndex = 0;
				fSearchType = kTabTypes[0];
			}
			_FillResults();
			return;
		}

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
			// 先把索引定下来：流式播放那一路以前漏了这步，播放页/底栏一直拿不到曲目
			int32 doneIndex = -1;
			message->FindInt32("index", &doneIndex);
			if (doneIndex >= 0)
				fCurrentIndex = doneIndex;

			BPositionIO* source = nullptr;
			if (message->FindPointer("source", reinterpret_cast<void**>(&source)) == B_OK
				&& source != nullptr) {
				std::string streamError;
				std::string error;
				if (!fPlayer->LoadStream(source, &streamError)) {
					_Alert(Prefixed(B_TRANSLATE("解码失败: "), streamError));
				} else {
					fSeek->SetEnabled(true);
					if (fPlayer->Play(&error)) {
						fPlayButton->SetKind(TransportButton::kPause);
						_SyncPlaylist();
						_StartNowPlayingThread();
					} else {
						fPlayButton->SetKind(TransportButton::kPlay);
						_Alert(Prefixed(B_TRANSLATE("没有音频输出设备: "), error));
					}
					_UpdateTransport();
				}
				fBusy = false;
				return;
			}

			const char* path = nullptr;
			if (message->FindString("path", &path) == B_OK) {
				std::string error;
				if (!fPlayer->Load(path, &error)) {
					_Alert(Prefixed(B_TRANSLATE("解码失败: "), error));
				} else {
					fSeek->SetEnabled(true);
					if (fPlayer->Play(&error)) {
						fPlayButton->SetKind(TransportButton::kPause);
						_SyncPlaylist();
						_StartNowPlayingThread();
					} else {
						fPlayButton->SetKind(TransportButton::kPlay);
						_Alert(Prefixed(B_TRANSLATE("没有音频输出设备: "), error));
					}
					_UpdateTransport();
				}
			}

			const char* text = nullptr;
			if (message->FindString("text", &text) == B_OK)
				_Alert(text);
			fBusy = false;
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
				_Alert(Prefixed(B_TRANSLATE("定位失败: "), error));
			_UpdateTransport();
			return;
		}

		case kMessageTick:
		{
			// tab 选中项靠轮询同步（beta6 的 BTabView 不给选择消息）
			_SyncTab();

			if (!fPlayer->IsPlaying())
				return;

			if (fSeekDirty && !fSeek->IsTracking()) {
				fSeekDirty = false;
				const double target = fSeekTarget / 1000.0
					* fPlayer->DurationSeconds();
				std::string seekError;
				if (!fPlayer->Seek(target, &seekError))
					_Alert(Prefixed(B_TRANSLATE("定位失败: "), seekError));
			}

			if (fPlayer->IsFinished()) {
				// 到曲末：停输出、复位按钮，再按模式决定要不要接下一首
				fPlayer->Stop();
				fPlayButton->SetKind(TransportButton::kPlay);
				fSeek->SetValue(1000);
				fTimeLabel->SetText(BString(FormatTime(
					fPlayer->DurationSeconds()).c_str()));

				fLock.Lock();
				const bool hasSongs = !fSongs.empty();
				fLock.Unlock();
				if (fPlayMode != 1 && hasSongs)
					PostMessage(kMessageNext);
				return;
			}

			_UpdateTransport();
			return;
		}

		case kMessageVolume:
		{
			fPlayer->SetGain(fVolume->Value() / 100.0f);
			return;
		}

		case kMessageMode:
			_CyclePlayMode();
			return;

		case kMessageQuality:
		{
			const char* level = nullptr;
			if (message->FindString("level", &level) != B_OK || level == nullptr)
				return;
			fQuality = level;
			for (int32 i = 0; i < fQualityMenu->CountItems(); i++) {
				BMenuItem* item = fQualityMenu->ItemAt(i);
				const char* value = nullptr;
				if (item != nullptr && item->Message() != nullptr
					&& item->Message()->FindString("level", &value) == B_OK)
					item->SetMarked(fQuality == value);
			}
			return;
		}

		case kMessageAccountDone:
			_ApplyAccount(message);
			return;

		case kMessageOpenUserPlaylist:
		{
			int64 id = 0;
			const char* name = nullptr;
			if (message->FindInt64("id", &id) != B_OK || id <= 0)
				return;
			message->FindString("name", &name);

			auto* detail = new DetailWindow(BMessenger(this),
				kDetailPlaylist);
			detail->LoadPlaylist(id, name);
			detail->Show();
			return;
		}

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
			return;
		}

		case kMessageQueueReorder:
		{
			// 播放列表窗口拖动排序：按歌 id 顺序重建 fSongs
			int32 count = 0;
			if (message->FindInt32("count", &count) != B_OK
				|| count <= 0 || count > 10000)
				return;
			std::vector<long long> order;
			order.reserve(count);
			for (int32 i = 0; i < count; i++) {
				int64 id = 0;
				if (message->FindInt64("id", i, &id) != B_OK)
					return;
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
				SaveSession(cookie, nickname, fUserId);

				BString label(B_TRANSLATE("使用 "));
				label << nickname;
				fUserMenu->Superitem()->SetLabel(label.String());
				fLogoutItem->SetEnabled(true);

				// 红心状态 + 我的歌单
				_FetchAccountInfo();
			}
			return;
		}

		case kMessageLogout:
			fApi->SetCookie(std::string());
			remove(SessionPath().c_str());
			fUserMenu->Superitem()->SetLabel(B_TRANSLATE("未登录"));
			fLogoutItem->SetEnabled(false);
			fUserId = 0;
			fLikedIds.clear();
			fPlaylists.clear();
			_ApplyLikes();
			_RebuildPlaylistMenu();
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
	if (text == nullptr || text[0] == '\0')
		return;

	fKeyword = text;
	// 关键字变了，之前各 tab 的缓存全部作废
	_ClearGeneric();

	if (fTabIndex == 0) {
		fBusy = true;
		fSearchButton->SetEnabled(false);
		thread_id thread = spawn_thread(_SearchThread, "ncm-search",
			B_NORMAL_PRIORITY, this);
		if (thread < B_OK) {
			fBusy = false;
			fSearchButton->SetEnabled(true);
			return;
		}
		resume_thread(thread);
		return;
	}

	// 类目搜索不占领 fBusy：通用列表慢慢拉也不该挡住放歌
	_FetchGeneric(fTabIndex - 1);
}

void MainWindow::_ClearGeneric()
{
	for (int32 i = 0; i < kTabCount - 1; i++) {
		fGenericReady[i] = false;
		fLock.Lock();
		fGenericItems[i].clear();
		fLock.Unlock();
		if (fGenericLists[i] != nullptr)
			fGenericLists[i]->MakeEmpty();
	}
	fSongReady = false;
}

void MainWindow::_SyncTab()
{
	if (fSearchTabs == nullptr)
		return;

	const int32 selection = fSearchTabs->Selection();
	if (selection >= 0 && selection < kTabCount && selection != fTabIndex)
		_SwitchToTab(selection);
}

void MainWindow::_SwitchToTab(int32 tab)
{
	fTabIndex = tab;
	fSearchType = kTabTypes[tab];
	if (tab == 0)
		return;

	const int32 slot = tab - 1;
	if (fGenericReady[slot])
		return;

	// 没缓存就按当前关键词补一次（不打断正在进行的搜索）
	if (!fGenericLoading[slot] && fKeyword.Length() > 0)
		_FetchGeneric(slot);
}

void MainWindow::_FetchGeneric(int32 slot)
{
	if (slot < 0 || slot >= kTabCount - 1 || fGenericLoading[slot])
		return;

	fGenericLoading[slot] = true;
	auto* job = new GenericJob{this, slot, fKeyword};
	thread_id thread = spawn_thread(_GenericSearchThread, "ncm-gen",
		B_NORMAL_PRIORITY, job);
	if (thread < B_OK) {
		delete job;
		fGenericLoading[slot] = false;
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
		BMessage failure(kMessageSearchDone);
		failure.AddString("error", error.c_str());
		self->_PostIfAlive(&failure);
	}
	return 0;
}

void MainWindow::_FillResults()
{
	// 上一次的关键词想用的结果已经过时，重新起播
	fPlayer->Stop();
	fResults->Clear();
	fCurrentIndex = -1;
	fPlayButton->SetKind(TransportButton::kPlay);
	fSeek->SetValue(0);
	fSeek->SetEnabled(false);
	fTimeLabel->SetText("0:00 / 0:00");

	fLock.Lock();
	const size_t count = fSongs.size();
	for (size_t i = 0; i < count; i++) {
		fResults->AddRow(new SongRow(fSongs[i]));
	}
	fLock.Unlock();

	fBusy = false;
	fSongReady = true;
	fSearchButton->SetEnabled(true);

	_ApplyLikes();
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

int32 MainWindow::_GenericSearchThread(void* data)
{
	auto* job = static_cast<GenericJob*>(data);
	MainWindow* self = job->window;
	const int32 slot = job->slot;
	const BString keyword = job->keyword;
	delete job;

	std::vector<ncm::GenericItem> items;
	std::string error;
	const bool ok = self->fApi->SearchGeneric(kTabTypes[slot + 1],
		keyword.String(), 30, &items, &error);

	BMessage done(kMessageGenericDone);
	done.AddInt32("slot", slot);
	if (ok) {
		self->fLock.Lock();
		self->fGenericItems[slot].swap(items);
		self->fLock.Unlock();
	} else {
		done.AddString("error", error.c_str());
	}
	self->_PostIfAlive(&done);
	return 0;
}

int32 MainWindow::_DetailThread(void* data)
{
	auto* payload = static_cast<std::pair<MainWindow*, BMessage>*>(data);
	MainWindow* self = payload->first;
	BMessage request = payload->second;
	delete payload;

	int32 kind = 0;
	int64 id = 0;
	request.FindInt32("kind", &kind);
	request.FindInt64("id", &id);

	std::vector<ncm::Song> songs;
	std::string error;
	bool ok = false;
	try {
		if (kind == 1000)
			ok = self->fApi->FetchPlaylistSongs(id, &songs, &error);
		else if (kind == 10)
			ok = self->fApi->FetchAlbumSongs(id, &songs, &error);
		else if (kind == 100)
			ok = self->fApi->FetchArtistSongs(id, &songs, &error);
	} catch (const std::exception& exception) {
		// 线程里漏出去的异常会直接 abort 整个程序，这里兜住
		error = exception.what();
		ok = false;
	}

	if (ok) {
		self->fLock.Lock();
		self->fSongs.swap(songs);
		self->fLock.Unlock();

		// 载入歌单/专辑/歌手后跳回单曲页展示
		BMessage done(kMessageSearchDone);
		done.AddBool("fromDetail", true);
		self->_PostIfAlive(&done);
	} else {
		BMessage failure(kMessageGenericDone);
		failure.AddInt32("slot", -1);
		failure.AddString("error", error.c_str());
		self->_PostIfAlive(&failure);
	}
	return 0;
}

int32 MainWindow::_NowPlayingThread(void* data)
{
	auto* payload = static_cast<std::pair<MainWindow*, ncm::Song>*>(data);
	MainWindow* self = payload->first;
	const ncm::Song song = payload->second;
	delete payload;

	std::string error;

	// 大封面：把缩略图参数换掉
	std::string coverUrl = song.coverUrl;
	const size_t param = coverUrl.find("120y120");
	if (param != std::string::npos)
		coverUrl.replace(param, 7, "512y512");

	const std::string coverPath = BigCoverPath(song.id);
	struct stat info;
	if (stat(coverPath.c_str(), &info) != 0 || info.st_size < 4096)
		self->fApi->DownloadCover(coverUrl, coverPath, &error);

	// 歌词三层：原文 / 翻译 / 罗马音，各自落盘（播放页直接读文件）
	std::string lrc;
	std::string translation;
	std::string romaji;
	self->fApi->FetchLyric(song.id, &lrc, &translation, &romaji, &error);

	const auto writeFile = [](const std::string& path, const std::string& text) {
		FILE* file = fopen(path.c_str(), "w");
		if (file == nullptr)
			return;
		if (!text.empty())
			fwrite(text.data(), 1, text.size(), file);
		fclose(file);
	};
	writeFile(LyricPath(song.id), lrc);
	writeFile(TranslationPath(song.id), translation);
	writeFile(RomajiPath(song.id), romaji);

	// 评论：格式化成纯文本，播放页的评论页签直接显示
	std::vector<ncm::Comment> comments;
	BString commentText;
	if (self->fApi->FetchComments(song.id, 30, 0, &comments, &error)) {
		for (const ncm::Comment& comment : comments) {
			commentText << comment.nickname.c_str();
			if (comment.likedCount > 0)
				commentText << "  ·  " << comment.likedCount
							<< B_TRANSLATE(" 赞");
			commentText << "\n" << comment.content.c_str() << "\n\n";
		}
	}
	writeFile(CommentsPath(song.id), std::string(commentText.String()));

	BMessage ready(kMessageNowPlayingReady);
	ready.AddInt64("id", song.id);
	self->_PostIfAlive(&ready);
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

	if (!valid)
		return;

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

	auto* payload = new PlayJob{this, index, fQuality};
	thread_id thread = spawn_thread(_PlayThread, "ncm-play", B_NORMAL_PRIORITY,
		payload);
	if (thread < B_OK) {
		delete payload;
		fBusy = false;
		return;
	}
	resume_thread(thread);
}

int32 MainWindow::_PlayThread(void* data)
{
	auto* payload = static_cast<PlayJob*>(data);
	MainWindow* self = payload->window;
	const int32 index = payload->index;
	const std::string level = payload->level;
	delete payload;

	self->fLock.Lock();
	if (index < 0 || index >= static_cast<int32>(self->fSongs.size())) {
		self->fLock.Unlock();
		BMessage invalid(kMessagePlayDone);
		invalid.AddString("text", B_TRANSLATE("该歌曲已被移出队列"));
		self->_PostIfAlive(&invalid);
		return 0;
	}
	const ncm::Song song = self->fSongs[index];
	self->fLock.Unlock();

	BMessage result(kMessagePlayDone);

	ncm::AudioUrl audio;
	std::string error;
	if (!self->fApi->SongUrl(song.id,
			level.empty() ? "standard" : level, &audio, &error)) {
		result.AddString("text", Prefixed(B_TRANSLATE("取流失败: "), error).String());
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
		result.AddInt32("index", index);
		result.AddPointer("source", source);
		self->_PostIfAlive(&result);
		return 0;
	}
	delete source;

	// 回退：CDN 不支持 Range（少见）就整曲下载
	if (!self->fApi->DownloadAudio(audio, path, &error)) {
		result.AddString("text", Prefixed(B_TRANSLATE("下载失败: "), error).String());
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
		_Alert(Prefixed(B_TRANSLATE("没有音频输出设备: "), error));
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
	if (fCurrentIndex < 0)
		return;

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
		_Alert(Prefixed(B_TRANSLATE("喜欢失败: "), error));
		return;
	}

	fLiked = target;
	fHeartButton->SetKind(fLiked ? TransportButton::kHeartFilled
								 : TransportButton::kHeart);
	fHeartButton->Invalidate();

	if (fLiked)
		fLikedIds.insert(static_cast<long long>(id));
	else
		fLikedIds.erase(static_cast<long long>(id));

	const int32 rows = fResults->CountRows(nullptr);
	for (int32 i = 0; i < rows; i++) {
		auto* row = static_cast<SongRow*>(fResults->RowAt(i, nullptr));
		if (row != nullptr && row->Song().id == id) {
			row->SetLiked(fLiked);
			fResults->InvalidateRow(row);
		}
	}
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

void MainWindow::_ToggleNowPlaying()
{
	_ShowNowPlayingPage(!fShowingNow);
}

void MainWindow::_ShowNowPlayingPage(bool show)
{
	fShowingNow = show;

	// 先收起/放出搜索行，再切卡片：反过来会先按旧位置画一遍，
	// 挪上去之后旧位置的像素没人擦，屏幕上就出现两份内容
	if (fSearchRow != nullptr) {
		if (show)
			fSearchRow->Hide();
		else
			fSearchRow->Show();
	}
	fContentHost->CardLayout()->SetVisibleItem(show ? 1 : 0);
	fContentHost->Invalidate();
	if (!show)
		return;

	// 立刻把已有的封面/歌词推过去，剩下由 ready 消息补
	fLock.Lock();
	const bool valid = fCurrentIndex >= 0
		&& static_cast<size_t>(fCurrentIndex) < fSongs.size();
	const ncm::Song song = valid ? fSongs[fCurrentIndex] : ncm::Song();
	fLock.Unlock();

	if (!valid)
		return;

	_PushNowPlaying(song);
	_StartNowPlayingThread();
}

void MainWindow::_PushNowPlaying(const ncm::Song& song)
{
	if (song.id == 0)
		return;

	const std::string coverPath = BigCoverPath(song.id);
	BBitmap* mini = BTranslationUtils::GetBitmapFile(coverPath.c_str());
	if (mini == nullptr)
		mini = BTranslationUtils::GetBitmapFile(CoverPath(song.id).c_str());
	fNowBadge->SetInfo(song.name.c_str(), song.artist.c_str(), mini);

	// 封面主题色铺满整个窗口（导航栏以外）
	const rgb_color theme = ThemeColorFromCover(mini);
	_ApplyTheme(theme);

	fNowView->SetSong(song.name.c_str(), song.artist.c_str(), song.artistId,
		song.album.c_str(), song.albumId, coverPath.c_str(),
		LyricPath(song.id).c_str(), TranslationPath(song.id).c_str(),
		RomajiPath(song.id).c_str());
	_SyncHeartButton();
}

void MainWindow::_ApplyTheme(rgb_color color)
{
	fTheme = color;
	const auto tint = [color](BView* view) {
		if (view == nullptr)
			return;
		view->SetViewColor(color);
		view->SetLowColor(color);
	};

	tint(fSearchRow);
	tint(fContentHost);
	tint(fTransport);
	tint(fTransportLeft);
	tint(fTransportRight);
	tint(fNowBadge);
	tint(fPrevButton);
	tint(fPlayButton);
	tint(fNextButton);
	tint(fHeartButton);
	tint(fCommentButton);
	tint(fVolume);
	if (fSearchTabs != nullptr) {
		tint(fSearchTabs);
		for (int32 i = 0; i < fSearchTabs->CountTabs(); i++)
			tint(fSearchTabs->ViewForTab(i));
	}
	if (fNowView != nullptr)
		fNowView->SetThemeColor(color);
}

void MainWindow::_SyncHeartButton()
{
	fLock.Lock();
	const bool valid = fCurrentIndex >= 0
		&& static_cast<size_t>(fCurrentIndex) < fSongs.size();
	const long long id = valid ? fSongs[fCurrentIndex].id : 0;
	fLock.Unlock();

	fLiked = id != 0 && fLikedIds.count(id) > 0;
	fHeartButton->SetKind(fLiked ? TransportButton::kHeartFilled
								 : TransportButton::kHeart);
	fHeartButton->Invalidate();
}

void MainWindow::_StartNowPlayingThread()
{
	fLock.Lock();
	const bool valid = fCurrentIndex >= 0
		&& static_cast<size_t>(fCurrentIndex) < fSongs.size();
	const ncm::Song song = valid ? fSongs[fCurrentIndex] : ncm::Song();
	fLock.Unlock();
	if (!valid)
		return;

	auto* payload = new std::pair<MainWindow*, ncm::Song>(this, song);
	thread_id thread = spawn_thread(_NowPlayingThread, "ncm-now",
		B_LOW_PRIORITY, payload);
	if (thread < B_OK) {
		delete payload;
		return;
	}
	resume_thread(thread);
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

	if (fShowingNow)
		fNowView->SetPosition(position);
}

void MainWindow::_BuildQualityMenu(BMenu* parent)
{
	static const struct {
		const char* label;
		const char* level;
	} kLevels[] = {
		{ B_TRANSLATE("标准"), "standard" },
		{ B_TRANSLATE("较高"), "higher" },
		{ B_TRANSLATE("极高"), "exhigh" },
	};

	for (const auto& entry : kLevels) {
		BMessage* message = new BMessage(kMessageQuality);
		message->AddString("level", entry.level);
		BMenuItem* item = new BMenuItem(entry.label, message);
		item->SetMarked(fQuality == entry.level);
		parent->AddItem(item);
	}
	parent->SetTargetForItems(this);
}

void MainWindow::_RebuildPlaylistMenu()
{
	if (fMyPlaylistMenu == nullptr)
		return;

	while (fMyPlaylistMenu->CountItems() > 0)
		delete fMyPlaylistMenu->RemoveItem((int32)0);

	if (fPlaylists.empty()) {
		BMenuItem* empty = new BMenuItem(
			fUserId > 0 ? B_TRANSLATE("（没有歌单）")
						: B_TRANSLATE("（登录后显示）"),
			nullptr);
		empty->SetEnabled(false);
		fMyPlaylistMenu->AddItem(empty);
		return;
	}

	for (const ncm::PlaylistInfo& playlist : fPlaylists) {
		BString label(playlist.name.c_str());
		if (playlist.trackCount > 0)
			label << " (" << playlist.trackCount << ")";
		BMessage* message = new BMessage(kMessageOpenUserPlaylist);
		message->AddInt64("id", playlist.id);
		message->AddString("name", playlist.name.c_str());
		fMyPlaylistMenu->AddItem(new BMenuItem(label.String(), message));
	}
	fMyPlaylistMenu->SetTargetForItems(this);
}

void MainWindow::_FetchAccountInfo()
{
	if (!fApi->IsLoggedIn())
		return;

	auto* job = new std::pair<MainWindow*, long long>(this, fUserId);
	thread_id thread = spawn_thread(_AccountThread, "ncm-account",
		B_LOW_PRIORITY, job);
	if (thread < B_OK) {
		delete job;
		return;
	}
	resume_thread(thread);
}

int32 MainWindow::_AccountThread(void* data)
{
	auto* job = static_cast<std::pair<MainWindow*, long long>*>(data);
	MainWindow* self = job->first;
	const long long uid = job->second;
	delete job;

	ncm::NeteaseApi api;
	std::string error;

	BMessage done(kMessageAccountDone);

	long long userId = uid;
	std::string nickname;
	if (api.FetchAccount(&userId, &nickname, &error)) {
		done.AddInt64("uid", userId);
		done.AddString("nickname", nickname.c_str());
	}

	// 红心：一次拿全量 id，列表里直接点亮
	std::vector<long long> ids;
	if (api.FetchLikedIds(userId, &ids, &error)) {
		for (long long id : ids)
			done.AddInt64("liked", id);
	}

	// 我的歌单（含收藏的）
	std::vector<ncm::PlaylistInfo> playlists;
	if (api.FetchUserPlaylists(userId, &playlists, &error)) {
		for (const ncm::PlaylistInfo& playlist : playlists) {
			done.AddInt64("plid", playlist.id);
			done.AddString("plname", playlist.name.c_str());
			done.AddInt32("plcount", playlist.trackCount);
		}
	}

	self->_PostIfAlive(&done);
	return 0;
}

void MainWindow::_ApplyAccount(BMessage* message)
{
	const char* freshName = nullptr;
	message->FindString("nickname", &freshName);

	int64 uid = 0;
	if (message->FindInt64("uid", &uid) == B_OK && uid > 0) {
		fUserId = uid;

		// 顺手把 uid / 昵称补进会话文件，下次启动就不用再查
		std::string cookie;
		std::string nickname;
		long long ignored = 0;
		LoadSession(&cookie, &nickname, &ignored);
		if (freshName != nullptr && freshName[0] != 0)
			nickname = freshName;
		SaveSession(cookie.empty() ? fApi->Cookie() : cookie, nickname,
			fUserId);

		if (!nickname.empty()) {
			BString label(B_TRANSLATE("使用 "));
			label << nickname.c_str();
			fUserMenu->Superitem()->SetLabel(label.String());
		}
	}

	fLikedIds.clear();
	int64 id = 0;
	for (int32 i = 0; message->FindInt64("liked", i, &id) == B_OK; i++)
		fLikedIds.insert(static_cast<long long>(id));

	fPlaylists.clear();
	const char* name = nullptr;
	int32 trackCount = 0;
	for (int32 i = 0; message->FindInt64("plid", i, &id) == B_OK; i++) {
		ncm::PlaylistInfo playlist;
		playlist.id = id;
		if (message->FindString("plname", i, &name) == B_OK)
			playlist.name = name;
		if (message->FindInt32("plcount", i, &trackCount) == B_OK)
			playlist.trackCount = trackCount;
		fPlaylists.push_back(playlist);
	}

	_ApplyLikes();
	_RebuildPlaylistMenu();
	_SyncHeartButton();
}

void MainWindow::_ApplyLikes()
{
	if (fLikedIds.empty())
		return;

	const int32 count = fResults->CountRows(nullptr);
	for (int32 i = 0; i < count; i++) {
		auto* row = static_cast<SongRow*>(fResults->RowAt(i, nullptr));
		if (row == nullptr)
			continue;
		const bool liked = fLikedIds.count(row->Song().id) > 0;
		if (liked != row->Field()->Liked()) {
			row->SetLiked(liked);
			fResults->InvalidateRow(row);
		}
	}
}

void MainWindow::_FetchAvatars(int32 slot)
{
	if (slot < 0 || slot >= kTabCount - 1)
		return;

	auto* job = new std::pair<MainWindow*, int32>(this, slot);
	thread_id thread = spawn_thread(_AvatarThread, "ncm-avatars",
		B_LOW_PRIORITY, job);
	if (thread < B_OK) {
		delete job;
		return;
	}
	resume_thread(thread);
}

int32 MainWindow::_AvatarThread(void* data)
{
	auto* job = static_cast<std::pair<MainWindow*, int32>*>(data);
	MainWindow* self = job->first;
	const int32 slot = job->second;
	delete job;

	ncm::NeteaseApi api;
	std::string error;

	self->fLock.Lock();
	std::vector<ncm::GenericItem> items = self->fGenericItems[slot];
	self->fLock.Unlock();

	for (const ncm::GenericItem& item : items) {
		if (item.imageUrl.empty())
			continue;

		const std::string path = ncm::CacheDir() + "/avatar_"
			+ std::to_string(item.id) + ".jpg";
		struct stat info;
		if (stat(path.c_str(), &info) == 0 && info.st_size > 1024)
			continue;

		api.DownloadCover(item.imageUrl, path, &error);
	}

	BMessage done(kMessageAvatarsDone);
	done.AddInt32("slot", slot);
	self->_PostIfAlive(&done);
	return 0;
}

void MainWindow::_Alert(const BString& text)
{
	BAlert* alert = new BAlert(B_TRANSLATE("HaikuNCM"), text.String(),
		B_TRANSLATE("好"), nullptr, nullptr, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->Go();
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
