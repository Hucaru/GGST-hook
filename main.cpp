#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include "detours.h"

extern "C" __declspec(dllexport) void dummyExport()
{
    printf("You are calling the dummy export\n");
    return;
}

int connection_counter = 0;
bool login_state = true;

typedef HINTERNET (WINAPI* internet_open_w_ptr)(LPCWSTR lpszAgent, DWORD dwAccessType, LPCWSTR lpszProxy, LPCWSTR lpszProxyBypass, DWORD dwFlags);
internet_open_w_ptr InternetOpenW_original;

HINTERNET internet_open_cache;

HINTERNET WINAPI InternetOpenW_hook(LPCWSTR lpszAgent, DWORD dwAccessType, LPCWSTR lpszProxy, LPCWSTR lpszProxyBypass, DWORD dwFlags)
{   
    if (login_state)
    {
        if (lstrcmpW(lpszAgent, L"Steam") == 0)
        {
            printf("\n\nConnection - %i\n", connection_counter);
            if (!internet_open_cache)
            {
                printf("Caching internet open\n");
                internet_open_cache = InternetOpenW_original(lpszAgent, dwAccessType, lpszProxy, lpszProxyBypass, dwFlags);
            }
            else
            {
                printf("Using internet open cache\n");
            }
        }

        return internet_open_cache;
    }

    return InternetOpenW_original(lpszAgent, dwAccessType, lpszProxy, lpszProxyBypass, dwFlags);
}

typedef BOOL (WINAPI* internet_close_handle_ptr)(HINTERNET hInternet);
internet_close_handle_ptr InternetCloseHandle_original;

BOOL InternetCloseHandle_hook(HINTERNET hInternet)
{
    connection_counter++;
    
    if (login_state)
    {
        return TRUE;
    }
    else
    {
        return InternetCloseHandle_original(hInternet);
    }
}

typedef HINTERNET (WINAPI* internet_connect_w_ptr)(HINTERNET hInternet, LPCWSTR lpszServerName, INTERNET_PORT nServerPort, LPCWSTR lpszUserName, LPCWSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext);
internet_connect_w_ptr InternetConnectW_original;

HINTERNET internet_connect_cache;

HINTERNET WINAPI InternetConnectW_hook(HINTERNET hInternet, LPCWSTR lpszServerName, INTERNET_PORT nServerPort, LPCWSTR lpszUserName, LPCWSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext)
{
    if (login_state)
    {
        if (lstrcmpW(lpszServerName, L"ggst-game.guiltygear.com") == 0 && nServerPort == 443)
        {
            if (!internet_connect_cache)
            {
                printf("Caching internet connect\n");
                internet_connect_cache = InternetConnectW_original(hInternet, lpszServerName, nServerPort, lpszUserName, lpszPassword, dwService, dwFlags, dwContext);
            }
            else
            {
                printf("Using internet connect cache\n");
            }

            return internet_connect_cache;
        }
    }

    return InternetConnectW_original(hInternet, lpszServerName, nServerPort, lpszUserName, lpszPassword, dwService, dwFlags, dwContext);
}

typedef HINTERNET (WINAPI* http_open_request_w_ptr)(HINTERNET hConnect, LPCWSTR lpszVerb, LPCWSTR lpszObjectName, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR *lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext);
http_open_request_w_ptr HttpOpenRequestW_original;

HINTERNET request_cache_statistics_get;
HINTERNET request_cache_statistics_set;
HINTERNET request_cache_catalog_get_replay;

bool statistics_set = false;

