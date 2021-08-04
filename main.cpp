#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include "detours.h"

extern "C" __declspec(dllexport) void dummyExport()
{
    printf("You are calling the dummy export\n");
    return;
}

typedef HINTERNET (WINAPI* internet_open_w_ptr)(LPCWSTR lpszAgent, DWORD dwAccessType, LPCWSTR lpszProxy, LPCWSTR lpszProxyBypass, DWORD dwFlags);
internet_open_w_ptr InternetOpenW_original;

HINTERNET internet_open_cache;

HINTERNET WINAPI InternetOpenW_hook(LPCWSTR lpszAgent, DWORD dwAccessType, LPCWSTR lpszProxy, LPCWSTR lpszProxyBypass, DWORD dwFlags)
{   
    if (!internet_open_cache)
    {
        internet_open_cache = InternetOpenW_original(lpszAgent, dwAccessType, lpszProxy, lpszProxyBypass, dwFlags);
    }

    return internet_open_cache;
}

typedef BOOL (WINAPI* internet_close_handle_ptr)(HINTERNET hInternet);
internet_close_handle_ptr InternetCloseHandle_original;

BOOL InternetCloseHandle_hook(HINTERNET hInternet)
{
    return TRUE;
}

typedef HINTERNET (WINAPI* internet_connect_w_ptr)(HINTERNET hInternet, LPCWSTR lpszServerName, INTERNET_PORT nServerPort, LPCWSTR lpszUserName, LPCWSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext);
internet_connect_w_ptr InternetConnectW_original;

HINTERNET internet_connect_cache;

HINTERNET WINAPI InternetConnectW_hook(HINTERNET hInternet, LPCWSTR lpszServerName, INTERNET_PORT nServerPort, LPCWSTR lpszUserName, LPCWSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext)
{
    if (!internet_connect_cache)
    {
        internet_connect_cache = InternetConnectW_original(hInternet, lpszServerName, nServerPort, lpszUserName, lpszPassword, dwService, dwFlags, dwContext);
    }

    return internet_connect_cache;
}

typedef HINTERNET (WINAPI* http_open_request_w_ptr)(HINTERNET hConnect, LPCWSTR lpszVerb, LPCWSTR lpszObjectName, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR *lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext);
http_open_request_w_ptr HttpOpenRequestW_original;

std::unordered_map<std::wstring, HINTERNET> request_lookup;

HINTERNET HttpOpenRequestW_hook(HINTERNET hConnect, LPCWSTR lpszVerb, LPCWSTR lpszObjectName, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR FAR *lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext)
{
    auto key = std::wstring(lpszObjectName);
    auto lookup = request_lookup.find(key);

    if (lookup == request_lookup.end())
    {
        printf("Caching: %ws\n", lpszObjectName);
        auto handle = HttpOpenRequestW_original(hConnect, lpszVerb, lpszObjectName, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, dwContext);
        request_lookup[key] = handle;
        return handle;
    }

    printf("Using cache for: %ws\n", lpszObjectName);
    return lookup->second;
}

typedef BOOL (WINAPI* http_send_request_ptr)(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength);
http_send_request_ptr HttpSendRequestW_original;

BOOL HttpSendRequestW_hook(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength)
{
    // Subsequent calls to HttpSendRequest don't adjust the content-length header for the same HttpOpenRequest handle (not documented on msdn!)
    wchar_t header[256];
    swprintf(header, sizeof(header) / sizeof(*header), L"Content-Length: %d\r\n", dwOptionalLength);
    HttpAddRequestHeadersW(hRequest, &header[0], -1, HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);

    printf("Sent http request, optional(size:%i)\n", dwOptionalLength);
    return HttpSendRequestW_original(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength);
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