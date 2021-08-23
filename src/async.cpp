#include "async.h"
#include <windows.h>
#include <winhttp.h>
#include <vector>
#include <thread>
#include <unordered_map>

std::vector<std::string> ggs_get_aobs = {
    {"a007ffffffff"},
    {"a009ffffffff"},
    {"a008ff00ffff"},
    {"a008ff01ffff"},
    {"a008ff02ffff"},
    {"a008ff03ffff"},
    {"a008ff04ffff"},
    {"a008ff05ffff"},
    {"a008ff06ffff"},
    {"a008ff07ffff"},
    {"a008ff08ffff"},
    {"a008ff09ffff"},
    {"a008ff0affff"},
    {"a008ff0bffff"},
    {"a008ff0cffff"},
    {"a008ff0dffff"},
    {"a008ff0effff"},
    {"a008ff0fffff"},
    {"a008ffffffff"},
    {"a006ff00ffff"},
    {"a006ff01ffff"},
    {"a006ff02ffff"},
    {"a006ff03ffff"},
    {"a006ff04ffff"},
    {"a006ff05ffff"},
    {"a006ff06ffff"},
    {"a006ff07ffff"},
    {"a006ff08ffff"},
    {"a006ff09ffff"},
    {"a006ff0affff"},
    {"a006ff0bffff"},
    {"a006ff0cffff"},
    {"a006ff0dffff"},
    {"a006ff0effff"},
    {"a006ff0fffff"},
    {"a006ffffffff"},
    {"a005ffffffff"},
    {"a0020100ffff"},
    {"a0020101ffff"},
    {"a0020102ffff"},
    {"a0020103ffff"},
    {"a0020104ffff"},
    {"a0020105ffff"},
    {"a0020106ffff"},
    {"a0020107ffff"},
    {"a0020108ffff"},
    {"a0020109ffff"},
    {"a002010affff"},
    {"a002010bffff"},
    {"a002010cffff"},
    {"a002010dffff"},
    {"a002010effff"},
    {"a002010fffff"},
    {"a00201ffffff"},
    {"a0010100feff"},
    {"a0010100ffff"},
    {"a0010101feff"},
    {"a0010101ffff"},
    {"a0010102feff"},
    {"a0010102ffff"},
    {"a0010103feff"},
    {"a0010103ffff"},
    {"a0010104feff"},
    {"a0010104ffff"},
    {"a0010105feff"},
    {"a0010105ffff"},
    {"a0010106feff"},
    {"a0010106ffff"},
    {"a0010107feff"},
    {"a0010107ffff"},
    {"a0010108feff"},
    {"a0010108ffff"},
    {"a0010109feff"},
    {"a0010109ffff"},
    {"a001010afeff"},
    {"a001010affff"},
    {"a001010bfeff"},
    {"a001010bffff"},
    {"a001010cfeff"},
    {"a001010cffff"},
    {"a001010dfeff"},
    {"a001010dffff"},
    {"a001010efeff"},
    {"a001010effff"},
    {"a001010ffeff"},
    {"a001010fffff"},
    {"a00101fffeff"},
    {"a00101ffffff"},
};

static std::string generate_stat_get_request(std::string& token, std::string& get_aob)
{
    return "data=9295" + token + "02a5"+"302e302e35" + "0396" + get_aob + "\x00";
}

struct connection_context
{
    int request_id;
    HANDLE request_sent;
    HANDLE data_available;
    HANDLE headers_available;
    HANDLE read_complete;
    DWORD timeout;
    DWORD read_amount;
    HINTERNET request_handle;
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

void fetch_status(std::string login_result, LPCWSTR pswzServerName, WORD port, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR* lplpszAcceptTypes, DWORD dwFlags, std::unordered_map<int, std::string>* results)
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

    std::vector<connection_context> contexts;
    contexts.reserve(ggs_get_aobs.size());

    std::string token = login_result.substr(60, 19) + login_result.substr(2, 14);
    token = to_hex(token);

    for (auto i = 0; i < ggs_get_aobs.size(); i++)
    {
        connection_context ctx;
        contexts.push_back(ctx);
        contexts.back().request_id = i;
        contexts.back().read_complete = CreateEvent(NULL, FALSE, FALSE, NULL);
        contexts.back().timeout = 5000; // 5 seconds
        contexts.back().request_payload = generate_stat_get_request(token, ggs_get_aobs[i]);
        contexts.back().request_handle = WinHttpOpenRequest(connect, L"POST", L"/api/statistics/get", NULL, WINHTTP_NO_REFERER, lplpszAcceptTypes, WINHTTP_FLAG_SECURE);

        if (!contexts.back().request_handle)
        {
            printf("Failed to open request\n");
            continue;
        }

        int headers_added = 0;

        if(WinHttpAddRequestHeaders(contexts.back().request_handle, L"Connection: keep-alive\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
        {
            headers_added++;
        }

        if(WinHttpAddRequestHeaders(contexts.back().request_handle, L"Content-Type: application/x-www-form-urlencoded\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
        {
            headers_added++;
        }

        if(WinHttpAddRequestHeaders(contexts.back().request_handle, L"Cache-Control: no-cache\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
        {
            headers_added++;
        }

        if(WinHttpAddRequestHeaders(contexts.back().request_handle, L"Cookie: theme=theme-dark\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
        {
            headers_added++;
        }

        if (WinHttpAddRequestHeaders(contexts.back().request_handle, L"User-Agent: Steam\r\n", -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
        {
            headers_added++;
        }

        if (headers_added < 5)
        {
            printf("Failed to add headers\n");
            continue;
        }

        if(!WinHttpSendRequest(contexts.back().request_handle, WINHTTP_NO_ADDITIONAL_HEADERS, -1L, 
            &contexts.back().request_payload[0], contexts.back().request_payload.size() + 1, contexts.back().request_payload.size() + 1, (DWORD_PTR)&contexts.back()))
        {
            printf("Failed to send request\n");
            continue;
        }
    }

    for (auto i = 0; i < contexts.size(); i++)
    {
        WaitForSingleObject(contexts[i].read_complete, contexts[i].timeout);
        WinHttpCloseHandle(contexts[i].request_handle);
        results->insert({contexts[i].request_id, contexts[i].response_payload});
    }

    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    printf("Async pre-fetch ended\n");
}