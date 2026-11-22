#pragma once

#include <vector>

bool physfs_init(const char *argv0);

std::vector<unsigned char> physfs_read_file(const char *path);
