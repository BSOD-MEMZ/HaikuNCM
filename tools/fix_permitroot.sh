#!/bin/sh
# 只改一处：PermitRootLogin yes（uid=0 的账号在 sshd 眼里就是 root）
# 改完不重启 sshd，由用户在 Network 偏好设置里开关一次服务，或重启虚拟机。
CFG=/boot/system/settings/ssh/sshd_config

echo "===== 改动前：生效值 ====="
/bin/sshd -T 2>&1 | grep -iE "^(permitrootlogin|pubkeyauthentication|authorizedkeysfile|passwordauthentication|strictmodes)"

cp "$CFG" /boot/home/sshd_config.bak 2>/dev/null && echo "(已备份 -> /boot/home/sshd_config.bak)"

# 把已有的 PermitRootLogin 行注释掉，再追加一行，避免"第一处生效"导致改不动
sed 's/^[[:space:]]*PermitRootLogin.*/#&/' "$CFG" > /tmp/cfg.new
printf 'PermitRootLogin yes\n' >> /tmp/cfg.new
cat /tmp/cfg.new > "$CFG"

echo
echo "===== 改动后：生效值 ====="
/bin/sshd -T 2>&1 | grep -iE "^(permitrootlogin|pubkeyauthentication|authorizedkeysfile|passwordauthentication|strictmodes)"

echo
echo "===== 相关配置原文 ====="
grep -n "PermitRootLogin" "$CFG"

echo
echo "FIX_DONE"
