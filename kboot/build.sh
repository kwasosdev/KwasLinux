#!/bin/bash
set -e

echo "==================================="
echo "KwasBoot for Kwas GNU/Linux builder."
echo "==================================="

ACTION=$1

if [ "$ACTION" == "iso" ]; then
    echo "Building ISO image..."
    
    echo "Compiling kboot.efi..."
    cd efi
    make clean || true
    make
    cd ..

    rm -rf iso_root
    mkdir -p iso_root

    echo "Creating efi.img (1024M)..."
    dd if=/dev/zero of=iso_root/efi.img bs=1M count=1024
    mkfs.vfat iso_root/efi.img
    mmd -i iso_root/efi.img ::/EFI
    mmd -i iso_root/efi.img ::/EFI/BOOT

    echo "Copying files into efi.img..."
    mcopy -i iso_root/efi.img efi/x86_64/kboot.efi ::/EFI/BOOT/BOOTX64.EFI
    mcopy -i iso_root/efi.img ../linux/arch/x86/boot/bzImage ::/bzImage
    mcopy -i iso_root/efi.img ../initramfs.cpio.gz ::/initramfs.cpio.gz
    mcopy -i iso_root/efi.img kboot.conf ::/EFI/BOOT/kboot.conf

    echo "Running xorriso..."
    xorriso -as mkisofs \
        -o ../kwaslinux.iso \
        -V "KWAS_LINUX" \
        -eltorito-alt-boot \
        -e efi.img \
        -no-emul-boot \
        -isohybrid-gpt-basdat \
        iso_root/

    rm -rf iso_root
    echo "UEFI ISO image created: kwaslinux.iso"
    exit 0
fi
