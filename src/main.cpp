#include <windows.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <thread>
#include <vector>
// #include <winhttp.h>
#include <wininet.h>
#include <stdio.h>
#include <thread>
#include "detours.h"
#include "async.h"

extern "C" __declspec(dllexport) void dummyExport()
{
    printf("You are calling the dummy export\n");
    return;
}

typedef HINTERNET (WINAPI* internet_open_w_ptr)(LPCWSTR lpszAgent, DWORD dwAccessType, LPCWSTR lpszProxy, LPCWSTR lpszProxyBypass, DWORD dwFlags);
internet_open_w_ptr InternetOpenW_original;

typedef BOOL (WINAPI* internet_close_handle_ptr)(HINTERNET hInternet);
internet_close_handle_ptr InternetCloseHandle_original;

typedef HINTERNET (WINAPI* internet_connect_w_ptr)(HINTERNET hInternet, LPCWSTR lpszServerName, INTERNET_PORT nServerPort, LPCWSTR lpszUserName, LPCWSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext);
internet_connect_w_ptr InternetConnectW_original;

typedef HINTERNET (WINAPI* http_open_request_w_ptr)(HINTERNET hConnect, LPCWSTR lpszVerb, LPCWSTR lpszObjectName, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR *lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext);
http_open_request_w_ptr HttpOpenRequestW_original;

typedef BOOL (WINAPI* http_send_request_ptr)(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength);
http_send_request_ptr HttpSendRequestW_original;

typedef BOOL (WINAPI* http_query_info_ptr)(HINTERNET hRequest, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex);
http_query_info_ptr HttpQueryInfoW_original;

typedef BOOL (WINAPI* internet_read_file_ptr)(HINTERNET hFile, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead);
internet_read_file_ptr InternetReadFile_original;

typedef BOOL (WINAPI* is_debugger_present_ptr)();
is_debugger_present_ptr IsDebuggerPresent_original;

bool stats_set;
bool stats_get;
bool login_state;
bool tus_write;
bool block_get;
bool follow_get;
bool replay_get;
bool vip_status;
bool item_get;

int stats_set_count = 0;
int stats_get_count = -1;

bool prefetch = true;

std::string login_result;

std::unordered_map<std::wstring, HINTERNET> request_lookup;
std::unordered_map<std::wstring, std::unordered_map<int, std::string>> results_lookup;

std::wstring current_request;
std::unordered_map<std::wstring, int> request_count;

HINTERNET internet_open_cache;
HINTERNET internet_connect_cache;

HINTERNET WINAPI InternetOpenW_hook(LPCWSTR lpszAgent, DWORD dwAccessType, LPCWSTR lpszProxy, LPCWSTR lpszProxyBypass, DWORD dwFlags)
{   
    if (!internet_open_cache)
    {
        internet_open_cache = InternetOpenW_original(lpszAgent, dwAccessType, lpszProxy, lpszProxyBypass, dwFlags);
    }

    return internet_open_cache;
}

BOOL InternetCloseHandle_hook(HINTERNET hInternet)
{
    return TRUE;
}

std::wstring server_name;
INTERNET_PORT server_port;

HINTERNET WINAPI InternetConnectW_hook(HINTERNET hInternet, LPCWSTR lpszServerName, INTERNET_PORT nServerPort, LPCWSTR lpszUserName, LPCWSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext)
{
    if (!internet_connect_cache)
    {
        internet_connect_cache = InternetConnectW_original(hInternet, lpszServerName, nServerPort, lpszUserName, lpszPassword, dwService, dwFlags, dwContext);
    }

    server_port = nServerPort;
    server_name = std::wstring(lpszServerName);

    return internet_connect_cache;
}

