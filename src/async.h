#pragma once
#include <windows.h>
#include <string>
#include <unordered_map>

void prefetch_requests(std::string login_result, 
    LPCWSTR pswzServerName, 
    WORD port, 
    LPCWSTR lpszVersion, 
    LPCWSTR lpszReferrer, 
    LPCWSTR* lplpszAcceptTypes, 
    DWORD dwFlags, 
    char api_version[5],
    std::unordered_map<std::wstring, std::unordered_map<int, std::string>>* results_lookup);

std::string to_hex(std::string& input);