#include <efi.h>

static EFI_GUID loaded_image_guid = { 0x5B1B31A1, 0x9562, 0x11d2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
static EFI_GUID fs_guid          = { 0x0964E5B22, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
static EFI_GUID file_info_guid   = { 0x09576E92, 0x6D3F, 0x11d2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };

UINTN K_StrLen(CHAR16 *str) {
    UINTN len = 0;
    while (str[len]) len++;
    return len;
}

void K_CopyMem(void *dest, void *src, UINTN len) {
    UINT8 *d = (UINT8*)dest;
    UINT8 *s = (UINT8*)src;
    for (UINTN i = 0; i < len; i++) d[i] = s[i];
}

CHAR8* K_StrStr(CHAR8 *haystack, CHAR8 *needle) {
    if (!*needle) return haystack;
    for (; *haystack; ++haystack) {
        if (*haystack == *needle) {
            CHAR8 *h = haystack;
            CHAR8 *n = needle;
            while (*h && *n && *h == *n) { h++; n++; }
            if (!*n) return haystack;
        }
    }
    return NULL;
}

void AsciiToUnicode(CHAR8 *ascii, CHAR16 *unicode) {
    while (*ascii) {
        *unicode++ = (CHAR16)*ascii++;
    }
    *unicode = 0;
}

EFI_STATUS LoadKbootConfig(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE *RootDir, CHAR16 *OutCmdLine, UINTN MaxSize) {
    EFI_FILE *ConfFile;
    EFI_STATUS Status = RootDir->Open(RootDir, &ConfFile, L"kboot.conf", EFI_FILE_MODE_READ, 0);
    
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = sizeof(EFI_FILE_INFO) + 256;
    SystemTable->BootServices->AllocatePool(EfiLoaderData, InfoSize, (void**)&FileInfo);
    ConfFile->GetInfo(ConfFile, &file_info_guid, &InfoSize, FileInfo);
    UINTN ConfSize = FileInfo->FileSize;
    SystemTable->BootServices->FreePool(FileInfo);

    if (ConfSize == 0 || ConfSize > 4096) {
        ConfFile->Close(ConfFile);
        return EFI_LOAD_ERROR;
    }

    CHAR8 *Buffer;
    SystemTable->BootServices->AllocatePool(EfiLoaderData, ConfSize + 1, (void**)&Buffer);
    UINTN ReadSize = ConfSize;
    ConfFile->Read(ConfFile, &ReadSize, Buffer);
    Buffer[ConfSize] = '\0';
    ConfFile->Close(ConfFile);

    CHAR8 *cmdline_ptr = K_StrStr(Buffer, "cmdline=");
    if (cmdline_ptr) {
        cmdline_ptr += 8;
        
        CHAR8 *end = cmdline_ptr;
        while (*end && *end != '\r' && *end != '\n') end++;
        *end = '\0';
        
        AsciiToUnicode(cmdline_ptr, OutCmdLine);
        Status = EFI_SUCCESS;
    } else {
        Status = EFI_NOT_FOUND;
    }

    SystemTable->BootServices->FreePool(Buffer);
    return Status;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    SystemTable->ConOut->Reset(SystemTable->ConOut, FALSE);

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"===================================\r\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"      Welcome to Kwas Boot!        \r\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"===================================\r\n\r\n");

    EFI_LOADED_IMAGE *LoadedImage;
    EFI_STATUS Status = SystemTable->BootServices->HandleProtocol(ImageHandle, &loaded_image_guid, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to get LoadedImage\r\n");
        while(1) {}
    }

    EFI_FILE_IO_INTERFACE *FileSystem;
    Status = SystemTable->BootServices->HandleProtocol(LoadedImage->DeviceHandle, &fs_guid, (void**)&FileSystem);
    if (EFI_ERROR(Status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to get FileSystem\r\n");
        while(1) {}
    }

    EFI_FILE *RootDir;
    Status = FileSystem->OpenVolume(FileSystem, &RootDir);
    if (EFI_ERROR(Status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to open volume\r\n");
        while(1) {}
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Looking for bzImage...\r\n");
    EFI_FILE *KernelFile;
    Status = RootDir->Open(RootDir, &KernelFile, L"bzImage", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Could not open bzImage!\r\n");
        while(1) {}
    }

    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = sizeof(EFI_FILE_INFO) + 256;
    SystemTable->BootServices->AllocatePool(EfiLoaderData, InfoSize, (void**)&FileInfo);
    KernelFile->GetInfo(KernelFile, &file_info_guid, &InfoSize, FileInfo);
    UINTN KernelSize = FileInfo->FileSize;
    SystemTable->BootServices->FreePool(FileInfo);

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Kernel found! Loading...\r\n");

    VOID *KernelBuffer;
    SystemTable->BootServices->AllocatePool(EfiLoaderData, KernelSize, &KernelBuffer);
    KernelFile->Read(KernelFile, &KernelSize, KernelBuffer);
    KernelFile->Close(KernelFile);

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Starting kernel...\r\n");
    EFI_HANDLE KernelHandle;
    Status = SystemTable->BootServices->LoadImage(FALSE, ImageHandle, NULL, KernelBuffer, KernelSize, &KernelHandle);
    if (EFI_ERROR(Status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Failed to LoadImage!\r\n");
        while(1) {}
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Reading kboot.conf...\r\n");
    
    CHAR16 CmdLineBuffer[512];
    AsciiToUnicode(
        "console=tty0 console=ttyS0,115200 nvidia_drm.modeset=1 nvidia_drm.fbdev=1 rdinit=/init initrd=\\initramfs.cpio.gz", 
        CmdLineBuffer
    );

    if (EFI_ERROR(LoadKbootConfig(SystemTable, RootDir, CmdLineBuffer, 512))) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Warning: kboot.conf not found or invalid. Using default.\r\n");
    } else {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Config loaded successfully.\r\n");
    }

    EFI_LOADED_IMAGE *KernelLoadedImage;
    SystemTable->BootServices->HandleProtocol(KernelHandle, &loaded_image_guid, (void**)&KernelLoadedImage);
    KernelLoadedImage->DeviceHandle = LoadedImage->DeviceHandle;

    KernelLoadedImage->LoadOptionsSize = (K_StrLen(CmdLineBuffer) + 1) * sizeof(CHAR16);
    SystemTable->BootServices->AllocatePool(EfiLoaderData, KernelLoadedImage->LoadOptionsSize, &KernelLoadedImage->LoadOptions);
    K_CopyMem(KernelLoadedImage->LoadOptions, CmdLineBuffer, KernelLoadedImage->LoadOptionsSize);

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Booting Kwasik...\r\n");
    Status = SystemTable->BootServices->StartImage(KernelHandle, NULL, NULL);

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Kernel returned control!\r\n");
    while(1) {}

    return EFI_SUCCESS;
}
