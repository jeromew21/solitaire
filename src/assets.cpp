#include "assets.h"

#include <physfs.h>

#include <filesystem>
#include <iostream>

bool physfs_init(const char *argv0) {
    if (!PHYSFS_init(argv0)) {
        std::fprintf(stderr, "PHYSFS_init: %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return false;
    }

    const char *exe_dir = PHYSFS_getBaseDir();
    auto assets_dir = (std::filesystem::path(exe_dir) / "assets").string();
    if (!PHYSFS_mount(assets_dir.c_str(), nullptr, 1)) {
        std::fprintf(stderr, "PHYSFS_mount: %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        PHYSFS_deinit();
        return false;
    }

    return true;
}

std::vector<unsigned char> physfs_read_file(const char *path) {
    PHYSFS_File *file = PHYSFS_openRead(path);
    if (!file) {
        std::fprintf(stderr, "Open failed: %s\n", path);
        return {};
    }

    const PHYSFS_sint64 size = PHYSFS_fileLength(file);
    if (size <= 0) {
        PHYSFS_close(file);
        return {};
    }

    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    const PHYSFS_sint64 read = PHYSFS_readBytes(file, bytes.data(), static_cast<PHYSFS_uint64>(size));
    PHYSFS_close(file);

    if (read != size) {
        std::fprintf(stderr, "Read failed: %s\n", path);
        return {};
    }
    return bytes;
}
