#pragma once
#include <string>
#include <zlib.h>

std::string compress_string_zlib(const std::string& str);
std::string decompress_string_zlib(const std::string& str);

std::string compress_string_zstd(const std::string& str);
std::string decompress_string_zstd(const std::string& str);

std::string compress_string(const std::string& str);
std::string decompress_string(const std::string& str);

