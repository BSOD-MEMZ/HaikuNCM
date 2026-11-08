# HaikuNCM

Haiku 原生的第三方网易云音乐客户端。C++17，Interface Kit + Media Kit + libcurl + OpenSSL，无任何跨平台框架——就是给 Haiku 写的 Haiku 程序。

## 功能

- 搜索 / 取流 / 下载播放（weapi + eapi 协议，全部独立实现）
- 歌曲表：封面、双行标题（标题+歌手）、专辑、喜欢、时长，自绘 `BListItem`
- 播放条：上一首 / 播放暂停 / 下一首 / 进度条 / 音量 / 播放模式（顺序·单曲·随机）
- 红心：调 `/weapi/song/like`（需登录）
- 播放列表窗口
- 登录三方式：手机验证码 / 扫码（本地 qrcodegen 生成二维码 + 轮询）/ 密码
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
src/ui/       MainWindow、SongItem/SongHeader（自绘表格）、TransportButton（自绘传输按钮，
              参考 Haiku MediaPlayer 的 SymbolButton：BShape + BControlLook + 渐变）、
              LoginWindow（三页签 + QrView）、UserMenuItem（菜单栏右侧登录项）
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

## 协议

代码 MIT。接口参考 NeteaseCloudMusicApi，独立实现；本项目与网易官方无关。
