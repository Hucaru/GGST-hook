#pragma once
#include <windows.h>
#include <string>
#include <unordered_map>

void fetch_status(std::string login_result, LPCWSTR pswzServerName, WORD port, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR* lplpszAcceptTypes, DWORD dwFlags, std::unordered_map<int, std::string>* results);