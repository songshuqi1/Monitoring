#pragma once

#include <cstddef>
#include <string>

struct EmbeddedFrontendAsset {
    const char* path;
    const char* contentType;
    const unsigned char* data;
    std::size_t size;
};

bool embeddedFrontendAvailable();
const EmbeddedFrontendAsset* findEmbeddedFrontendAsset(const std::string& path);
