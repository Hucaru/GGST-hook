#include "async.h"
#include <windows.h>
#include <winhttp.h>
#include <vector>
#include <thread>
#include <unordered_map>

std::vector<std::string> ggs_status_get_aobs = {
    {"96a007ffffffff"},
    {"96a009ffffffff"},
    {"96a008ff00ffff"},
    {"96a008ff01ffff"},
    {"96a008ff02ffff"},
    {"96a008ff03ffff"},
    {"96a008ff04ffff"},
    {"96a008ff05ffff"},
    {"96a008ff06ffff"},
    {"96a008ff07ffff"},
    {"96a008ff08ffff"},
    {"96a008ff09ffff"},
    {"96a008ff0affff"},
    {"96a008ff0bffff"},
    {"96a008ff0cffff"},
    {"96a008ff0dffff"},
    {"96a008ff0effff"},
    {"96a008ff0fffff"},
    {"96a008ffffffff"},
    {"96a006ff00ffff"},
    {"96a006ff01ffff"},
    {"96a006ff02ffff"},
    {"96a006ff03ffff"},
    {"96a006ff04ffff"},
    {"96a006ff05ffff"},
    {"96a006ff06ffff"},
    {"96a006ff07ffff"},
    {"96a006ff08ffff"},
    {"96a006ff09ffff"},
    {"96a006ff0affff"},
    {"96a006ff0bffff"},
    {"96a006ff0cffff"},
    {"96a006ff0dffff"},
    {"96a006ff0effff"},
    {"96a006ff0fffff"},
    {"96a006ffffffff"},
    {"96a005ffffffff"},
    {"96a0020100ffff"},
    {"96a0020101ffff"},
    {"96a0020102ffff"},
    {"96a0020103ffff"},
    {"96a0020104ffff"},
    {"96a0020105ffff"},
    {"96a0020106ffff"},
    {"96a0020107ffff"},
    {"96a0020108ffff"},
    {"96a0020109ffff"},
    {"96a002010affff"},
    {"96a002010bffff"},
    {"96a002010cffff"},
    {"96a002010dffff"},
    {"96a002010effff"},
    {"96a002010fffff"},
    {"96a00201ffffff"},
    {"96a0010100feff"},
    {"96a0010100ffff"},
    {"96a0010101feff"},
    {"96a0010101ffff"},
    {"96a0010102feff"},
    {"96a0010102ffff"},
    {"96a0010103feff"},
    {"96a0010103ffff"},
    {"96a0010104feff"},
    {"96a0010104ffff"},
    {"96a0010105feff"},
    {"96a0010105ffff"},
    {"96a0010106feff"},
    {"96a0010106ffff"},
    {"96a0010107feff"},
    {"96a0010107ffff"},
    {"96a0010108feff"},
    {"96a0010108ffff"},
    {"96a0010109feff"},
    {"96a0010109ffff"},
    {"96a001010afeff"},
    {"96a001010affff"},
    {"96a001010bfeff"},
    {"96a001010bffff"},
    {"96a001010cfeff"},
    {"96a001010cffff"},
    {"96a001010dfeff"},
    {"96a001010dffff"},
    {"96a001010efeff"},
    {"96a001010effff"},
    {"96a001010ffeff"},
    {"96a001010fffff"},
    {"96a00101fffeff"},
    {"96a00101ffffff"},
};

std::unordered_map<std::wstring, std::vector<std::string>> aobs = {
    {L"/api/statistics/get", ggs_status_get_aobs},
    {L"/api/catalog/get_block", std::vector<std::string>{"920101"}},
    {L"/api/catalog/get_follow", std::vector<std::string>{"93000101"}},
    {L"/api/lobby/get_vip_status", std::vector<std::string>{"91a0"}},
    {L"/api/item/get_item", std::vector<std::string>{"9105"}},
    {L"/api/catalog/get_replay", std::vector<std::string>{"940100059aff000963900c0e010001", "940100059aff000963900c01010001", "940100059aff000963900c0c020001"}},
};

static std::string generate_request(std::string& token, std::string& aob)
{
    return "data=9295" + token + "02a5"+"302e302e35" + "03" + aob + "\x00";
}

struct connection_context
{
    int request_id;
    int end_point_id;
    HANDLE request_sent;
    HANDLE read_complete;
    DWORD timeout;
    HINTERNET request_handle;
    std::wstring end_point;
    std::string request_payload;
    std::string read_buffer;
    std::string response_payload;
};

std::string to_hex(std::string& input)
{
    static const char hex_digits[] = "0123456789abcdef";

    std::string output;
    output.reserve(input.length() * 2);
    for (unsigned char c : input)
    {
        output.push_back(hex_digits[c >> 4]);
        output.push_back(hex_digits[c & 15]);
    }
    return output;
}

