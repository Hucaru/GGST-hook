#include <windows.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <thread>
#include <vector>
#include <wininet.h>
#include <stdio.h>
#include <thread>
#include <time.h>
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

bool prefetch = true;
bool stats_set;
bool stats_get;
bool login_state;
bool tus_write;
bool block_get;
bool follow_get;
bool replay_get;
bool vip_status;
bool item_get;
bool env_get;
bool lobby_get;
bool floor_get;

char api_versions[3][5];

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
    if (!internet_connect_cache || env_get)
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
    
    if (wcscmp(lpszObjectName, L"/api/statistics/get") == 0)
    {
        stats_get = true;

        if (login_state && prefetch)
        {
            prefetch_requests(login_result, server_name.c_str(), server_port, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, api_versions[0], &results_lookup);
            prefetch = false;
        }
    }
    else
    {
        stats_get = false;

        for (auto& r : request_count)
        {
            r.second = 0;
        }
    }

    stats_set = wcscmp(lpszObjectName, L"/api/statistics/set") == 0;
    tus_write = wcscmp(lpszObjectName, L"/api/tus/write") == 0;
    block_get = wcscmp(lpszObjectName, L"/api/catalog/get_block") == 0;
    follow_get = wcscmp(lpszObjectName, L"/api/catalog/get_follow") == 0;
    replay_get = wcscmp(lpszObjectName, L"/api/catalog/get_replay") == 0;
    vip_status = wcscmp(lpszObjectName, L"/api/lobby/get_vip_status") == 0;
    item_get = wcscmp(lpszObjectName, L"/api/item/get_item") == 0;
    env_get = wcscmp(lpszObjectName, L"/api/sys/get_env") == 0;
    lobby_get = wcscmp(lpszObjectName, L"/api/catalog/get_lobby") == 0;
    floor_get = wcscmp(lpszObjectName, L"/api/catalog/get_floor") == 0;

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

BOOL HttpSendRequestW_hook(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength)
{
    if (login_state)
    {
        if (stats_set || tus_write)
        {
            std::thread sender(send_request, hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength);
            sender.detach();
            return TRUE;
        }

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
    if (login_state)
    {
        if (stats_set)
        {
            char* tmp = (char*)lpBuffer;
            tmp[0] = 0xc8; // http status code: 200
            tmp[1] = 0x00;
            tmp[2] = 0x00;
            tmp[3] = 0x00;

            return TRUE;
        }

        auto end_point = results_lookup.find(current_request);

        if (end_point != results_lookup.end())
        {
            auto count = request_count.find(current_request);

            if (count != request_count.end())
            {
                auto response = end_point->second.find(count->second);

                if (response != end_point->second.end())
                {
                    char* tmp = (char*)lpBuffer;
                    tmp[0] = 0xc8; // http status code: 200
                    tmp[1] = 0x00;
                    tmp[2] = 0x00;
                    tmp[3] = 0x00;

                    return TRUE;
                }
            }
        }
    }

    return HttpQueryInfoW_original(hRequest, dwInfoLevel, lpBuffer, lpdwBufferLength, lpdwIndex);
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

void current_time(stat_set_response& stat_resp) {
    time_t now = time(0);
    tm  *tstruct = nullptr;
    char buf[80];
    tstruct = localtime(&now);

    if (tstruct)
    {
        strftime(&stat_resp.timestamp[0], sizeof(stat_resp.timestamp), "%Y/%m/%d %X", tstruct);
    }
}

stat_set_response generate_stat_set_response()
{
    stat_set_response r = {
        .header={'\x92', '\x98', '\xad', '\x36'},
        .random={}, // not sure what this is, varies greatly with each request
        .aob1={'\x00', '\xb3'},
        .timestamp={}, // human readable (UTC)
        .aob2={'\xa5'},
        .version1={}, // populated further down
        .aob3={'\xa5'},
        .version2={}, // populated further down
        .aob4={'\xa5'},
        .version3={}, // populated further down
        .aob5={'\xa0', '\xa0', '\x91', '\x00'}
    };

    current_time(r);

    memcpy(r.version1, &api_versions[0], 5);
    memcpy(r.version2, &api_versions[1], 5);
    memcpy(r.version3, &api_versions[2], 5);

    return r;
}

int count = 0;
BOOL InternetReadFile_hook(HINTERNET hFile, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead)
{
    count++;

    if (count % 2 == 0)
    {
        *lpdwNumberOfBytesRead = 0;
        return TRUE;
    }

    if (env_get)
    {
        auto r = InternetReadFile_original(hFile, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
        int version = -1;

        for (auto i = 0; i < *lpdwNumberOfBytesRead; i++)
        {
            if (((char*)lpBuffer)[i] == '\xa5')
            {
                version++;
                memcpy(&api_versions[version], ((char*)lpBuffer) + i + 1, 5);
                printf("API version %d: %.*s\n", version, (int)sizeof(api_versions[version]), api_versions[version]);
            }
        }

        return r;
    }

    if (login_state)
    {
        if (login_result.empty())
        {
            auto r = InternetReadFile_original(hFile, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
            login_result.resize(*lpdwNumberOfBytesRead);
            memcpy(&login_result[0], lpBuffer, *lpdwNumberOfBytesRead);
            return r;
        }

        if (stats_set || tus_write)
        {
            auto r = generate_stat_set_response();
            memcpy(lpBuffer, &r, sizeof(stat_set_response));
            *lpdwNumberOfBytesRead = sizeof(stat_set_response);
            return TRUE;
        }

        auto end_point = results_lookup.find(current_request);

        if (end_point != results_lookup.end())
        {
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
            }
        }
    }

    return InternetReadFile_original(hFile, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
}

BOOL IsDebuggerPresent_hook()
{
    printf("IsDebuggerPresent called\n");
    return FALSE;
}

BOOL apply_hook(__inout PVOID* ppvTarget, __in PVOID pvDetour, const char* name)
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

        freopen("CON", "w", stdout); // CON is windows device/file descriptor for current proc console, /dev/tty is the equivalent unix file descriptor

        char title[128];
        sprintf_s(title, "Attached to: %i", GetCurrentProcessId());
        SetConsoleTitleA(title);

        DisableThreadLibraryCalls(hinstDLL);

        return hook();
    }

    return TRUE;
}   