HINTERNET HttpOpenRequestW_hook(HINTERNET hConnect, LPCWSTR lpszVerb, LPCWSTR lpszObjectName, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR FAR *lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext)
{
    if (wcscmp(lpszObjectName, L"/api/user/login") == 0)
    {
        login_state = true;
    }
    else if (wcscmp(lpszObjectName, L"/api/catalog/get_lobby") == 0 || wcscmp(lpszObjectName, L"/api/sys/get_news") == 0)
    {
        login_state = false;
    }

    stats_set = wcscmp(lpszObjectName, L"/api/statistics/set") == 0;
    
    if (wcscmp(lpszObjectName, L"/api/statistics/get") == 0)
    {
        stats_get = true;

        if (login_state && prefetch)
        {
            prefetch_requests(login_result, server_name.c_str(), server_port, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, &results_lookup);
            prefetch = false;
        }
    }
    else
    {
        stats_get = false;
    }

    if (wcscmp(lpszObjectName, L"/api/tus/write") == 0)
    {
        tus_write = true;
    }
    else
    {
        tus_write = false;
    }
    
    if (wcscmp(lpszObjectName, L"/api/catalog/get_block") == 0)
    {
        block_get = true;
    }
    else
    {
        block_get = false;
    }

    if (wcscmp(lpszObjectName, L"/api/catalog/get_follow") == 0)
    {
        follow_get = true;
    }
    else
    {
        follow_get = false;
    }

    if (wcscmp(lpszObjectName, L"/api/catalog/get_replay") == 0)
    {
        replay_get = true;
    }
    else
    {
        replay_get = false;
    }

    if (wcscmp(lpszObjectName, L"/api/lobby/get_vip_status") == 0)
    {
        vip_status = true;
    }
    else
    {
        vip_status = false;
    }

    if (wcscmp(lpszObjectName, L"/api/item/get_item") == 0)
    {
        item_get = true;
    }
    else
    {
        item_get = false;
    }

    current_request = lpszObjectName;

    auto rc = request_count.find(lpszObjectName);

    if (rc == request_count.end())
    {
        request_count.insert({lpszObjectName, 0});
    }
    else
    {
        rc->second++;
    }

    auto lookup = request_lookup.find(current_request);

    if (lookup == request_lookup.end())
    {
        printf("Caching: %ws\n", lpszObjectName);
        auto handle = HttpOpenRequestW_original(hConnect, lpszVerb, lpszObjectName, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, dwContext);
        request_lookup[current_request] = handle;
        return handle;
    }

    printf("Using cache for: %ws\n", lpszObjectName);
    return lookup->second;
}

