#!/bin/sh
# Haiku 端一次性引导：装公钥 + 确保 sshd 允许公钥登录，然后重启 sshd。
PUB="ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIONBFdOfg3QYYNW0TTQ5zv5FMHbUq9xY7tw+4SLcwZTv haikuncm-dev"
CFG=/boot/system/settings/ssh/sshd_config

mkdir -p /boot/home/.ssh
printf '%s\n' "$PUB" > /boot/home/.ssh/authorized_keys
chmod 700 /boot/home/.ssh
chmod 600 /boot/home/.ssh/authorized_keys

grep -q '^PubkeyAuthentication' "$CFG" 2>/dev/null || echo 'PubkeyAuthentication yes' >> "$CFG"
grep -q '^AuthorizedKeysFile' "$CFG" 2>/dev/null || echo 'AuthorizedKeysFile %h/.ssh/authorized_keys' >> "$CFG"

PID=$(ps | grep -w sshd | grep -v grep | head -1 | awk '{print $1}')
if [ -n "$PID" ]; then kill "$PID" 2>/dev/null; sleep 1; fi
/bin/sshd

echo "BOOTSTRAP_OK"
ls -l /boot/home/.ssh/
