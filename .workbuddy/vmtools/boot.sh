#!/bin/sh
# HaikuNCM 开发机一次性引导：补 sshd 主机密钥 + 装公钥 + 允许 root 公钥登录 + 重启 sshd
# 目标：让宿主机能通过 127.0.0.1:2222 免密登录，之后所有操作走 SSH。

PUB='ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIKquQ1dYKviOWT0NLQ3cbNwhb/AXwfg65y9ZvT/cCGg4 workbuddy@windows'
CFG=/boot/system/settings/ssh/sshd_config

echo '########## 1. 当前 sshd 生效配置 ##########'
/bin/sshd -T 2>&1 | grep -iE '^(hostkey|permitrootlogin|pubkeyauthentication|authorizedkeysfile|passwordauthentication|strictmodes)' || echo '(sshd -T 不可用)'

echo
echo '########## 2. 补主机密钥 ##########'
KEYS=$(/bin/sshd -T 2>/dev/null | grep -i '^hostkey ' | awk '{print $2}')
if [ -z "$KEYS" ]; then
	KEYS='/boot/system/settings/ssh/ssh_host_rsa_key /boot/system/settings/ssh/ssh_host_ecdsa_key /boot/system/settings/ssh/ssh_host_ed25519_key'
	echo '(sshd -T 没给出 HostKey，用默认路径)'
fi
for k in $KEYS; do
	if [ -f "$k" ]; then
		echo "  ok    $k"
		continue
	fi
	case "$k" in
		*_ed25519_key) ssh-keygen -q -t ed25519     -N '' -f "$k" ;;
		*_ecdsa_key)   ssh-keygen -q -t ecdsa -b 256 -N '' -f "$k" ;;
		*_dsa_key)     ssh-keygen -q -t rsa -b 2048  -N '' -f "$k" ;;
		*)             ssh-keygen -q -t rsa -b 2048  -N '' -f "$k" ;;
	esac
	if [ -f "$k" ]; then echo "  gen   $k"; else echo "  FAIL  $k"; fi
done
echo '-- ssh 设置目录 --'
ls -l /boot/system/settings/ssh/ | head -12

echo
echo '########## 3. 装公钥（两个位置都写） ##########'
mkdir -p /boot/home/.ssh /boot/home/config/settings/ssh
printf '%s\n' "$PUB" > /boot/home/.ssh/authorized_keys
printf '%s\n' "$PUB" > /boot/home/config/settings/ssh/authorized_keys
chmod 700 /boot/home/.ssh /boot/home/config/settings/ssh
chmod 600 /boot/home/.ssh/authorized_keys /boot/home/config/settings/ssh/authorized_keys
ls -ld /boot/home /boot/home/.ssh /boot/home/.ssh/authorized_keys

echo
echo '########## 4. 修 sshd_config ##########'
cp "$CFG" /boot/home/sshd_config.bak 2>/dev/null && echo '(已备份 -> /boot/home/sshd_config.bak)'
# 注释掉所有已有 PermitRootLogin，末尾追加唯一生效的一条
sed 's/^[[:space:]]*PermitRootLogin.*/#&/' "$CFG" > /tmp/cfg.new 2>/dev/null
printf 'PermitRootLogin yes\n' >> /tmp/cfg.new
if ! grep -q '^AuthorizedKeysFile' /tmp/cfg.new; then
	printf 'AuthorizedKeysFile %%h/.ssh/authorized_keys\n' >> /tmp/cfg.new
fi
if [ -s /tmp/cfg.new ]; then cat /tmp/cfg.new > "$CFG"; fi
grep -nE '^(AuthorizedKeysFile|PermitRootLogin)' "$CFG"

echo
echo '########## 5. 重启 sshd ##########'
for p in $(ps | grep -w sshd | grep -v grep | awk '{print $2}'); do
	kill "$p" 2>/dev/null
done
sleep 1
/bin/sshd
sleep 1
echo '-- 运行中的 sshd --'
ps | grep -w sshd | grep -v grep

echo
echo '########## 6. 最终校验 ##########'
/bin/sshd -T 2>&1 | grep -iE '^(hostkey|permitrootlogin|pubkeyauthentication|authorizedkeysfile)'
echo
echo 'BOOTSTRAP_DONE'
