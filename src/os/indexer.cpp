#include "indexer.h"
#include <windows.h>
#include <iostream>

std::string convert_to_utf8(const std::wstring& utf16_string) {
    if (utf16_string.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &utf16_string[0], (int)utf16_string.size(), NULL, 0, NULL, NULL);
    std::string utf8_string(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &utf16_string[0], (int)utf16_string.size(), &utf8_string[0], size_needed, NULL, NULL);
    return utf8_string;
}

void crawl_directory(const std::wstring& root_path, DataOrientedTrie& engine) {
    WIN32_FIND_DATAW findData;
    std::wstring searchPath = root_path + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        std::wstring fileName = findData.cFileName;
        // Skip current and parent directory markers
        if (fileName == L"." || fileName == L"..") continue;

        std::wstring fullPath = root_path + L"\\" + fileName;

        // Convert the UTF-16 Windows string to UTF-8
        std::string utf8_name = convert_to_utf8(fileName);
        std::string utf8_fullpath = convert_to_utf8(fullPath);
        
        // Insert it into the engine (works for both files AND folders!)
        engine.insertPointerNode(utf8_name, utf8_fullpath);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // It's a folder, recursively crawl inside it too
            crawl_directory(fullPath, engine);
        }
        
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);
}