#!/bin/sh
# 诊断：让 sshd 打印生效配置 + 看进程和目录属性
echo "===== id ====="
id

echo
echo "===== sshd -T (生效配置) ====="
if /bin/sshd -T 2>/tmp/t.err | grep -iE "^(authorizedkeysfile|pubkeyauthentication|passwordauthentication|strictmodes|permitrootlogin|usedns|usepam|kbdinteractive)"; then
	:
else
	echo "-- sshd -T 不可用:"
	cat /tmp/t.err
	echo "-- 配置原文中的相关行:"
	grep -nvE "^\s*#|^$" /boot/system/settings/ssh/sshd_config | head -40
fi

echo
echo "===== 目录/文件属性 ====="
ls -ld /boot/home /boot/home/.ssh /boot/home/.ssh/authorized_keys

echo
echo "===== 运行中的 sshd ====="
ps | grep -w sshd | grep -v grep

echo
echo "===== 公钥内容(前 40 字节) ====="
cut -c1-40 /boot/home/.ssh/authorized_keys

echo
echo "DIAG_DONE"
