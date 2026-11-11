#!/usr/bin/env bash
# 从 Windows 推送 HaikuNCM 源码到 Haiku 虚拟机并编译。
#   ./sync-build.sh          推送 + 编译 + 注入资源
#   ./sync-build.sh --run    另外重启虚拟机里的 HaikuNCM
#   ./sync-build.sh --force  源码没变也强制重编
set -uo pipefail

HOST=${HOST:-haiku}
REMOTE_DIR=${REMOTE_DIR:-/boot/home/HaikuNCM}
VM=${VM:-HaikuOS}
VBOXMANAGE=${VBOXMANAGE:-"/c/Program Files/Oracle/VirtualBox/VBoxManage.exe"}
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
STATE="$ROOT/.workbuddy/vmtools/.last-sync-hash"

RUN_AFTER=0
FORCE=0
for arg in "$@"; do
	case "$arg" in
		--run) RUN_AFTER=1 ;;
		--force) FORCE=1 ;;
	esac
done

# 虚拟机会话重启后运行时 NAT 转发规则会丢，这里自动补回
ssh_probe() { ssh -o BatchMode=yes -o ConnectTimeout=6 "$HOST" true 2>/dev/null; }
if ! ssh_probe; then
	echo "==> SSH 不通，检查并重建端口转发"
	if ! "$VBOXMANAGE" showvminfo "$VM" --machinereadable 2>/dev/null | grep -q 'Forwarding(0)="ssh,'; then
		"$VBOXMANAGE" controlvm "$VM" natpf1 "ssh,tcp,127.0.0.1,2222,,22" >/dev/null 2>&1
		sleep 2
	fi
	if ! ssh_probe; then
		echo "!! SSH 仍然不通，sshd 可能卡死（升级过 openssh？重启虚拟机即可）"
		exit 1
	fi
fi

cd "$ROOT" || exit 1

# 源码内容指纹：没变化就跳过编译。
# 客户机时钟与宿主不同步，tar 保留的 mtime 到客户机上会"变旧"，不能靠 mtime 判断改动。
SRC_HASH=$(find src tools third_party rdef icon Makefile -type f -print0 2>/dev/null \
	| sort -z | xargs -0 md5sum 2>/dev/null | md5sum | cut -d' ' -f1)
if [ $FORCE -eq 0 ] && [ $RUN_AFTER -eq 0 ] && [ -f "$STATE" ] \
	&& [ "$(cat "$STATE" 2>/dev/null)" = "$SRC_HASH" ]; then
	echo "==> 源码未变化，跳过编译（--force 可强制）"
	exit 0
fi

echo "==> 推送源码 -> $HOST:$REMOTE_DIR"
tar -czf - \
	--exclude=.git --exclude=.workbuddy --exclude=build --exclude=build-host \
	--exclude='screenshot-*.png' --exclude='docs-screenshot.png' --exclude='*.wav' \
	src tools third_party rdef icon Makefile \
	| ssh -o BatchMode=yes "$HOST" "mkdir -p $REMOTE_DIR && cd $REMOTE_DIR && tar -xzf -"
if [ $? -ne 0 ]; then
	echo "!! 推送失败"
	exit 1
fi

# mtime 不可信，统一 touch 成"刚刚改过"，逼 make 真正编译
ssh -o BatchMode=yes "$HOST" "cd $REMOTE_DIR && find src tools third_party rdef icon Makefile -type f -exec touch {} + > /dev/null 2>&1; true"

echo "==> 编译"
ssh -o BatchMode=yes "$HOST" "cd $REMOTE_DIR && make -j4 > /tmp/build.log 2>&1; ec=\$?; grep -E 'error:|Error ' /tmp/build.log | head -20; grep -c 'warning:' /tmp/build.log | sed 's/^/warning 数: /'; tail -2 /tmp/build.log; exit \$ec"
BUILD_EC=$?
if [ $BUILD_EC -ne 0 ]; then
	echo "!! 编译失败 (exit=$BUILD_EC)，完整日志：ssh $HOST 'cat /tmp/build.log'"
	exit $BUILD_EC
fi

echo "==> 注入应用资源 (rdef/xres)"
ssh -o BatchMode=yes "$HOST" "cd $REMOTE_DIR && make rdef > /tmp/rdef.log 2>&1; ec=\$?; tail -2 /tmp/rdef.log; exit \$ec" || { echo "!! rdef 失败"; exit 1; }

echo "$SRC_HASH" > "$STATE"

if [ $RUN_AFTER -eq 1 ]; then
	echo "==> 重启虚拟机里的 HaikuNCM"
	# Haiku 没有 killall；ps 的列是 Team|Id|#Threads|Gid|Uid，PID 在第 2 列。
	# 必须 -n + </dev/null + 超时，否则后台进程会占住 SSH 通道把会话挂死
	timeout 90 ssh -n -o BatchMode=yes "$HOST" "cd $REMOTE_DIR && for p in \$(ps | grep -i build/HaikuNCM | grep -v grep | awk '{print \$2}'); do kill \$p 2>/dev/null; done; sleep 1; nohup ./build/HaikuNCM </dev/null >/tmp/ncm.log 2>&1 & sleep 3; ps | grep -i build/HaikuNCM | grep -v grep | head -2"
fi

echo "==> 完成"