void send_request(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength)
{
    // Subsequent calls to HttpSendRequest don't adjust the content-length header for the same HttpOpenRequest handle (not documented on msdn!)
    wchar_t header[256];
    swprintf(header, sizeof(header) / sizeof(*header), L"Content-Length: %d\r\n", dwOptionalLength);
    HttpAddRequestHeadersW(hRequest, &header[0], -1, HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
    HttpAddRequestHeadersW(hRequest, L"Connection: keep-alive\r\n", -1, HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);

    printf("Sent http request (using thread), optional(size:%i)\n", dwOptionalLength);
    HttpSendRequestW_original(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength);
}

#include <iostream>
BOOL HttpSendRequestW_hook(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength)
{
    if (stats_set || tus_write)
    {
        std::thread sender(send_request, hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength);
        sender.detach();
        return TRUE;
    }

    // if (item_get)
    // {
    //     std::cout << std::string((char*)lpOptional, dwOptionalLength) << "\n\n";
    // }

    if (login_state)
    {
        auto end_point = results_lookup.find(current_request);

        if (end_point != results_lookup.end())
        {
            auto count = request_count.find(current_request);

            if (count != request_count.end())
            {
                auto response = end_point->second.find(count->second);

                if (response != end_point->second.end())
                {
                    return TRUE;
                }
            }    
        }
    }

    // Subsequent calls to HttpSendRequest don't adjust the content-length header for the same HttpOpenRequest handle (not documented on msdn!)
    wchar_t header[256];
    swprintf(header, sizeof(header) / sizeof(*header), L"Content-Length: %d\r\n", dwOptionalLength);
    HttpAddRequestHeadersW(hRequest, &header[0], -1, HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
    HttpAddRequestHeadersW(hRequest, L"Connection: keep-alive\r\n", -1, HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);

    printf("Sent http request, optional(size:%i)\n", dwOptionalLength);
    return HttpSendRequestW_original(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength);
}

BOOL HttpQueryInfoW_hook(HINTERNET hRequest, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex)
{
    // if (stats_set || ( (stats_get || follow_get) && login_state))
    // {
        char* tmp = (char*)lpBuffer;
        // No idea what this means, but the client is happy as long as it receives this
        tmp[0] = 0xc8;
        tmp[1] = 0x00;
        tmp[2] = 0x00;
        tmp[3] = 0x00;

        return TRUE;
    // }

    // return HttpQueryInfoW_original(hRequest, dwInfoLevel, lpBuffer, lpdwBufferLength, lpdwIndex);
}

struct stat_set_response
{
    char header[4];
    char random[12];
    char aob1[2];
    char timestamp[19];
    char aob2[1];
    char version1[5];
    char aob3[1];
    char version2[5];
    char aob4[1];
    char version3[5];
    char aob5[4];
};

stat_set_response generate_stat_set_response()
{
    stat_set_response r = {
        .header={'\x92', '\x98', '\xad', '\x36'},
        .random={}, // can leave empty
        .aob1={'\x00', '\xb3'},
        .timestamp={}, // human readable (UTC) can leave empty
        .aob2={'\xa5'},
        .version1={'\x30', '\x2e', '\x30', '\x2e', '\x35'},
        .aob3={'\xa5'},
        .version2={'\x30', '\x2e', '\x30', '\x2e', '\x32'},
        .aob4={'\xa5'},
        .version3={'\x30', '\x2e', '\x30', '\x2e', '\x32'},
        .aob5={'\xa0', '\xa0', '\x91', '\x00'}
    };

    return r;
}

int count = 0;
BOOL InternetReadFile_hook(HINTERNET hFile, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead)
{
    if (login_state && login_result.empty())
    {
        auto r = InternetReadFile_original(hFile, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
        login_result.resize(*lpdwNumberOfBytesRead);
        memcpy(&login_result[0], lpBuffer, *lpdwNumberOfBytesRead);
        return r;
    }

    if (login_state && stats_set)
    {
        if (stats_set_count % 2 == 0)
        {
            auto r = generate_stat_set_response();
            memcpy(lpBuffer, &r, sizeof(stat_set_response));
            printf("Using fabricated server response\n");
        }

        stats_set_count++;
        return TRUE;
    }

    if (login_state)
    {
        auto end_point = results_lookup.find(current_request);

        if (end_point != results_lookup.end())
        {
            count++;

            if (count % 2 == 0)
            {
                *lpdwNumberOfBytesRead = 0;
                return TRUE;
            }

            auto count = request_count.find(current_request);

            if (count != request_count.end())
            {
                auto response = end_point->second.find(count->second);

                if (response != end_point->second.end())
                {
                    // Client most likely frees this buffer at some point so best copy it to be safe
                    memcpy(lpBuffer, &response->second[0], response->second.size());
                    *lpdwNumberOfBytesRead = response->second.size();
                    return TRUE;
                }
                else
                {
                    printf("Failed to find payload for %ws with index\n", current_request.c_str(), count->second);
                }
            }
            printf("Failed to find count for %ws\n", current_request.c_str());
        }
    }

    return InternetReadFile_original(hFile, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
}

BOOL IsDebuggerPresent_hook()
{
    printf("IsDebuggerPresent called\n");
    return FALSE;
}

BOOL apply_hook(__inout PVOID* ppvTarget, __in PVOID pvDetour, char* name)
{
    printf("Hooking %s....", name);
    if (DetourTransactionBegin() != NO_ERROR)
    {
        printf("Failed (being detour)\n");
        return FALSE;
    }

    if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR)
    {
        printf("Failed (thread update failed)\n");
        return FALSE;
    }

    if (DetourAttach(ppvTarget, pvDetour) != NO_ERROR)
    {
        printf("Failed (detour attach)\n");
        return FALSE;
    }

    if (DetourTransactionCommit() != NO_ERROR)
    {
        printf("Failed (detour transaction commit)\n");
        DetourTransactionAbort();
        return FALSE;
    }

     printf("success\n", name);
    
    return TRUE;
}

BOOL hook()
{
    InternetOpenW_original = &InternetOpenW;
    if (!apply_hook((PVOID*)&InternetOpenW_original, (PVOID)InternetOpenW_hook, "InternetOpenW"))
    {
        return FALSE;
    }

    InternetCloseHandle_original = &InternetCloseHandle;
    if (!apply_hook((PVOID*)&InternetCloseHandle_original, (PVOID)InternetCloseHandle_hook, "InternetCloseHandle"))
    {
        return FALSE;
    }

    InternetConnectW_original = &InternetConnectW;
    if (!apply_hook((PVOID*)&InternetConnectW_original, (PVOID)InternetConnectW_hook, "InternetConnectW"))
    {
        return FALSE;
    }

    HttpOpenRequestW_original = &HttpOpenRequestW;
    if (!apply_hook((PVOID*)&HttpOpenRequestW_original, (PVOID)HttpOpenRequestW_hook, "HttpOpenRequestW"))
    {
        return FALSE;
    }

    HttpSendRequestW_original = &HttpSendRequestW;
    if (!apply_hook((PVOID*)&HttpSendRequestW_original, (PVOID)HttpSendRequestW_hook, "HttpSendRequestW"))
    {
        return FALSE;
    }

    HttpQueryInfoW_original = &HttpQueryInfoW;
    if (!apply_hook((PVOID*)&HttpQueryInfoW_original, (PVOID)HttpQueryInfoW_hook, "HttpQueryInfoW"))
    {
        return FALSE;
    }

    InternetReadFile_original = &InternetReadFile;
    if (!apply_hook((PVOID*)&InternetReadFile_original, (PVOID)InternetReadFile_hook, "InternetReadFile"))
    {
        return FALSE;
    }

    IsDebuggerPresent_original = &IsDebuggerPresent;
    if (!apply_hook((PVOID*)&IsDebuggerPresent_original, (PVOID)IsDebuggerPresent_hook, "IsDebuggerPresent"))
    {
        return FALSE;
    }

    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        AllocConsole();
        AttachConsole(GetCurrentProcessId());

        freopen("CON", "w", stdout);

        char title[128];
        sprintf_s(title, "Attached to: %i", GetCurrentProcessId());
        SetConsoleTitleA(title);

        DisableThreadLibraryCalls(hinstDLL);

        return hook();
    }

    return TRUE;
}   