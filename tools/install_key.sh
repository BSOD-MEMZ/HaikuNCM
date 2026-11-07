#!/bin/sh
# 只做一件事：装公钥。不重启 sshd、不改任何系统配置。
PUB="ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIONBFdOfg3QYYNW0TTQ5zv5FMHbUq9xY7tw+4SLcwZTv haikuncm-dev"

mkdir -p /boot/home/.ssh
printf '%s\n' "$PUB" > /boot/home/.ssh/authorized_keys
chmod 700 /boot/home/.ssh
chmod 600 /boot/home/.ssh/authorized_keys

echo "===== 结果 ====="
ls -ld /boot/home /boot/home/.ssh /boot/home/.ssh/authorized_keys
echo "===== 身份 ====="
id
echo "===== sshd 进程 ====="
ps | grep -w sshd | grep -v grep
echo "KEY_INSTALLED"