void async_callback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{
    if (dwContext != 0)
    {
        connection_context* ctx = (connection_context*)dwContext;

        switch(dwInternetStatus)
        {
            case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
                if(!WinHttpReceiveResponse(ctx->request_handle, NULL))
                {
                    printf("Request %d Error in receive response, code: %d\n", ctx->request_id, GetLastError());
                    SetEvent(ctx->read_complete);
                }
                break;
            case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
            {
                DWORD statusCode = 0;
                DWORD statusCodeSize = sizeof(DWORD);

                if (!WinHttpQueryHeaders(ctx->request_handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX))
                {
                    printf("Request %d failed to query headers\n", ctx->request_id);
                    SetEvent(ctx->read_complete);
                }

                if (HTTP_STATUS_OK != statusCode)
                {
                    printf("Request %d bad status\n", ctx->request_id);
                    SetEvent(ctx->read_complete);
                }

                if(!WinHttpQueryDataAvailable(ctx->request_handle, NULL))
                {
                    printf("Reques %d bad query data available\n", ctx->request_id);
                    SetEvent(ctx->read_complete);
                }
                break;
            }
            case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
                if (*((LPDWORD)lpvStatusInformation) != 0)
                {
                    DWORD buffer_size = *((LPDWORD)lpvStatusInformation) + 1;
                    ctx->read_buffer = std::string(buffer_size, '\0');

                    if (!WinHttpReadData(ctx->request_handle, &ctx->read_buffer[0], ctx->read_buffer.size(), 0))
                    {
                        printf("Request %d bad subsequent read data\n", ctx->request_id);
                        SetEvent(ctx->read_complete);
                    }
                }
                else
                {
                    SetEvent(ctx->read_complete);
                }
                break;
            case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
                if (dwStatusInformationLength != 0)
                {
                    ctx->response_payload.append(ctx->read_buffer.c_str(), dwStatusInformationLength);
                    ctx->read_buffer.clear();

                    if(!WinHttpQueryDataAvailable(ctx->request_handle, NULL))
                    {
                        printf("Request %d bad query data available\n", ctx->request_id);
                        SetEvent(ctx->read_complete);
                    }
                }
                break;        
        }
    }
}

void prefetch_requests(std::string login_result, 
    LPCWSTR pswzServerName, 
    WORD port, 
    LPCWSTR lpszVersion, 
    LPCWSTR lpszReferrer, 
    LPCWSTR* lplpszAcceptTypes, 
    DWORD dwFlags, 
    std::unordered_map<std::wstring, std::unordered_map<int, std::string>>* results)
{
    printf("Async pre-fetch started\n");

    HINTERNET session = WinHttpOpen(L"Steam", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);

    if (!session)
    {
        printf("Failed to make session\n");
        return;
    }

    WinHttpSetStatusCallback(session, async_callback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);

    HINTERNET connect = WinHttpConnect(session, pswzServerName, port, 0);

    if (!connect)
    {
        printf("Failed to make connect\n");
        WinHttpCloseHandle(session);
        return;
    }

    

    std::string token = login_result.substr(60, 19) + login_result.substr(2, 14);
    token = to_hex(token);

    auto context_size = 0;

    for (auto& end_points : aobs)
    {
        context_size += end_points.second.size();
    }

    printf("Creating %d contexts\n", context_size);
    std::vector<connection_context> contexts;
    contexts.reserve(context_size);

    auto i = 0;
    for (auto& end_points : aobs)
    {
        printf("Working on: %ws\n", end_points.first.c_str());
        auto j = 0;

        for (auto& payload : end_points.second)
        {
            connection_context ctx;
            ctx.request_id = i;
            ctx.end_point_id = j;
            ctx.read_complete = CreateEvent(NULL, FALSE, FALSE, NULL);
            ctx.timeout = 5000; // 5 seconds
            ctx.request_payload = generate_request(token, payload);
            ctx.end_point = end_points.first;
            ctx.request_handle = WinHttpOpenRequest(connect, L"POST", end_points.first.c_str(), NULL, WINHTTP_NO_REFERER, lplpszAcceptTypes, WINHTTP_FLAG_SECURE);

            if (!ctx.request_handle)
            {
                printf("Failed to open request\n");
                continue;
            }

            int headers_added = 0;

            if(WinHttpAddRequestHeaders(ctx.request_handle, L"Connection: keep-alive\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
            {
                headers_added++;
            }

            if(WinHttpAddRequestHeaders(ctx.request_handle, L"Content-Type: application/x-www-form-urlencoded\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
            {
                headers_added++;
            }

            if(WinHttpAddRequestHeaders(ctx.request_handle, L"Cache-Control: no-cache\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
            {
                headers_added++;
            }

            if(WinHttpAddRequestHeaders(ctx.request_handle, L"Cookie: theme=theme-dark\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
            {
                headers_added++;
            }

            if (WinHttpAddRequestHeaders(ctx.request_handle, L"User-Agent: Steam\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
            {
                headers_added++;
            }

            if (headers_added < 5)
            {
                printf("Failed to add headers\n");
                continue;
            }

            contexts.push_back(ctx);

            if(!WinHttpSendRequest(ctx.request_handle, 
                WINHTTP_NO_ADDITIONAL_HEADERS, -1L, 
                &contexts.back().request_payload[0], 
                contexts.back().request_payload.size() + 1, 
                contexts.back().request_payload.size() + 1, (DWORD_PTR)&contexts.back()))
            {
                printf("Failed to send request\n");
                continue;
            }
            
            j++;
            i++;
        }
    }

    for (auto& ctx : contexts)
    {
        WaitForSingleObject(ctx.read_complete, ctx.timeout);
        WinHttpCloseHandle(ctx.request_handle);

        auto ep = results->find(ctx.end_point);

        if (ep == results->end())
        {
            results->insert({ctx.end_point, std::unordered_map<int, std::string>{{ctx.end_point_id, ctx.response_payload}}});
        }
        else
        {
            ep->second.insert({ctx.end_point_id, ctx.response_payload});
        }
    }

    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    printf("Async pre-fetch ended\n");
}