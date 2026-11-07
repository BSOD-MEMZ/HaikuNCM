#!/bin/sh
# 恢复：把上一次脚本追加的两行从 sshd_config 里摘掉，校验配置，再拉 sshd
CFG=/boot/system/settings/ssh/sshd_config
cp "$CFG" /boot/home/sshd_config.bak 2>/dev/null && echo "已备份 -> /boot/home/sshd_config.bak"

grep -v '^PubkeyAuthentication yes$' "$CFG" \
	| grep -v '^AuthorizedKeysFile %h/\.ssh/authorized_keys$' > /tmp/cfg.new
if [ -s /tmp/cfg.new ]; then
	cp /tmp/cfg.new "$CFG"
	echo "已摘除追加行"
fi

echo
echo "===== 配置尾部 5 行 ====="
tail -5 "$CFG"

echo
echo "===== sshd -T 校验 ====="
if /bin/sshd -T >/tmp/t.out 2>/tmp/t.err; then
	echo "配置合法"
	grep -iE "^(authorizedkeysfile|pubkeyauthentication|passwordauthentication|strictmodes)" /tmp/t.out
else
	echo "配置有错:"
	cat /tmp/t.err
fi

echo
echo "===== 身份 ====="
id

echo
echo "===== 启动 sshd ====="
/bin/sshd
echo "sshd exit=$?"
ps | grep -w sshd | grep -v grep

echo
echo "RECOVER_DONE"