HINTERNET HttpOpenRequestW_hook(HINTERNET hConnect, LPCWSTR lpszVerb, LPCWSTR lpszObjectName, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR FAR *lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext)
{
    if (lstrcmpW(lpszVerb, L"POST") == 0 && lstrcmpW(lpszObjectName, L"/api/user/login") == 0)
    {
        login_state = true; // when quick matching this is triggered again
    }
    else if (lstrcmpW(lpszVerb, L"POST") == 0 && (lstrcmpW(lpszObjectName, L"/api/catalog/get_lobby") == 0 || lstrcmpW(lpszObjectName, L"/api/sys/get_news") == 0))
    {
        login_state = false;
        request_cache_statistics_get = NULL;
        request_cache_statistics_set = NULL;
        request_cache_catalog_get_replay = NULL;
        printf("\nResuming normal execution\n"); // Should probably unhook
    }

    statistics_set = (lstrcmpW(lpszVerb, L"POST") == 0 && lstrcmpW(lpszObjectName, L"/api/statistics/set") == 0);

    if (login_state)
    {
        if (lstrcmpW(lpszVerb, L"POST") == 0 && lstrcmpW(lpszObjectName, L"/api/statistics/get") == 0)
        {
            if (!request_cache_statistics_get)
            {
                printf("Caching http open request for %ws %ws\n", lpszVerb, lpszObjectName);
                request_cache_statistics_get = HttpOpenRequestW_original(hConnect, lpszVerb, lpszObjectName, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, dwContext);
            }
            else
            {
                printf("Using http open request cache for /api/statistics/get\n");
            }

            return request_cache_statistics_get;
        }
        
        else if (lstrcmpW(lpszVerb, L"POST") == 0 && lstrcmpW(lpszObjectName, L"/api/statistics/set") == 0)
        {
            if (!request_cache_statistics_set)
            {
                printf("Caching http open request for %ws %ws\n", lpszVerb, lpszObjectName);
                request_cache_statistics_set = HttpOpenRequestW_original(hConnect, lpszVerb, lpszObjectName, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, dwContext);
            }
            else
            {
                printf("Using http open request cache for /api/statistics/set\n");
            }

            return request_cache_statistics_set;
        }
        else if (lstrcmpW(lpszVerb, L"POST") == 0 && lstrcmpW(lpszObjectName, L"/api/catalog/get_replay") == 0)
        {
            if (!request_cache_catalog_get_replay)
            {
                printf("Caching http open request for %ws %ws\n", lpszVerb, lpszObjectName);
                request_cache_catalog_get_replay = HttpOpenRequestW_original(hConnect, lpszVerb, lpszObjectName, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, dwContext);
            }
            else
            {
                printf("Using http open request cache for /api/catalog/get_replay\n");
            }

            return request_cache_catalog_get_replay;
        }
    }

    printf("Creating HTTP request: %ws %ws\n", lpszVerb, lpszObjectName);
    return HttpOpenRequestW_original(hConnect, lpszVerb, lpszObjectName, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, dwContext);
}

typedef BOOL (WINAPI* http_send_request_ptr)(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength);
http_send_request_ptr HttpSendRequestW_original;

BOOL HttpSendRequestW_hook(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength)
{
    if (statistics_set)
    {
        // Subsequent calls to HttpSendRequest don't adjust the content-length header for the same HttpOpenRequest handle (not documented on msdn!)
        wchar_t header[256];
        swprintf(header, sizeof(header) / sizeof(*header), L"Content-Length: %d\r\n", dwOptionalLength);
        HttpAddRequestHeadersW(request_cache_statistics_set, &header[0], -1, HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
    }

    printf("Send http request, optional(size:%i)\n", dwOptionalLength);
    return HttpSendRequestW_original(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength);
}

typedef BOOL (WINAPI* http_query_info_ptr)(HINTERNET hRequest, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex);
http_query_info_ptr HttpQueryInfoW_original;

BOOL HttpQueryInfoW_hook(HINTERNET hRequest, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex)
{
    return HttpQueryInfoW_original(hRequest, dwInfoLevel, lpBuffer, lpdwBufferLength, lpdwIndex);
}

typedef BOOL (WINAPI* internet_read_file_ptr)(HINTERNET hFile, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead);
internet_read_file_ptr InternetReadFile_original;

BOOL InternetReadFile_hook(HINTERNET hFile, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead)
{
    BOOL result = InternetReadFile_original(hFile, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);

    if ((*lpdwNumberOfBytesRead == dwNumberOfBytesToRead) && dwNumberOfBytesToRead> 0)
    {
        printf("Http response(size:%i)\n", dwNumberOfBytesToRead);
    }
    
    return result;
}

typedef BOOL (WINAPI* is_debugger_present_ptr)();
is_debugger_present_ptr IsDebuggerPresent_original;

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