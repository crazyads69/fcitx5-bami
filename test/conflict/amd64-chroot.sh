#!/bin/bash
# Run a command in an amd64 rootfs on an aarch64 host WITHOUT root binfmt registration.
# Host here (Fedora Asahi) has a broken x86 binfmt-dispatcher (needs muvm), so podman
# --platform linux/amd64 cannot exec. Linux >=6.7 allows binfmt_misc per user namespace:
# we start a privileged ROOTLESS container, mount a private binfmt_misc, register
# qemu-x86_64-static there and chroot into the amd64 rootfs.
# usage: amd64-chroot.sh NAME IMAGE_DIGEST_REF 'shell command'   (env QEMU=path to qemu-x86_64-static)
#   rootfs is cached in $WORK/rootfs-NAME ; /src is bound read-only to the repo, /work rw to $WORK/NAME-work
set -euo pipefail
name=$1 ref=$2 cmd=$3
WORK=${WORK:-/tmp/bami-amd64}; QEMU=${QEMU:?path to static qemu-x86_64}
SRC=${SRC:-$(cd "$(dirname "$0")/../.." && pwd)}
mkdir -p "$WORK/rootfs-$name" "$WORK/$name-work"
if [ ! -e "$WORK/rootfs-$name/etc/os-release" ] && [ ! -e "$WORK/rootfs-$name/usr/lib/os-release" ]; then
  cid=$(podman create --platform linux/amd64 "$ref" /bin/true)   # use a @sha256 ref: never retags local arm64 tags
  podman export "$cid" | podman unshare tar -x -C "$WORK/rootfs-$name"; podman rm "$cid" >/dev/null
fi
podman run --rm --name "bami-amd64-$name-$$" --privileged \
  -v "$QEMU":/q:ro -v "$WORK/rootfs-$name":/rootfs -v "$SRC":/rootfs/src:ro -v "$WORK/$name-work":/rootfs/work \
  docker.io/library/debian:12 sh -c '
set -e
mount -t binfmt_misc none /proc/sys/fs/binfmt_misc
printf ":qemu-x86_64:M::\\x7fELF\\x02\\x01\\x01\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x02\\x00\\x3e\\x00:\\xff\\xff\\xff\\xff\\xff\\xff\\xfe\\x00\\xff\\xff\\xff\\xff\\xff\\xfe\\xff\\xff\\xff:/q:OCF" > /proc/sys/fs/binfmt_misc/register
mount -t proc proc /rootfs/proc; mount --rbind /dev /rootfs/dev; mount -t tmpfs tmpfs /rootfs/tmp
cp /etc/resolv.conf /rootfs/etc/resolv.conf
exec chroot /rootfs /bin/sh -c "$0"' "$cmd"
