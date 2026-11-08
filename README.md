# HaikuNCM

Haiku 原生的第三方网易云音乐客户端。C++17，Interface Kit + Media Kit + libcurl + OpenSSL，无任何跨平台框架——就是给 Haiku 写的 Haiku 程序。

## 功能

- 搜索 / 取流 / 下载播放（weapi + eapi 协议，全部独立实现）
- 聚合搜索：单曲 / 歌单 / 专辑 / 歌手 / 用户 / 声音 六个类目 tab，
  无边框 `BTabView` 直接压在列表上（HaikuDepot 同款），切 tab 按需拉取并缓存
- 歌曲表：封面、双行标题（标题+歌手）、专辑、喜欢、时长，BColumnListView 自绘列
- 歌单页 / 专辑页 / 歌手页：新窗口，头像·封面 + 名字 + 信息，同一套功能丰富的歌曲表，
  双击歌曲直接把整批接成播放队列；搜索结果里歌手 / 用户 tab 是头像网格
- 播放页：嵌在主窗口内容区（封面 + 滚动歌词），底部播放控件始终保留；
  歌词支持翻译 / 罗马音开关（有数据才显示）；整个窗口（除导航栏）都会
  叠上一层从专辑封面取的主题色（面板色 + 30% 封面平均色）
- 评论：底板红心旁的评论按钮弹出独立评论窗口（热评在前）
- 播放条：进度条压在底板顶部，封面·曲名在左、传输键居中、时间·红心·模式·音量在右；
  hover 时按钮底面提亮、字形转绿色渐变，播放中播放键保持绿色
- 音质选择：音乐 → 音质（标准 / 较高 / 极高；无损对流式播放压力太大，先撤了）
- 登录态：红心状态从 `/weapi/song/like/get` 全量同步到列表，音乐菜单里列「我的歌单」
- 点歌手名（标题列下半）或专辑列、播放页里的歌手·专辑链接，直接开对应页面
- 红心：调 `/weapi/song/like`（需登录）
- 播放列表窗口
- 登录三方式：扫码（默认页，开窗自动取码）/ 手机验证码 / 密码
- i18n 钩子：全部文案走 `B_TRANSLATE`，`make catkeys` 抽翻译模板

## 构建（在 Haiku 上）

依赖：`pkgman install curl_devel ca_root_certificates`（beta6 自带 gcc、openssl 头、ffmpeg 解码插件）。

```
make          # 构建 build/HaikuNCM
make rdef     # rc + xres 注入应用资源（签名/版本）
make catkeys  # 抽取翻译模板 build/zh-Hans.catkeys
```

## 结构

```
src/net/      crypto（weapi/eapi/MD5，OpenSSL）、http（libcurl）、netease（API 封装）
src/audio/    Player：BMediaFile + BMediaTrack + BSoundPlayer 封装，增益/进度/Seek
src/ui/       MainWindow（内容区 + 底部播放底板）、DetailWindow（歌手页/专辑页）、
              NowPlayingPage（LrcView 歌词 + CoverView 封面 + 底部徽章）、
              SongRow（BColumnListView 的列/行）、TransportButton（自绘传输按钮，
              参考 Haiku MediaPlayer 的 SymbolButton：BShape + BControlLook + 渐变）、
              LoginWindow（三页签 + QrView）
tools/        ncmcli（终端验证工具）、wavdump（解码导 WAV）、proto_spike.py（协议基准）、
              make_catkeys.py（beta6 的 collectcatkeys 是坏的，自己抽）
third_party/  nlohmann/json、nayuki qrcodegen（MIT）、simple-icons 网易云 logo 路径
rdef/         应用资源定义
icon/         SVG 源 + 64/32/16 PNG（HVIF 待 Icon-O-Matic 导出）
```

## Haiku 开发踩坑记录（实测 beta6 / hrev59866+79）

- `B_TRANSLATE` 的实现库是 **`-llocalestub`**；`Catalog.h` 在 `headers/os/locale/` 下
- beta6 的 `collectcatkeys` 抽不出 `B_CATKEY((ctx),(str))` 展开的键，`makecat` 未随包提供
- `BButton::SetIcon()` 在 beta6 上图标不显示；离屏 BView `Sync()` 会崩 → 传输按钮用 BControl 自绘
- 控件只能在自己的窗口线程操作，跨线程 `SetText` 直接崩，走 `BMessage`
- `B_AUTO_UPDATE_SIZE_LIMITS` 会把窗口压成布局最小尺寸
- sshd 默认读 `~/config/settings/ssh/authorized_keys`（不是 `~/.ssh/`）；`user` 账号 uid=0，被 `PermitRootLogin` 管
- beta6 开发包头里没有 HTTP 客户端 API，libcurl 是唯一现实选择
- VMware 对 Haiku 模拟 ES1371 声卡无驱动、拒绝实例化 HDA → 出声要用 VirtualBox（AC'97，`auich` 驱动）
- `BTabView` 构造时**显式传 flags 会顶掉默认的 `B_WILL_DRAW`**，整个 tab 条都画不出来
- beta6 的接口叫 `SetBorder(border_style)`（不是 `SetBorderStyle`）；`B_NO_BORDER` 才能让 tab 贴着列表
- `BTabView::Select()` 内部会 `Hide()/Show()` 子视图，**必须在窗口锁里调**，否则 `Looper must be locked` 直接崩
- 自定义 `BControl`（如传输按钮）不实现 `GetPreferredSize` 就没有尺寸，放进布局会被压成 0 宽
- `BListView(name, type, flags)` 第三个参数是 **flags**；把 `B_PLAIN_BORDER`（值为 0）当 flags 传进去
  等于清掉 `B_WILL_DRAW`，列表有数据也一个字不画
- `BLayoutBuilder` 里 `Add*/AddGroup*` 默认 weight=1：菜单栏行、搜索行、底板都要显式给 0.0f，
  否则窗口一放大它们就跟着长，把内容区挤走
- 透明子视图（`SetViewColor(B_TRANSPARENT_COLOR)`）不擦底色：内容会移动的视图（比如滚动的歌词）
  必须自己有背景，否则旧位置的字会一直留着，看起来像"歌词重复"
- `BStringView::SetText()` 不会触发父布局重排，文字宽度变了要自己 `InvalidateLayout(true)`

## 命令行（调试用）

```
./build/HaikuNCM --search "关键词" [--play N]   # 启动即搜索 / 搜完播第 N 首
--tab N              # 启动时选中第 N 个类目（0 单曲 / 1 歌单 / 2 专辑 / 3 歌手 / 4 用户 / 5 声音）
--now                # 启动即切到播放页
--artist ID / --album ID   # 直接打开某个歌手页 / 专辑页
--login / --playlist       # 打开登录窗口 / 播放列表
```

## 协议

代码 MIT。接口参考 NeteaseCloudMusicApi，独立实现；本项目与网易官方无关。
