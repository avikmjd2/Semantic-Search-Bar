#pragma once
#include <string>
#include "../engine/searcher.h"


std::string convert_to_utf8(const std::wstring& utf16_string);
void crawl_directory(const std::wstring& root_path, DataOrientedTrie& engine);