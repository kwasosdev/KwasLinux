#!/bin/bash
set -e

echo "=============================="
echo "ISO builder for Kwas GNU/Linux"
echo "=============================="

echo "Preparing iso_root..."
rm -rf iso_root
mkdir -p iso_root/boot/grub

# Копируем ядро и initramfs (проверьте пути: если скрипт лежит в корне, то linux/... и initramfs.cpio.gz)
cp linux/arch/x86/boot/bzImage iso_root/boot/bzImage
cp initramfs.cpio.gz iso_root/boot/initramfs.cpio.gz

# Копируем конфигурацию GRUB
cp configs/grub.cfg iso_root/boot/grub/grub.cfg

echo "Building ISO with grub-mkrescue..."
grub-mkrescue -o kwaslinux.iso iso_root -- -volid "KWAS_LINUX"

rm -rf iso_root
echo "Hybrid BIOS/UEFI ISO created: kwaslinux.iso"

