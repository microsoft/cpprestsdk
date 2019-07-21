// winhttp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include <sstream>
#include <vector>
#include <mutex>
#include <thread>
#include <algorithm>
#include <map>
#include <condition_variable>
#include <future>
#include <queue>
#include <chrono>
#include <ctime>
#include <string.h>
#include <time.h>
#include <regex>
#include <cstdlib>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <memory>
#include <codecvt>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>

#define _WINHTTP_INTERNAL_
#ifdef _MSC_VER
#include <windows.h>
#include "winhttp.h"
#else
#include <pthread.h>
#include <unistd.h>
#include "winhttppal.h"
#endif

#include <assert.h>
extern "C"
{
#include <curl/curl.h>
}

class WinHttpSessionImp;
class WinHttpRequestImp;

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

std::map<DWORD, TSTRING> StatusCodeMap = {
{ 100, TEXT("Continue") },
{ 101, TEXT("Switching Protocols") },
{ 200, TEXT("OK") },
{ 201, TEXT("Created") },
{ 202, TEXT("Accepted") },
{ 203, TEXT("Non-Authoritative Information") },
{ 204, TEXT("No Content") },
{ 205, TEXT("Reset Content") },
{ 206, TEXT("Partial Content") },
{ 300, TEXT("Multiple Choices") },
{ 301, TEXT("Moved Permanently") },
{ 302, TEXT("Found") },
{ 303, TEXT("See Other") },
{ 304, TEXT("Not Modified") },
{ 305, TEXT("Use Proxy") },
{ 306, TEXT("(Unused)") },
{ 307, TEXT("Temporary Redirect") },
{ 400, TEXT("Bad Request") },
{ 401, TEXT("Unauthorized") },
{ 402, TEXT("Payment Required") },
{ 403, TEXT("Forbidden") },
{ 404, TEXT("Not Found") },
{ 405, TEXT("Method Not Allowed") },
{ 406, TEXT("Not Acceptable") },
{ 407, TEXT("Proxy Authentication Required") },
{ 408, TEXT("Request Timeout") },
{ 409, TEXT("Conflict") },
{ 410, TEXT("Gone") },
{ 411, TEXT("Length Required") },
{ 412, TEXT("Precondition Failed") },
{ 413, TEXT("Request Entity Too Large") },
{ 414, TEXT("Request-URI Too Long") },
{ 415, TEXT("Unsupported Media Type") },
{ 416, TEXT("Requested Range Not Satisfiable") },
{ 417, TEXT("Expectation Failed") },
{ 500, TEXT("Internal Server Error") },
{ 501, TEXT("Not Implemented") },
{ 502, TEXT("Bad Gateway") },
{ 503, TEXT("Service Unavailable") },
{ 504, TEXT("Gateway Timeout") },
{ 505, TEXT("HTTP Version Not Supported") },
};

enum
{
    WINHTTP_CLASS_SESSION,
    WINHTTP_CLASS_REQUEST,
    WINHTTP_CLASS_IMP,
};

int winhttp_tracing = false;
int winhttp_tracing_verbose = false;

#ifdef _MSC_VER
int gettimeofday(struct timeval * tp, struct timezone * tzp);
#endif

std::mutex trcmtx;

#ifndef _MSC_VER
static void TRACE_INTERNAL(const char *fmt, ...) __attribute__ ((format (gnu_printf, 1, 2)));
#endif

static void TRACE_INTERNAL(const char *fmt, ...)
{
    std::lock_guard<std::mutex> lck(trcmtx);
    va_list args;
    va_start(args, fmt);

    struct timeval tv;
    time_t nowtime;
    struct tm *nowtm;
    char tmbuf[64], buf[64];

    gettimeofday(&tv, NULL);
    nowtime = tv.tv_sec;
    nowtm = localtime(&nowtime);
    strftime(tmbuf, sizeof tmbuf, "%H:%M:%S", nowtm);
    snprintf(buf, sizeof buf, "%s.%06ld ", tmbuf, tv.tv_usec);
    printf("%s", buf);

    char szBuffer[512];
    vsnprintf(szBuffer, sizeof szBuffer -1, fmt, args);
    szBuffer[sizeof(szBuffer)/sizeof(szBuffer[0]) - 1] = '\0';
    printf("%s", szBuffer);
    va_end(args);
}

#define TRACE(fmt, ...) \
            do { if (winhttp_tracing) TRACE_INTERNAL(fmt, __VA_ARGS__); } while (0)

#define TRACE_VERBOSE(fmt, ...) \
            do { if (winhttp_tracing_verbose) TRACE_INTERNAL(fmt, __VA_ARGS__); } while (0)

typedef void (*CompletionCb)(std::shared_ptr<WinHttpRequestImp>, DWORD status);

#define MUTEX_TYPE                              std::mutex
#define MUTEX_SETUP(x)
#define MUTEX_CLEANUP(x)
#define MUTEX_LOCK(x)                           x.lock()
#define MUTEX_UNLOCK(x)                         x.unlock()
#ifdef WIN32
#define THREAD_ID                               GetCurrentThreadId()
#define THREAD_HANDLE                           HANDLE
#define THREADPARAM                             LPVOID
#define CREATETHREAD(func, param, id) \
                                                CreateThread(                         \
                                                        NULL,                         \
                                                        0,                            \
                                                        (LPTHREAD_START_ROUTINE)func, \
                                                        param,                        \
                                                        0,                             \
                                                        id)

#define THREADJOIN(h)                           WaitForSingleObject(h, INFINITE);
#define THREADRETURN                            DWORD
#else
#define THREADPARAM                             void*
#define THREADRETURN                            void*
#define THREAD_ID                               pthread_self()
#define THREAD_HANDLE                           pthread_t
#define THREADJOIN(x)                           pthread_join(x, NULL)

typedef void* (*LPTHREAD_START_ROUTINE)(void *);
static inline THREAD_HANDLE CREATETHREAD(LPTHREAD_START_ROUTINE func, LPVOID param, pthread_t *id)
{
    pthread_t inc_x_thread;
    pthread_create(&inc_x_thread, NULL, func, param);
    return inc_x_thread;
}
#endif

void handle_error(const char *file, int lineno, const char *msg)
{
    fprintf(stderr, "** %s:%d %s\n", file, lineno, msg);
    ERR_print_errors_fp(stderr);
    /* exit(-1); */
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
/* This array will store all of the mutexes available to OpenSSL. */
static MUTEX_TYPE *mutex_buf = NULL;

static void locking_function(int mode, int n, const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
        MUTEX_LOCK(mutex_buf[n]);
    else
        MUTEX_UNLOCK(mutex_buf[n]);
}

static unsigned long id_function(void)
{
    return static_cast<unsigned long>(THREAD_ID);
}
#endif

int thread_setup(void)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    int i;

    mutex_buf = new MUTEX_TYPE[CRYPTO_num_locks()];
    if (!mutex_buf)
        return 0;
    for (i = 0; i < CRYPTO_num_locks(); i++)
        MUTEX_SETUP(mutex_buf[i]);
    CRYPTO_set_id_callback(id_function);
    CRYPTO_set_locking_callback(locking_function);
#endif
    return 1;
}

int thread_cleanup(void)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    int i;

    if (!mutex_buf)
        return 0;
    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    for (i = 0; i < CRYPTO_num_locks(); i++)
        MUTEX_CLEANUP(mutex_buf[i]);
    delete [] mutex_buf;
    mutex_buf = NULL;
#endif
    return 1;
}

class UserCallbackContext
{
    std::shared_ptr<WinHttpRequestImp> m_request;
    DWORD m_dwInternetStatus;
    DWORD m_dwStatusInformationLength;
    DWORD m_dwNotificationFlags;
    WINHTTP_STATUS_CALLBACK m_cb;
    LPVOID m_userdata;
    LPVOID m_StatusInformationVal = NULL;
    BYTE* m_StatusInformation = NULL;
    bool m_allocate = FALSE;
    BOOL m_AsyncResultValid = false;
    CompletionCb m_requestCompletionCb;

    BOOL SetAsyncResult(LPVOID statusInformation, DWORD statusInformationCopySize, bool allocate) {

        if (allocate)
        {
            m_StatusInformation = new BYTE[statusInformationCopySize];
            if (m_StatusInformation)
            {
                memcpy(m_StatusInformation, statusInformation, statusInformationCopySize);
            }
            else
                return FALSE;
        }
        else
        {
            m_StatusInformationVal = statusInformation;
        }
        m_AsyncResultValid = true;
        m_allocate = allocate;

        return TRUE;
    }

public:
    UserCallbackContext(std::shared_ptr<WinHttpRequestImp> &request,
        DWORD dwInternetStatus,
        DWORD dwStatusInformationLength,
        DWORD dwNotificationFlags,
        WINHTTP_STATUS_CALLBACK cb,
        LPVOID userdata,
        LPVOID statusInformation,
        DWORD statusInformationCopySize,
        bool allocate,
        CompletionCb completion);
    ~UserCallbackContext();
    std::shared_ptr<WinHttpRequestImp> GetRequestRef() { return m_request; }
    WinHttpRequestImp *GetRequest() { return m_request.get(); }
    DWORD GetInternetStatus() const { return m_dwInternetStatus; }
    DWORD GetStatusInformationLength() const { return m_dwStatusInformationLength; }
    DWORD GetNotificationFlags() const { return m_dwNotificationFlags; }
    WINHTTP_STATUS_CALLBACK &GetCb() { return m_cb; }
    CompletionCb &GetRequestCompletionCb() { return m_requestCompletionCb; }
    LPVOID GetUserdata() { return m_userdata; }
    LPVOID GetStatusInformation() {
        if (m_allocate)
            return m_StatusInformation;
        else
            return m_StatusInformationVal;
    }
    BOOL GetStatusInformationValid() const { return m_AsyncResultValid; }
};

static void ConvertCstrAssign(const TCHAR *lpstr, size_t cLen, std::string &target)
{
    size_t bLen = cLen;

#ifdef UNICODE
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    target = conv.to_bytes(std::wstring(lpstr, cLen));
#else
    target.assign(lpstr, cLen);
#endif
    target[bLen] = '\0';
}

static std::vector<std::string> Split(std::string &str, char delimiter) {
    std::vector<std::string> internal;
    std::stringstream ss(str); // Turn the string into a stream.
    std::string tok;

    while (getline(ss, tok, delimiter)) {
        internal.push_back(tok);
    }

    return internal;
}

static BOOL SizeCheck(LPVOID lpBuffer, LPDWORD lpdwBufferLength, DWORD Required)
{
    if (!lpBuffer)
    {
        if (lpdwBufferLength)
            *lpdwBufferLength = Required;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    if (lpdwBufferLength && (*lpdwBufferLength < Required)) {
        *lpdwBufferLength = Required;

        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    if ((Required == 0) || (lpdwBufferLength && (*lpdwBufferLength == 0)))
        return FALSE;

    return TRUE;
}

class WinHttpBase
{
public:
    virtual ~WinHttpBase() {}
};

class WinHttpConnectImp :public WinHttpBase
{
    WinHttpSessionImp *m_Handle = NULL;
    void *m_UserBuffer = NULL;

public:
    void SetHandle(WinHttpSessionImp *session) { m_Handle = session; }
    WinHttpSessionImp *GetHandle() { return m_Handle; }

    BOOL SetUserData(void **data)
    {
        m_UserBuffer = *data;
        return TRUE;
    }
    void *GetUserData() { return m_UserBuffer; }
};

struct BufferRequest
{
    LPCVOID m_Buffer = NULL;
    size_t  m_Length = 0;
};

class WinHttpRequestImp :public WinHttpBase, public std::enable_shared_from_this<WinHttpRequestImp>
{
    CURL *m_curl = NULL;
    std::vector<BYTE> m_ResponseString;
    std::string m_HeaderString = "";
    size_t m_TotalHeaderStringLength = 0;
    std::string m_Header = "";
    std::string m_FullPath = "";
    std::string m_OptionalData = "";
    size_t m_TotalSize = 0;
    size_t m_TotalReceiveSize = 0;
    std::vector<BYTE> m_ReadData;

    std::mutex m_ReadDataEventMtx;
    DWORD m_ReadDataEventCounter = 0;
    THREAD_HANDLE m_UploadCallbackThread;
    bool m_UploadThreadExitStatus = false;

    DWORD m_SecureProtocol = 0;
    DWORD m_MaxConnections = 0;

    std::string m_Type = "";
    LPVOID m_UserBuffer = NULL;
    bool m_HeaderReceiveComplete = false;

    struct curl_slist *m_HeaderList = NULL;
    WinHttpConnectImp *m_Session = NULL;
    std::mutex m_HeaderStringMutex;
    std::mutex m_BodyStringMutex;

    std::condition_variable m_CompletionEvent;
    std::mutex m_CompletionEventMtx;
    bool m_CompletionEventState = false;

    std::condition_variable m_QueryDataEvent;
    std::mutex m_QueryDataEventMtx;
    bool m_QueryDataEventState = false;
    std::atomic<bool> m_QueryDataPending;

    std::condition_variable m_ReceiveCompletionEvent;
    std::mutex m_ReceiveCompletionEventMtx;
    std::atomic<DWORD> m_ReceiveCompletionEventCounter;

    std::mutex m_ReceiveResponseMutex;
    std::atomic<bool> m_ReceiveResponsePending;
    std::atomic<int> m_ReceiveResponseEventCounter;
    int m_ReceiveResponseSendCounter;

    std::atomic<bool> m_RedirectPending;

    std::condition_variable m_DataAvailable;
    std::mutex m_DataAvailableMtx;
    std::atomic<DWORD> m_DataAvailableEventCounter;
    bool m_closing = false;
    bool m_closed = false;
    bool m_Completion = false;
    bool m_Async = false;
    CURLcode m_CompletionCode = CURLE_OK;
    long m_VerifyPeer = 1;
    long m_VerifyHost = 2;

    WINHTTP_STATUS_CALLBACK m_InternetCallback = NULL;
    DWORD m_NotificationFlags = 0;
    std::vector<BufferRequest> m_OutstandingWrites;

public:
    void QueueWriteRequest(LPCVOID buffer, size_t length)
    {
        BufferRequest shr;
        shr.m_Buffer = buffer;
        shr.m_Length = length;
        m_OutstandingWrites.push_back(shr);
    }

    BufferRequest GetWriteRequest()
    {
        if (m_OutstandingWrites.empty())
            return BufferRequest();
        BufferRequest shr = m_OutstandingWrites.front();
        m_OutstandingWrites.pop_back();
        return shr;
    }

    long &VerifyPeer() { return m_VerifyPeer; }
    long &VerifyHost() { return m_VerifyHost; }

    static size_t WriteHeaderFunction(void *ptr, size_t size, size_t nmemb, void* rqst);
    static size_t WriteBodyFunction(void *ptr, size_t size, size_t nmemb, void* rqst);
    void SetCallback(WINHTTP_STATUS_CALLBACK lpfnInternetCallback, DWORD dwNotificationFlags) {
        m_InternetCallback = lpfnInternetCallback;
        m_NotificationFlags = dwNotificationFlags;
    }
    WINHTTP_STATUS_CALLBACK GetCallback(DWORD *dwNotificationFlags)
    {
        *dwNotificationFlags = m_NotificationFlags;
        return m_InternetCallback;
    }

    void SetAsync() { m_Async = TRUE; }
    BOOL GetAsync() { return m_Async; }

    WinHttpRequestImp();

    bool &GetQueryDataEventState() { return m_QueryDataEventState; }
    std::mutex &GetQueryDataEventMtx() { return m_QueryDataEventMtx; }
    std::condition_variable &GetQueryDataEvent() { return m_QueryDataEvent; }

    bool HandleQueryDataNotifications(std::shared_ptr<WinHttpRequestImp>, size_t available);
    void WaitAsyncQueryDataCompletion(std::shared_ptr<WinHttpRequestImp>);

    void HandleReceiveNotifications(std::shared_ptr<WinHttpRequestImp>);
    void WaitAsyncReceiveCompletion(std::shared_ptr<WinHttpRequestImp> srequest);
    size_t WaitDataAvailable();

    CURLcode &GetCompletionCode() { return m_CompletionCode; }
    bool &GetCompletionStatus() { return m_Completion; }
    bool &GetClosing() { return m_closing; }
    bool &GetClosed() { return m_closed; }
    void CleanUp();
    ~WinHttpRequestImp();

    bool &GetUploadThreadExitStatus() { return m_UploadThreadExitStatus; }
    std::atomic <bool> &GetQueryDataPending() { return m_QueryDataPending; }

    THREAD_HANDLE &GetUploadThread() {
        return m_UploadCallbackThread;
    }

    static THREADRETURN UploadThreadFunction(THREADPARAM lpThreadParameter)
    {
        WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(lpThreadParameter);
        CURL *curl = request->GetCurl();
        CURLcode res;

        TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)request);
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK)
        {
            TRACE("curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
            return FALSE;
        }
        TRACE("%-35s:%-8d:%-16p res:%d\n", __func__, __LINE__, (void*)request, res);
        request->GetUploadThreadExitStatus() = true;

    #if OPENSSL_VERSION_NUMBER >= 0x10000000L && OPENSSL_VERSION_NUMBER < 0x10100000L
        ERR_remove_thread_state(NULL);
    #elif OPENSSL_VERSION_NUMBER < 0x10000000L
        ERR_remove_state(0);
    #endif
        return 0;
    }

    // used to wake up CURL ReadCallback triggered on a upload request
    std::mutex &GetReadDataEventMtx() { return m_ReadDataEventMtx; }
    DWORD &GetReadDataEventCounter() { return m_ReadDataEventCounter; }

    // sent by WriteHeaderFunction and WriteBodyFunction during incoming data
    // to send user notifications for the following events:
    //
    // WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE
    // WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED
    // WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE
    //
    // WinHttpReceiveResponse is blocked until one of the above events happen
    //
    std::condition_variable &GetReceiveCompletionEvent() { return m_ReceiveCompletionEvent; }
    std::mutex &GetReceiveCompletionEventMtx() { return m_ReceiveCompletionEventMtx; }
    std::atomic<DWORD> &GetReceiveCompletionEventCounter() { return m_ReceiveCompletionEventCounter; }

    // counters used to see which one of these events are observed: ResponseCallbackEventCounter
    // counters used to see which one of these events are broadcasted: ResponseCallbackSendCounter
    // WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE
    // WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED
    // WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE
    //
    // These counters need to be 3 and identical under normal circumstances
    std::atomic<int> &ResponseCallbackEventCounter() { return m_ReceiveResponseEventCounter; }
    int &ResponseCallbackSendCounter() { return m_ReceiveResponseSendCounter; }
    std::atomic <bool> &GetReceiveResponsePending() { return m_ReceiveResponsePending; }
    std::mutex  &GetReceiveResponseMutex() { return m_ReceiveResponseMutex; }

    std::atomic<bool> &GetRedirectPending() { return m_RedirectPending; }

    std::condition_variable &GetDataAvailableEvent() { return m_DataAvailable; }
    std::mutex &GetDataAvailableMtx() { return m_DataAvailableMtx; }
    std::atomic<DWORD> &GetDataAvailableEventCounter() { return m_DataAvailableEventCounter; }

    std::mutex &GetHeaderStringMutex() { return m_HeaderStringMutex; }
    std::mutex &GetBodyStringMutex() { return m_HeaderStringMutex; }

    BOOL AsyncQueue(std::shared_ptr<WinHttpRequestImp> &,
                    DWORD dwInternetStatus, size_t statusInformationLength,
                    LPVOID async, DWORD asyncLength, bool allocate);

    void SetHeaderReceiveComplete() { m_HeaderReceiveComplete = TRUE; }

    BOOL SetSecureProtocol(DWORD *data)
    {
        m_SecureProtocol = *data;
        return TRUE;
    }
    DWORD GetSecureProtocol() { return m_SecureProtocol; }

    BOOL SetMaxConnections(DWORD *data)
    {
        m_MaxConnections = *data;
        return TRUE;
    }
    DWORD GetMaxConnections() { return m_MaxConnections; }

    BOOL SetUserData(void **data)
    {
        m_UserBuffer = *data;
        return TRUE;
    }
    LPVOID GetUserData() { return m_UserBuffer; }

    void SetFullPath(std::string &server, std::string &relative)
    {
        m_FullPath.append(server);
        m_FullPath.append(relative);
    }
    std::string &GetFullPath() { return m_FullPath; }
    std::string &GetType() { return m_Type; }

    void SetSession(WinHttpConnectImp *connect) { m_Session = connect; }
    WinHttpConnectImp *GetSession() { return m_Session; }

    struct curl_slist *GetHeaderList() { return m_HeaderList; }

    void SetHeaderList(struct curl_slist *list) { m_HeaderList = list; }
    std::string &GetOutgoingHeaderList() { return m_Header; }
    void AddHeader(std::string &headers) {
        std::stringstream check1(headers);
        std::string str;

        while(getline(check1, str, '\n'))
        {
            str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
            SetHeaderList(curl_slist_append(GetHeaderList(), str.c_str()));
        }
    }

    std::vector<BYTE> &GetResponseString() { return m_ResponseString; }
    size_t &GetTotalHeaderStringLength() { return m_TotalHeaderStringLength; }
    std::string &GetHeaderString() { return m_HeaderString; }
    CURL *GetCurl() { return m_curl; }

    BOOL SetProxy(std::vector<std::string> &proxies)
    {
        std::vector<std::string>::iterator it;

        for (it = proxies.begin(); it != proxies.end(); it++)
        {
            std::string &urlstr = (*it);
            CURLcode res;

            res = curl_easy_setopt(GetCurl(), CURLOPT_PROXY, urlstr.c_str());
            if (res != CURLE_OK)
                return FALSE;
        }

        return TRUE;
    }

    BOOL SetServer(std::string &ServerName, int nServerPort)
    {
        CURLcode res;

        res = curl_easy_setopt(GetCurl(), CURLOPT_URL, ServerName.c_str());
        if (res != CURLE_OK)
            return FALSE;

        res = curl_easy_setopt(GetCurl(), CURLOPT_PORT, nServerPort);
        if (res != CURLE_OK)
            return FALSE;

        return TRUE;
    }

    size_t &GetTotalLength() { return m_TotalSize; }
    size_t &GetReadLength() { return m_TotalReceiveSize; }

    std::string &GetOptionalData() { return m_OptionalData; }
    void SetOptionalData(void *lpOptional, size_t dwOptionalLength)
    {
        m_OptionalData.assign(&(static_cast<char*>(lpOptional))[0], dwOptionalLength);
    }

    std::vector<BYTE> &GetReadData() { return m_ReadData; }

    void AppendReadData(const void *data, size_t len)
    {
        std::lock_guard<std::mutex> lck(GetReadDataEventMtx());
        m_ReadData.insert(m_ReadData.end(), static_cast<const BYTE*>(data), static_cast<const BYTE*>(data) + len);
    }

    static int SocketCallback(CURL *handle, curl_infotype type,
        char *data, size_t size,
        void *userp);

    static size_t ReadCallback(void *ptr, size_t size, size_t nmemb, void *userp);
};

typedef std::vector<UserCallbackContext*> UserCallbackQueue;

class UserCallbackContainer
{
    UserCallbackQueue m_Queue;

    // used to protect GetCallbackMap()
    std::mutex m_MapMutex;

    THREAD_HANDLE m_hThread;

    // used by UserCallbackThreadFunction as a wake-up signal
    std::mutex m_hEventMtx;
    std::condition_variable m_hEvent;

    // cleared by UserCallbackThreadFunction, set by multiple event providers
    std::atomic<DWORD> m_EventCounter;

    bool m_closing = false;
    UserCallbackQueue &GetCallbackQueue() { return m_Queue; }


public:

    bool GetClosing() const { return m_closing; }

    UserCallbackContext* GetNext()
    {
        UserCallbackContext *ctx = NULL;
        // show content:
        if (!GetCallbackQueue().empty())
        {
            ctx = GetCallbackQueue().front();
            GetCallbackQueue().erase(GetCallbackQueue().begin());
        }

        return ctx;
    }

    static THREADRETURN UserCallbackThreadFunction(LPVOID lpThreadParameter);

    void Queue(UserCallbackContext *ctx)
    {
        {
            std::lock_guard<std::mutex> lck(m_MapMutex);

            TRACE_VERBOSE("%-35s:%-8d:%-16p ctx = %p cb = %p userdata = %p dwInternetStatus = %p\n",
                __func__, __LINE__, (void*)ctx->GetRequest(), reinterpret_cast<void*>(ctx), reinterpret_cast<void*>(ctx->GetCb()),
                ctx->GetUserdata(), ctx->GetStatusInformation());
            GetCallbackQueue().push_back(ctx);
        }
        {
            std::lock_guard<std::mutex> lck(m_hEventMtx);
            m_EventCounter++;
            m_hEvent.notify_all();
        }
    }

    void DrainQueue()
    {
        while (1)
        {
            m_MapMutex.lock();
            UserCallbackContext *ctx = GetNext();
            m_MapMutex.unlock();

            if (!ctx)
                break;

            delete ctx;
        }
    }

    UserCallbackContainer(): m_EventCounter(0)
    {
        DWORD thread_id;
        m_hThread = CREATETHREAD(
            UserCallbackThreadFunction,       // thread function name
            this,          // argument to thread function
            &thread_id
        );
    }
    ~UserCallbackContainer()
    {
        m_closing = true;
        {
            std::lock_guard<std::mutex> lck(m_hEventMtx);
            m_EventCounter++;
            m_hEvent.notify_all();
        }
        THREADJOIN(m_hThread);
        DrainQueue();
    }
    static UserCallbackContainer &GetInstance()
    {
        static UserCallbackContainer the_instance;
        return the_instance;
    }
private:
	UserCallbackContainer(const UserCallbackContainer&);
	UserCallbackContainer& operator=(const UserCallbackContainer&);
};

class EnvInit
{
public:
    EnvInit()
    {
        if (const char* env_p = std::getenv("WINHTTP_PAL_DEBUG"))
            winhttp_tracing = atoi(env_p);

        if (const char* env_p = std::getenv("WINHTTP_PAL_DEBUG_VERBOSE"))
            winhttp_tracing_verbose = atoi(env_p);
    }
};

static EnvInit envinit;

class ComContainer
{
    THREAD_HANDLE m_hAsyncThread;
    std::mutex m_hAsyncEventMtx;

    // used to wake up the Async Thread
    // Set by external components, cleared by the Async thread
    std::atomic<DWORD> m_hAsyncEventCounter;
    std::condition_variable m_hAsyncEvent;
    std::mutex m_MultiMutex;
    CURLM *m_curlm = NULL;
    std::vector< CURL *> m_ActiveCurl;
    std::vector<std::shared_ptr<WinHttpRequestImp>> m_ActiveRequests;
    BOOL m_closing = FALSE;

    // used to protect CURLM data structures
    BOOL GetThreadClosing() const { return m_closing; }

public:
    CURL *AllocCURL()
    {
        CURL *ptr;

        m_MultiMutex.lock();
        ptr = curl_easy_init();
        m_MultiMutex.unlock();

        return ptr;
    }

    void FreeCURL(CURL *ptr)
    {
        m_MultiMutex.lock();
        curl_easy_cleanup(ptr);
        m_MultiMutex.unlock();
    }

    static ComContainer &GetInstance()
    {
        static ComContainer the_instance;
        return the_instance;
    }

    void ResumeTransfer(CURL *handle, int bitmask)
    {
        int still_running;

        m_MultiMutex.lock();
        curl_easy_pause(handle, bitmask);
        curl_multi_perform(m_curlm, &still_running);
        m_MultiMutex.unlock();
    }

    BOOL AddHandle(std::shared_ptr<WinHttpRequestImp> srequest, CURL *handle)
    {
        CURLMcode mres = CURLM_OK;

        m_MultiMutex.lock();
        if (std::find(m_ActiveCurl.begin(), m_ActiveCurl.end(), handle) != m_ActiveCurl.end()) {
            mres = curl_multi_remove_handle(m_curlm, handle);
            m_ActiveCurl.erase(std::remove(m_ActiveCurl.begin(), m_ActiveCurl.end(), handle), m_ActiveCurl.end());
        }
        if (std::find(m_ActiveRequests.begin(), m_ActiveRequests.end(), srequest) != m_ActiveRequests.end()) {
            m_ActiveRequests.erase(std::remove(m_ActiveRequests.begin(), m_ActiveRequests.end(), srequest), m_ActiveRequests.end());
        }
        m_MultiMutex.unlock();
        if (mres != CURLM_OK)
        {
            TRACE("curl_multi_remove_handle() failed: %s\n", curl_multi_strerror(mres));
            return FALSE;
        }

        m_MultiMutex.lock();
        mres = curl_multi_add_handle(m_curlm, handle);
        m_ActiveCurl.push_back(handle);
        m_ActiveRequests.push_back(srequest);
        m_MultiMutex.unlock();
        if (mres != CURLM_OK)
        {
            TRACE("curl_multi_add_handle() failed: %s\n", curl_multi_strerror(mres));
            return FALSE;
        }

        return TRUE;
    }
    BOOL RemoveHandle(std::shared_ptr<WinHttpRequestImp> srequest, CURL *handle, bool clearPrivate)
    {
        CURLMcode mres;

        m_MultiMutex.lock();
        mres = curl_multi_remove_handle(m_curlm, handle);

        if (clearPrivate)
            curl_easy_getinfo(handle, CURLINFO_PRIVATE, NULL);

        m_ActiveCurl.erase(std::remove(m_ActiveCurl.begin(), m_ActiveCurl.end(), handle), m_ActiveCurl.end());
        m_ActiveRequests.erase(std::remove(m_ActiveRequests.begin(), m_ActiveRequests.end(), srequest), m_ActiveRequests.end());
        m_MultiMutex.unlock();
        if (mres != CURLM_OK)
        {
            TRACE("curl_multi_add_handle() failed: %s\n", curl_multi_strerror(mres));
            return FALSE;
        }

        return TRUE;
    }

    long GetTimeout()
    {
        long curl_timeo;

        m_MultiMutex.lock();
        curl_multi_timeout(m_curlm, &curl_timeo);
        m_MultiMutex.unlock();

        return curl_timeo;
    }

    void KickStart()
    {
        std::lock_guard<std::mutex> lck(m_hAsyncEventMtx);
        m_hAsyncEventCounter++;
        m_hAsyncEvent.notify_all();
    }

    ComContainer(): m_hAsyncEventCounter(0)
    {
        curl_global_init(CURL_GLOBAL_ALL);
        thread_setup();

        m_curlm = curl_multi_init();

        DWORD thread_id;
        m_hAsyncThread = CREATETHREAD(
            AsyncThreadFunction,       // thread function name
            this, &thread_id);          // argument to thread function
    }

    ~ComContainer()
    {
        TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)this);
        m_closing = true;
        {
            std::lock_guard<std::mutex> lck(m_hAsyncEventMtx);
            m_hAsyncEventCounter++;
            m_hAsyncEvent.notify_all();
        }
        THREADJOIN(m_hAsyncThread);
        TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)this);

        curl_multi_cleanup(m_curlm);
        curl_global_cleanup();
        thread_cleanup();
    }


    static THREADRETURN AsyncThreadFunction(THREADPARAM lpThreadParameter);

    int QueryData(int *still_running)
    {
        int rc = 0;
        struct timeval timeout;

        fd_set fdread;
        fd_set fdwrite;
        fd_set fdexcep;
        int maxfd = -1;

        long curl_timeo = -1;

        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);

        /* set a suitable timeout to play around with */
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        m_MultiMutex.lock();
        curl_multi_timeout(m_curlm, &curl_timeo);
        m_MultiMutex.unlock();
        if (curl_timeo < 0)
            /* no set timeout, use a default */
            curl_timeo = 10000;

        if (curl_timeo > 0) {
            timeout.tv_sec = curl_timeo / 1000;
            if (timeout.tv_sec > 1)
                timeout.tv_sec = 1;
            else
                timeout.tv_usec = (curl_timeo % 1000) * 1000;
        }

        /* get file descriptors from the transfers */
        m_MultiMutex.lock();
        rc = curl_multi_fdset(m_curlm, &fdread, &fdwrite, &fdexcep, &maxfd);
        m_MultiMutex.unlock();

        if ((maxfd == -1) || (rc != CURLM_OK)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        else
            rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);

        switch (rc) {
        case -1:
            /* select error */
            *still_running = 0;
            TRACE("%s\n", "select() returns error, this is badness\n");
            break;
        case 0:
        default:
            /* timeout or readable/writable sockets */
            m_MultiMutex.lock();
            curl_multi_perform(m_curlm, still_running);
            m_MultiMutex.unlock();
            break;
        }

        return rc;
    }
private:
	ComContainer(const ComContainer&);
	ComContainer& operator=(const ComContainer&);
};

template<class T>
class WinHttpHandleContainer
{
    std::mutex m_ActiveRequestMtx;
    std::vector<std::shared_ptr<T>> m_ActiveRequests;

public:
    static WinHttpHandleContainer &Instance()
    {
        static WinHttpHandleContainer the_instance;
        return the_instance;
    }

    typedef std::shared_ptr<T> t_shared;
    void UnRegister(T *val)
    {
        typename std::vector<t_shared>::iterator findit;
        bool found = false;
        std::lock_guard<std::mutex> lck(m_ActiveRequestMtx);

        TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)val);

        for (auto it = m_ActiveRequests.begin(); it != m_ActiveRequests.end(); ++it)
        {
            auto v = (*it);
            if (v.get() == val)
            {
                findit = it;
                found = true;
                break;
            }
        }
        if (found)
            m_ActiveRequests.erase(findit);
    }

    bool IsRegistered(T *val)
    {
        bool found = false;
        std::lock_guard<std::mutex> lck(m_ActiveRequestMtx);

        for (auto it = m_ActiveRequests.begin(); it != m_ActiveRequests.end(); ++it)
        {
            auto v = (*it);
            if (v.get() == val)
            {
                found = true;
                break;
            }
        }
        TRACE("%-35s:%-8d:%-16p found:%d\n", __func__, __LINE__, (void*)val, found);

        return found;
    }

    void Register(std::shared_ptr<T> rqst)
    {
        TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)(rqst.get()));
        std::lock_guard<std::mutex> lck(m_ActiveRequestMtx);
        m_ActiveRequests.push_back(rqst);
    }
};

class WinHttpSessionImp :public WinHttpBase
{
    std::string m_ServerName = "";
    std::string m_Referrer = "";
    std::vector<std::string> m_Proxies;
    std::string m_Proxy = "";
    WINHTTP_STATUS_CALLBACK m_InternetCallback = NULL;
    DWORD m_NotificationFlags = 0;

    int m_ServerPort = 0;
    long m_Timeout = 15000;
    BOOL m_Async = false;

    bool m_closing = false;
    DWORD m_MaxConnections = 0;
    DWORD m_SecureProtocol = 0;
    void *m_UserBuffer = NULL;

public:

    BOOL SetUserData(void **data)
    {
        m_UserBuffer = *data;
        return TRUE;
    }
    void *GetUserData() { return m_UserBuffer; }

    BOOL SetSecureProtocol(DWORD *data)
    {
        m_SecureProtocol = *data;
        return TRUE;
    }
    DWORD GetSecureProtocol() const { return m_SecureProtocol; }

    BOOL SetMaxConnections(DWORD *data)
    {
        m_MaxConnections = *data;
        return TRUE;
    }
    DWORD GetMaxConnections() const { return m_MaxConnections; }

    void SetAsync() { m_Async = TRUE; }
    BOOL GetAsync() const { return m_Async; }

    BOOL GetThreadClosing() const { return m_closing; }

    std::vector<std::string> &GetProxies() { return m_Proxies; }
    void SetProxies(std::vector<std::string> &proxies) { m_Proxies = proxies; }

    std::string &GetProxy() { return m_Proxy; }

    int GetServerPort() const { return m_ServerPort; }
    void SetServerPort(int port) { m_ServerPort = port; }

    long GetTimeout() const {
        if (m_Timeout)
            return m_Timeout;

        long curl_timeo;

        curl_timeo = ComContainer::GetInstance().GetTimeout();

        if (curl_timeo < 0)
            curl_timeo = 10000;

        return curl_timeo;
    }
    void SetTimeout(long timeout) { m_Timeout = timeout; }

    std::string &GetServerName() { return m_ServerName; }

    std::string &GetReferrer() { return m_Referrer; }

    void SetCallback(WINHTTP_STATUS_CALLBACK lpfnInternetCallback, DWORD dwNotificationFlags) {
        m_InternetCallback = lpfnInternetCallback;
        m_NotificationFlags = dwNotificationFlags;
    }
    WINHTTP_STATUS_CALLBACK GetCallback(DWORD *dwNotificationFlags)
    {
        *dwNotificationFlags = m_NotificationFlags;
        return m_InternetCallback;
    }

    WinHttpSessionImp()
    {
    }

    ~WinHttpSessionImp()
    {
        TRACE("%-35s:%-8d:%-16p sesion\n", __func__, __LINE__, (void*)this);
        SetCallback(NULL, 0);
        TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)this);
    }
};

THREADRETURN ComContainer::AsyncThreadFunction(LPVOID lpThreadParameter)
{
    ComContainer *comContainer = static_cast<ComContainer *>(lpThreadParameter);

    while (1)
    {
        {
            std::unique_lock<std::mutex> getAsyncEventHndlMtx(comContainer->m_hAsyncEventMtx);
            while (!comContainer->m_hAsyncEventCounter)
                comContainer->m_hAsyncEvent.wait(getAsyncEventHndlMtx);
            getAsyncEventHndlMtx.unlock();

            comContainer->m_hAsyncEventCounter--;
        }

        int still_running = 1;

        while (still_running && !comContainer->GetThreadClosing())
        {
            WinHttpRequestImp *request = NULL;

            comContainer->QueryData(&still_running);

            //TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)request);
            struct CURLMsg *m;

            /* call curl_multi_perform or curl_multi_socket_action first, then loop
               through and check if there are any transfers that have completed */

            do {
                int msgq = 0;
                request = NULL;
                std::shared_ptr<WinHttpRequestImp> srequest;

                comContainer->m_MultiMutex.lock();
                m = curl_multi_info_read(comContainer->m_curlm, &msgq);
                if (m)
                    curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &request);
                if (request)
                {
                    srequest = request->shared_from_this();
                }
                comContainer->m_MultiMutex.unlock();

                if (m && (m->msg == CURLMSG_DONE) && request && srequest) {
                    WINHTTP_ASYNC_RESULT result = { 0, 0 };
                    DWORD dwInternetStatus;

                    TRACE("%-35s:%-8d:%-16p type:%s result:%d\n", __func__, __LINE__, (void*)request, request->GetType().c_str(), m->data.result);
                    request->GetCompletionStatus() = true;
                    request->GetCompletionCode() = m->data.result;

                    // unblock ReceiveResponse if waiting
                    if (m->data.result != CURLE_OK)
                    {
                        std::lock_guard<std::mutex> lck(request->GetReceiveCompletionEventMtx());
                        request->GetReceiveCompletionEventCounter()++;
                        request->GetReceiveCompletionEvent().notify_all();
                        TRACE("%-35s:%-8d:%-16p GetReceiveCompletionEvent().notify_all\n", __func__, __LINE__, (void*)request);
                    }

                    if (m->data.result == CURLE_OK)
                    {
                        if (request->HandleQueryDataNotifications(srequest, 0))
                        {
                            TRACE("%-35s:%-8d:%-16p GetQueryDataEvent().notify_all\n", __func__, __LINE__, (void*)request);
                        }
                        {
                            std::lock_guard<std::mutex> lck(request->GetQueryDataEventMtx());
                            request->GetQueryDataEventState() = true;
                            request->GetQueryDataEvent().notify_all();
                        }
                        request->HandleQueryDataNotifications(srequest, 0);
                    }

                    if (m->data.result == CURLE_OK)
                    {
                        TRACE("%s:%d m->data.result:%d\n", __func__, __LINE__, m->data.result);
                        std::lock_guard<std::mutex> lck(request->GetDataAvailableMtx());
                        request->GetDataAvailableEventCounter()++;
                        request->GetDataAvailableEvent().notify_all();
                    }

                    if (m->data.result == CURLE_OK)
                    {
                    }
                    else if (m->data.result == CURLE_OPERATION_TIMEDOUT)
                    {
                        result.dwError = ERROR_WINHTTP_TIMEOUT;
                        dwInternetStatus = WINHTTP_CALLBACK_STATUS_REQUEST_ERROR;
                        request->AsyncQueue(srequest, dwInternetStatus, 0, &result, sizeof(result), true);
                        TRACE("%-35s:%-8d:%-16p request done type = %s CURLE_OPERATION_TIMEDOUT\n",
                              __func__, __LINE__, (void*)request, request->GetType().c_str());
                    }
                    else
                    {
                        result.dwError = ERROR_WINHTTP_OPERATION_CANCELLED;
                        dwInternetStatus = WINHTTP_CALLBACK_STATUS_REQUEST_ERROR;
                        TRACE("%-35s:%-8d:%-16p unknown async request done m->data.result = %d\n",
                              __func__, __LINE__, (void*)request, m->data.result);
#ifdef _DEBUG
                        assert(0);
#endif
                        request->AsyncQueue(srequest, dwInternetStatus, 0, &result, sizeof(result), true);
                    }
                } else if (m && (m->msg != CURLMSG_DONE)) {
                    TRACE("%-35s:%-8d:%-16p unknown async request done\n", __func__, __LINE__, (void*)request);
                    DWORD dwInternetStatus;
                    WINHTTP_ASYNC_RESULT result = { 0, 0 };
                    result.dwError = ERROR_WINHTTP_OPERATION_CANCELLED;
                    dwInternetStatus = WINHTTP_CALLBACK_STATUS_REQUEST_ERROR;
#ifdef _DEBUG
                    assert(0);
#endif
                    if (request)
                        request->AsyncQueue(srequest, dwInternetStatus, 0, &result, sizeof(result), true);
                }

                if (request)
                {
                    comContainer->RemoveHandle(srequest, request->GetCurl(), true);
                    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)request);
                }
            } while (m);
        }
        if (comContainer->GetThreadClosing())
        {
            TRACE("%s:%d exiting\n", __func__, __LINE__);
            break;
        }
    }

#if OPENSSL_VERSION_NUMBER >= 0x10000000L && OPENSSL_VERSION_NUMBER < 0x10100000L
    ERR_remove_thread_state(NULL);
#elif OPENSSL_VERSION_NUMBER < 0x10000000L
    ERR_remove_state(0);
#endif
    return 0;
}

THREADRETURN UserCallbackContainer::UserCallbackThreadFunction(LPVOID lpThreadParameter)
{
    UserCallbackContainer *cbContainer = static_cast<UserCallbackContainer *>(lpThreadParameter);

    while (1)
    {
        {
            std::unique_lock<std::mutex> GetEventHndlMtx(cbContainer->m_hEventMtx);
            while (!cbContainer->m_EventCounter)
                cbContainer->m_hEvent.wait(GetEventHndlMtx);
            GetEventHndlMtx.unlock();

            cbContainer->m_EventCounter--;
        }

        if (cbContainer->GetClosing())
        {
            TRACE("%s", "exiting\n");
            break;
        }

        while (1)
        {
            cbContainer->m_MapMutex.lock();
            UserCallbackContext *ctx = NULL;
            ctx = cbContainer->GetNext();
            cbContainer->m_MapMutex.unlock();
            if (!ctx)
                break;

            void *statusInformation = NULL;

            if (ctx->GetStatusInformationValid())
                statusInformation = reinterpret_cast<void*>(ctx->GetStatusInformation());

            if (!ctx->GetRequestRef()->GetClosed() && ctx->GetCb()) {
                TRACE_VERBOSE("%-35s:%-8d:%-16p ctx = %p cb = %p ctx->GetUserdata() = %p dwInternetStatus:0x%lx statusInformation=%p refcount:%lu\n",
                    __func__, __LINE__, (void*)ctx->GetRequest(), reinterpret_cast<void*>(ctx), reinterpret_cast<void*>(ctx->GetCb()),
                    ctx->GetUserdata(), ctx->GetInternetStatus(), statusInformation, ctx->GetRequestRef().use_count());
                ctx->GetCb()(ctx->GetRequest(),
                    (DWORD_PTR)(ctx->GetUserdata()),
                    ctx->GetInternetStatus(),
                    statusInformation,
                    ctx->GetStatusInformationLength());

                TRACE_VERBOSE("%-35s:%-8d:%-16p ctx = %p cb = %p ctx->GetUserdata() = %p dwInternetStatus:0x%lx statusInformation=%p refcount:%lu\n",
                    __func__, __LINE__, (void*)ctx->GetRequest(), reinterpret_cast<void*>(ctx), reinterpret_cast<void*>(ctx->GetCb()),
                    ctx->GetUserdata(), ctx->GetInternetStatus(), statusInformation, ctx->GetRequestRef().use_count());
            }
            ctx->GetRequestCompletionCb()(ctx->GetRequestRef(), ctx->GetInternetStatus());
            delete ctx;
        }
    }

#if OPENSSL_VERSION_NUMBER >= 0x10000000L && OPENSSL_VERSION_NUMBER < 0x10100000L
    ERR_remove_thread_state(NULL);
#elif OPENSSL_VERSION_NUMBER < 0x10000000L
    ERR_remove_state(0);
#endif

    return 0;
}

UserCallbackContext::UserCallbackContext(std::shared_ptr<WinHttpRequestImp> &request,
    DWORD dwInternetStatus,
    DWORD dwStatusInformationLength,
    DWORD dwNotificationFlags,
    WINHTTP_STATUS_CALLBACK cb,
    LPVOID userdata,
    LPVOID statusInformation,
    DWORD statusInformationCopySize, bool allocate,
    CompletionCb completion)
    : m_request(request), m_dwInternetStatus(dwInternetStatus),
    m_dwStatusInformationLength(dwStatusInformationLength),
    m_dwNotificationFlags(dwNotificationFlags), m_cb(cb), m_userdata(userdata),
    m_requestCompletionCb(completion)
{
    if (statusInformation)
        SetAsyncResult(statusInformation, statusInformationCopySize, allocate);
}

WinHttpSessionImp *GetImp(WinHttpBase *base)
{
    WinHttpConnectImp *connect;
    WinHttpSessionImp *session;
    WinHttpRequestImp *request;

    if ((connect = dynamic_cast<WinHttpConnectImp *>(base)))
    {
        session = connect->GetHandle();
    }
    else if ((request = dynamic_cast<WinHttpRequestImp *>(base)))
    {
        WinHttpConnectImp *connect = request->GetSession();
        session = connect->GetHandle();
    }
    else
    {
        session = dynamic_cast<WinHttpSessionImp *>(base);
    }

    return session;
}


UserCallbackContext::~UserCallbackContext()
{
    if (m_StatusInformation)
        delete [] m_StatusInformation;
}

void WinHttpRequestImp::HandleReceiveNotifications(std::shared_ptr<WinHttpRequestImp> srequest)
{
    bool expected = true;

    bool result = std::atomic_compare_exchange_strong(&GetReceiveResponsePending(), &expected, false);
    if (result)
    {
        bool redirectPending;
        {
            expected = true;
            redirectPending = std::atomic_compare_exchange_strong(&GetRedirectPending(), &expected, false);
            TRACE("%-35s:%-8d:%-16p redirectPending = %d ResponseCallbackSendCounter = %d\n", __func__, __LINE__, (void*)this,
                    redirectPending, (int)ResponseCallbackSendCounter());
        }
        GetReceiveResponseMutex().lock();
        if (redirectPending)
        {
            if (ResponseCallbackSendCounter() == 0)
            {
                TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE\n", __func__, __LINE__, (void*)this);
                AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE, 0, NULL, 0, false);
                ResponseCallbackSendCounter()++;
            }
            if (ResponseCallbackSendCounter() == 1)
            {
                TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED\n", __func__, __LINE__, (void*)this);
                AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED, GetHeaderString().length(), NULL, 0, false);
                ResponseCallbackSendCounter()++;
            }
            AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_REDIRECT, 0, NULL, 0, false);
            ResponseCallbackSendCounter()++;
        }
        if (ResponseCallbackSendCounter() == (0 + redirectPending * 3))
        {
            TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE\n", __func__, __LINE__, (void*)this);
            AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE, 0, NULL, 0, false);
            ResponseCallbackSendCounter()++;
        }
        if (ResponseCallbackSendCounter() == (1 + redirectPending * 3))
        {
            TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED\n", __func__, __LINE__, (void*)this);
            AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED, GetHeaderString().length(), NULL, 0, false);
            ResponseCallbackSendCounter()++;
        }
        if (ResponseCallbackSendCounter() == (2 + redirectPending * 3))
        {
            TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE\n", __func__, __LINE__, (void*)this);
            SetHeaderReceiveComplete();
            AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, 0, NULL, 0, false);
            ResponseCallbackSendCounter()++;
        }
        GetReceiveResponseMutex().unlock();
    }
}

size_t WinHttpRequestImp::WriteHeaderFunction(void *ptr, size_t size, size_t nmemb, void* rqst) {
    WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(rqst);
    std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
    if (!srequest)
        return size * nmemb;
    bool EofHeaders = false;

    TRACE("%-35s:%-8d:%-16p %zu\n", __func__, __LINE__, (void*)request, size * nmemb);
    {
        std::lock_guard<std::mutex> lck(request->GetHeaderStringMutex());

        if (request->GetAsync() && request->GetHeaderString().length() == 0) {
            request->ResponseCallbackEventCounter()++;

            if (request->GetReceiveResponsePending()) {
                request->GetReceiveResponseMutex().lock();

                if (request->ResponseCallbackSendCounter() == 0) {
                    TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE\n", __func__, __LINE__, (void*)request);
                    request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE, 0, NULL, 0, false);
                    request->ResponseCallbackSendCounter()++;
                }
                request->GetReceiveResponseMutex().unlock();
            }
        }

        request->GetHeaderString().append(reinterpret_cast<char*>(ptr), size * nmemb);
        request->GetTotalHeaderStringLength() += size * nmemb;

        if (request->GetHeaderString().find("\r\n\r\n") != std::string::npos)
            EofHeaders = true;
        if (request->GetHeaderString().find("\n\n") != std::string::npos)
            EofHeaders = true;

        if (request->GetAsync() && EofHeaders) {
            request->ResponseCallbackEventCounter()++;

            if (request->GetReceiveResponsePending())
            {
                request->GetReceiveResponseMutex().lock();

                if (request->ResponseCallbackSendCounter() == 0)
                {
                    TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE\n", __func__, __LINE__, (void*)request);
                    request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE, 0, NULL, 0, false);
                    request->ResponseCallbackSendCounter()++;
                }
                if (request->ResponseCallbackSendCounter() == 1)
                {
                    TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED\n", __func__, __LINE__, (void*)request);
                    request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED, request->GetHeaderString().length(), NULL, 0, false);
                    request->ResponseCallbackSendCounter()++;
                }
                request->GetReceiveResponseMutex().unlock();
            }
        }
    }
    if (EofHeaders && request->GetAsync())
    {
        DWORD retValue;

        curl_easy_getinfo(request->GetCurl(), CURLINFO_RESPONSE_CODE, &retValue);
        if (retValue == 302)
        {
            std::lock_guard<std::mutex> lck(request->GetHeaderStringMutex());
            request->GetHeaderString() = "";
            TRACE("%-35s:%-8d:%-16p Redirect \n", __func__, __LINE__, (void*)request);
            request->ResponseCallbackEventCounter()++;
            request->GetRedirectPending() = true;
            return size * nmemb;
        }

        request->ResponseCallbackEventCounter()++;
        TRACE("%-35s:%-8d:%-16p HandleReceiveNotifications \n", __func__, __LINE__, (void*)request);
        request->HandleReceiveNotifications(srequest);
        {
            std::lock_guard<std::mutex> lck(request->GetReceiveCompletionEventMtx());
            request->GetReceiveCompletionEventCounter()++;
            request->GetReceiveCompletionEvent().notify_all();
            TRACE("%-35s:%-8d:%-16p GetReceiveCompletionEvent().notify_all\n", __func__, __LINE__, (void*)request);
        }

    }
    return size * nmemb;
}

size_t WinHttpRequestImp::WriteBodyFunction(void *ptr, size_t size, size_t nmemb, void* rqst) {
    WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(rqst);
    std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
    if (!srequest)
        return size * nmemb;

    {
        std::lock_guard<std::mutex> lck(request->GetBodyStringMutex());
        request->GetResponseString().insert(request->GetResponseString().end(),
                                            reinterpret_cast<const BYTE*>(ptr),
                                            reinterpret_cast<const BYTE*>(ptr) + size * nmemb);
    }

    if (request->GetAsync()) {
        bool expected = true;

        bool result = std::atomic_compare_exchange_strong(&request->GetReceiveResponsePending(), &expected, false);

        if (result) {
            request->GetReceiveResponseMutex().lock();
            if (request->ResponseCallbackSendCounter() == 0)
            {
                TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE\n", __func__, __LINE__, (void*)request);
                request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE, 0, NULL, 0, false);
                request->ResponseCallbackSendCounter()++;
            }

            if (request->ResponseCallbackSendCounter() == 1)
            {
                TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED\n", __func__, __LINE__, (void*)request);
                request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED, request->GetHeaderString().length(), NULL, 0, false);
                request->ResponseCallbackSendCounter()++;
            }

            if (request->ResponseCallbackSendCounter() == 2)
            {
                TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE\n", __func__, __LINE__, (void*)request);
                request->SetHeaderReceiveComplete();
                request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, 0, NULL, 0, false);
                request->ResponseCallbackSendCounter()++;
            }
            {
                std::lock_guard<std::mutex> lck(request->GetReceiveCompletionEventMtx());
                request->GetReceiveCompletionEventCounter()++;
                request->GetReceiveCompletionEvent().notify_all();
                TRACE("%-35s:%-8d:%-16p GetReceiveCompletionEvent().notify_all\n", __func__, __LINE__, (void*)request);
            }
            request->GetReceiveResponseMutex().unlock();
        }

        {
            std::lock_guard<std::mutex> lck(request->GetDataAvailableMtx());
            request->GetDataAvailableEventCounter()++;
            request->GetDataAvailableEvent().notify_all();
            TRACE_VERBOSE("%s:%d data available:%lu\n", __func__, __LINE__, size * nmemb);
        }

        if (request->HandleQueryDataNotifications(srequest, size * nmemb))
            TRACE("%-35s:%-8d:%-16p GetQueryDataEvent().notify_all\n", __func__, __LINE__, (void*)request);
    }

    return size * nmemb;
}

void WinHttpRequestImp::CleanUp()
{
    m_CompletionCode = CURLE_OK;
    m_ResponseString.clear();
    m_HeaderString.clear();
    m_TotalHeaderStringLength = 0;
    m_TotalReceiveSize = 0;
    m_ReadData.clear();
    m_ReadDataEventCounter = 0;
    m_UploadThreadExitStatus = false;
    m_ReceiveCompletionEventCounter = 0;
    m_CompletionEventState = false;
    m_QueryDataPending = false;
    m_ReceiveResponsePending = false;
    m_RedirectPending = false;
    m_ReceiveResponseEventCounter = 0;
    m_ReceiveResponseSendCounter = 0;
    m_DataAvailableEventCounter = 0;
    m_OutstandingWrites.clear();
    m_Completion = false;
}

WinHttpRequestImp::WinHttpRequestImp():
            m_QueryDataPending(false),
            m_ReceiveCompletionEventCounter(0),
            m_ReceiveResponsePending(false),
            m_ReceiveResponseEventCounter(0),
            m_ReceiveResponseSendCounter(0),
            m_RedirectPending(false),
            m_DataAvailableEventCounter(0)
{
    m_curl = ComContainer::GetInstance().AllocCURL();
    if (!m_curl)
        return;
    return;
}

WinHttpRequestImp::~WinHttpRequestImp()
{
    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)this);

    m_closed = true;

    if (!GetAsync() && (GetType() == "PUT"))
    {
        {
            std::lock_guard<std::mutex> lck(GetReadDataEventMtx());
            GetReadDataEventCounter()++;
        }
        THREADJOIN(m_UploadCallbackThread);
    }

    if (GetAsync()) {
            TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)this);
    }
    else
        curl_easy_getinfo(GetCurl(), CURLINFO_PRIVATE, NULL);

    /* always cleanup */
    ComContainer::GetInstance().FreeCURL(m_curl);

    /* free the custom headers */
    if (m_HeaderList)
        curl_slist_free_all(m_HeaderList);

    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)this);
}

static void RequestCompletionCb(std::shared_ptr<WinHttpRequestImp> requestRef, DWORD status)
{
    if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)
    {
        TRACE_VERBOSE("%-35s:%-8d:%-16p status:0x%lx\n", __func__, __LINE__, (void*)(requestRef.get()), status);
        requestRef->GetClosed() = true;
    }
}

BOOL WinHttpRequestImp::AsyncQueue(std::shared_ptr<WinHttpRequestImp> &requestRef,
                                    DWORD dwInternetStatus, size_t statusInformationLength,
                                  LPVOID statusInformation, DWORD statusInformationCopySize,
                                  bool allocate)
{
    UserCallbackContext* ctx = NULL;
    DWORD dwNotificationFlags;
    WINHTTP_STATUS_CALLBACK cb = GetCallback(&dwNotificationFlags);
    LPVOID userdata = NULL;

    if (this->GetUserData())
        userdata = (LPVOID)GetUserData();

    ctx = new UserCallbackContext(requestRef, dwInternetStatus, (DWORD)statusInformationLength,
                                    dwNotificationFlags, cb, userdata, (LPVOID)statusInformation,
                                    statusInformationCopySize, allocate, RequestCompletionCb);
    if (ctx) {
        UserCallbackContainer::GetInstance().Queue(ctx);
    }
    else
        return FALSE;


    return TRUE;
}


size_t WinHttpRequestImp::ReadCallback(void *ptr, size_t size, size_t nmemb, void *userp)
{
    WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(userp);
    std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
    if (!srequest)
        return size * nmemb;

    size_t len;

    TRACE("request->GetTotalLength(): %lu\n", request->GetTotalLength());
    if (request->GetOptionalData().length() > 0)
    {
        len = request->GetOptionalData().length();
        TRACE("%s:%d writing optional length of %lu\n", __func__, __LINE__, len);
        std::copy(request->GetOptionalData().begin(), request->GetOptionalData().end(), (char*)ptr);
        request->GetOptionalData().erase(0, len);
        request->GetReadLength() += len;
        return len;
    }

    if (request->GetClosed())
        return -1;

    TRACE("%-35s:%-8d:%-16p request->GetTotalLength():%lu request->GetReadLength():%lu\n", __func__, __LINE__, (void*)request, request->GetTotalLength(), request->GetReadLength());
    if (((request->GetTotalLength() == 0) && (request->GetOptionalData().length() == 0) && (request->GetType() == "PUT")) ||
        (request->GetTotalLength() != request->GetReadLength()))
    {
        std::unique_lock<std::mutex> getReadDataEventHndlMtx(request->GetReadDataEventMtx());
        if (!request->GetReadDataEventCounter())
        {
            TRACE("%s:%d transfer paused:%lu\n", __func__, __LINE__, size * nmemb);
            return CURL_READFUNC_PAUSE;
        }
        request->GetReadDataEventCounter()--;
        TRACE("%-35s:%-8d:%-16p transfer resumed as already signalled:%lu\n", __func__, __LINE__, (void*)request, size * nmemb);
    }

    if (request->GetAsync())
    {
        DWORD dwInternetStatus;
        DWORD result;
        size_t written = 0;

        {
            request->GetReadDataEventMtx().lock();
            BufferRequest buf = request->GetWriteRequest();
            len = MIN(buf.m_Length, size * nmemb);
            request->GetReadLength() += len;
            request->GetReadDataEventMtx().unlock();

            TRACE("%s:%d writing additional length:%lu written:%lu\n", __func__, __LINE__, len, written);
            if (len)
                memcpy((char*)ptr, buf.m_Buffer, len);

            result = len;
            dwInternetStatus = WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE;
            request->AsyncQueue(srequest, dwInternetStatus, sizeof(dwInternetStatus), &result, sizeof(result), true);

            written += len;
        }

        len = written;
    }
    else
    {
        request->GetReadDataEventMtx().lock();
        len = MIN(request->GetReadData().size(), size * nmemb);
        TRACE("%-35s:%-8d:%-16p writing additional length:%lu\n", __func__, __LINE__, (void*)request, len);
        std::copy(request->GetReadData().begin(), request->GetReadData().begin() + len, (char*)ptr);
        request->GetReadLength() += len;
        request->GetReadData().erase(request->GetReadData().begin(), request->GetReadData().begin() + len);
        request->GetReadData().shrink_to_fit();
        request->GetReadDataEventMtx().unlock();
    }

    return len;
}

int WinHttpRequestImp::SocketCallback(CURL *handle, curl_infotype type,
    char *data, size_t size,
    void *userp)
{
    const char *text = "";

    switch (type) {
    case CURLINFO_TEXT:
        TRACE_VERBOSE("%-35s:%-8d:%-16p == Info: %s", __func__, __LINE__, (void*)userp, data);
        return 0;
    default: /* in case a new one is introduced to shock us */
        TRACE_VERBOSE("%-35s:%-8d:%-16p type:%d\n", __func__, __LINE__, (void*)userp, type);
        return 0;

    case CURLINFO_HEADER_IN:
        text = "=> Receive header";
        break;
    case CURLINFO_HEADER_OUT:
        text = "=> Send header";
        break;
    case CURLINFO_DATA_OUT:
        text = "=> Send data";
        break;
    case CURLINFO_SSL_DATA_OUT:
        text = "=> Send SSL data";
        break;
    case CURLINFO_DATA_IN:
        text = "<= Recv data";
        break;
    case CURLINFO_SSL_DATA_IN:
        text = "<= Recv SSL data";
        break;
    }
    TRACE_VERBOSE("%-35s:%-8d:%-16p %s\n", __func__, __LINE__, (void*)userp, text);

    return 0;
}

WINHTTPAPI HINTERNET WINAPI WinHttpOpen
(
    LPCTSTR pszAgentW,
    DWORD dwAccessType,
    LPCTSTR pszProxyW,
    LPCTSTR pszProxyBypassW,
    DWORD dwFlags
)
{
    std::shared_ptr<WinHttpSessionImp> session = std::make_shared<WinHttpSessionImp> ();

    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*) (session.get()));
    if (dwAccessType == WINHTTP_ACCESS_TYPE_NAMED_PROXY)
    {
        if (!pszProxyW)
            return NULL;

        ConvertCstrAssign(pszProxyW, WCTLEN(pszProxyW), session->GetProxy());

        std::vector<std::string> proxies = Split(session->GetProxy(), ';');
        session->SetProxies(proxies);
    }

    if (dwFlags & WINHTTP_FLAG_ASYNC) {
        session->SetAsync();
    }

    WinHttpHandleContainer<WinHttpSessionImp>::Instance().Register(session);
    return session.get();
}

WINHTTPAPI HINTERNET WINAPI WinHttpConnect
(
    HINTERNET hSession,
    LPCTSTR pswzServerName,
    INTERNET_PORT nServerPort,
    DWORD dwReserved
)
{
    WinHttpSessionImp *session = static_cast<WinHttpSessionImp *>(hSession);

    TRACE("%-35s:%-8d:%-16p pswzServerName: " STRING_LITERAL " nServerPort:%d\n",
          __func__, __LINE__, (void*)session, pswzServerName, nServerPort);
    ConvertCstrAssign(pswzServerName, WCTLEN(pswzServerName), session->GetServerName());

    session->SetServerPort(nServerPort);
    std::shared_ptr<WinHttpConnectImp> connect = std::make_shared<WinHttpConnectImp> ();
    if (!connect)
        return NULL;

    connect->SetHandle(session);
    WinHttpHandleContainer<WinHttpConnectImp>::Instance().Register(connect);
    return connect.get();
}


BOOLAPI WinHttpCloseHandle(
    HINTERNET hInternet
)
{
    WinHttpBase *base = static_cast<WinHttpBase *>(hInternet);

    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)base);
    if (!base)
        return FALSE;

    if (WinHttpHandleContainer<WinHttpRequestImp>::Instance().IsRegistered(reinterpret_cast<WinHttpRequestImp *>(hInternet)))
    {
        WinHttpRequestImp *request = dynamic_cast<WinHttpRequestImp *>(base);
        if (!request)
            return FALSE;

        std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
        if (!srequest)
            return FALSE;

        request->GetClosing() = true;
        WinHttpHandleContainer<WinHttpRequestImp>::Instance().UnRegister(request);
        return TRUE;
    }

    if (WinHttpHandleContainer<WinHttpSessionImp>::Instance().IsRegistered(reinterpret_cast<WinHttpSessionImp *>(hInternet)))
    {
        WinHttpSessionImp *session = dynamic_cast<WinHttpSessionImp *>(base);
        if (session)
        {
            WinHttpHandleContainer<WinHttpSessionImp>::Instance().UnRegister(session);
            return TRUE;
        }
        return TRUE;
    }

    if (WinHttpHandleContainer<WinHttpConnectImp>::Instance().IsRegistered(reinterpret_cast<WinHttpConnectImp *>(hInternet)))
    {
        WinHttpConnectImp *connect = dynamic_cast<WinHttpConnectImp *>(base);
        if (connect)
        {
            WinHttpHandleContainer<WinHttpConnectImp>::Instance().UnRegister(connect);
            return TRUE;
        }
    }

    return TRUE;
}

WINHTTPAPI HINTERNET WINAPI WinHttpOpenRequest(
    HINTERNET hConnect,
    LPCTSTR pwszVerb,  /* include "GET", "POST", and "HEAD" */
    LPCTSTR pwszObjectName,
    LPCTSTR pwszVersion,
    LPCTSTR pwszReferrer,
    LPCTSTR * ppwszAcceptTypes,
    DWORD dwFlags /* isSecurePort ? (WINHTTP_FLAG_REFRESH | WINHTTP_FLAG_SECURE) : WINHTTP_FLAG_REFRESH */
)
{
    WinHttpConnectImp *connect = static_cast<WinHttpConnectImp *>(hConnect);
    WinHttpSessionImp *session = connect->GetHandle();
    CURLcode res;

    if (!pwszVerb)
        pwszVerb = TEXT("GET");

    TRACE("%-35s:%-8d:%-16p pwszVerb = " STRING_LITERAL " pwszObjectName: " STRING_LITERAL " pwszVersion = " STRING_LITERAL " pwszReferrer = " STRING_LITERAL "\n",
        __func__, __LINE__, (void*)session, pwszVerb, pwszObjectName, pwszVersion, pwszReferrer);

    std::shared_ptr<WinHttpRequestImp> srequest(new WinHttpRequestImp, [](WinHttpRequestImp *request) {

        std::shared_ptr<WinHttpRequestImp> close_request(request);
        TRACE("%-35s:%-8d:%-16p reference count reached to 0\n", __func__, __LINE__, (void*)request);

        TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING\n", __func__, __LINE__, (void*)request);
        request->AsyncQueue(close_request, WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING, 0, NULL, 0, false);
    });

    if (!srequest)
        return NULL;
    WinHttpRequestImp *request = srequest.get();
    if (!request)
        return NULL;

    if (session->GetAsync())
    {
        DWORD flag;
        WINHTTP_STATUS_CALLBACK cb;
        LPVOID userdata = NULL;

        cb = session->GetCallback(&flag);
        request->SetAsync();
        request->SetCallback(cb, flag);

        if (connect->GetUserData())
            userdata = (LPVOID)connect->GetUserData();
        else if (session->GetUserData())
            userdata = (LPVOID)session->GetUserData();

        if (userdata)
            request->SetUserData(&userdata);

    }

    const char *prefix;
    std::string server = session->GetServerName();
    if (dwFlags & WINHTTP_FLAG_SECURE)
        prefix = "https://";
    else
        prefix = "http://";

    if (server.find("http://") == std::string::npos)
        server = prefix + server;
    if (pwszObjectName)
    {
        std::string objectname;

        ConvertCstrAssign(pwszObjectName, WCTLEN(pwszObjectName), objectname);

        size_t index = 0;
        // convert # to %23 to support links to fragments
        while (true) {
            /* Locate the substring to replace. */
            index = objectname.find("#", index);
            if (index == std::string::npos) break;
            /* Make the replacement. */
            objectname.replace(index, 1, "%23");
            /* Advance index forward so the next iteration doesn't pick it up as well. */
            index += 3;
        }
        request->SetFullPath(server, objectname);
        if (!request->SetServer(request->GetFullPath(), session->GetServerPort())) {
            return NULL;
        }
    }
    else
    {
        std::string nullstr("");
        request->SetFullPath(server, nullstr);
        if (!request->SetServer(request->GetFullPath(), session->GetServerPort())) {
            return NULL;
        }
    }

    if (session->GetTimeout() > 0)
    {
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_TIMEOUT_MS, session->GetTimeout());
        if (res != CURLE_OK) {
            return NULL;
        }
    }

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_LOW_SPEED_TIME, 60L);
    if (res != CURLE_OK)
        return NULL;
    res = curl_easy_setopt(request->GetCurl(), CURLOPT_LOW_SPEED_LIMIT, 1L);
    if (res != CURLE_OK)
        return NULL;

    request->SetProxy(session->GetProxies());

    if (WCTCMP(pwszVerb, (const TCHAR *)TEXT("PUT")) == 0)
    {
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_PUT, 1L);
        if (res != CURLE_OK) {
            return NULL;
        }

        res = curl_easy_setopt(request->GetCurl(), CURLOPT_UPLOAD, 1L);
        if (res != CURLE_OK) {
            return NULL;
        }
    }
    else if (WCTCMP(pwszVerb, (const TCHAR *)TEXT("GET")) == 0)
    {
    }
    else if (WCTCMP(pwszVerb, (const TCHAR *)TEXT("POST")) == 0)
    {
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_POST, 1L);
        if (res != CURLE_OK) {
            return NULL;
        }
    }
    else
    {
        std::string verb;

        ConvertCstrAssign(pwszVerb, WCTLEN(pwszVerb), verb);
        TRACE("%-35s:%-8d:%-16p setting custom header %s\n", __func__, __LINE__, (void*)request, verb.c_str());
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_CUSTOMREQUEST, verb.c_str());
        if (res != CURLE_OK) {
            return NULL;
        }
    }

    if (pwszVersion)
    {
        int ver;

        if (WCTCMP(pwszVerb, (const TCHAR*)TEXT("1.0")) == 0)
            ver = CURL_HTTP_VERSION_1_0;
        else if (WCTCMP(pwszVerb, (const TCHAR*)TEXT("1.1")) == 0)
            ver = CURL_HTTP_VERSION_1_1;
        else if (WCTCMP(pwszVerb, (const TCHAR*)TEXT("2.0")) == 0)
            ver = CURL_HTTP_VERSION_2_0;
        else
            return NULL;

        res = curl_easy_setopt(request->GetCurl(), CURLOPT_HTTP_VERSION, ver);
        if (res != CURLE_OK) {
            return NULL;
        }
    }

    if (pwszReferrer)
    {
        ConvertCstrAssign(pwszReferrer, WCTLEN(pwszReferrer), session->GetReferrer());

        res = curl_easy_setopt(request->GetCurl(), CURLOPT_REFERER, session->GetReferrer().c_str());
        if (res != CURLE_OK) {
            return NULL;
        }
    }

    if (dwFlags & WINHTTP_FLAG_SECURE)
    {
        const char *pKeyName;
        const char *pKeyType;
        const char *pEngine;
        static const char *pCertFile = "testcert.pem";
        static const char *pCACertFile = "cacert.pem";
        //static const char *pHeaderFile = "dumpit";
        const char *pPassphrase = NULL;

#ifdef USE_ENGINE
        pKeyName = "rsa_test";
        pKeyType = "ENG";
        pEngine = "chil";            /* for nChiper HSM... */
#else
        pKeyName = NULL;
        pKeyType = "PEM";
        pCertFile = NULL;
        pCACertFile = NULL;
        pEngine = NULL;
#endif
        if (const char* env_p = std::getenv("WINHTTP_PAL_KEYNAME"))
            pKeyName = env_p;

        if (const char* env_p = std::getenv("WINHTTP_PAL_KEYTYPE"))
            pKeyType = env_p;

        if (const char* env_p = std::getenv("WINHTTP_PAL_CERTFILE"))
            pCertFile = env_p;

        if (const char* env_p = std::getenv("WINHTTP_PAL_CACERTFILE"))
            pCACertFile = env_p;

        if (const char* env_p = std::getenv("WINHTTP_PAL_ENGINE"))
            pEngine = env_p;

        if (pEngine) {
            if (curl_easy_setopt(request->GetCurl(), CURLOPT_SSLENGINE, pEngine) != CURLE_OK) {
                /* load the crypto engine */
                TRACE("%s", "can't set crypto engine\n");
                return NULL;
            }
            if (curl_easy_setopt(request->GetCurl(), CURLOPT_SSLENGINE_DEFAULT, 1L) != CURLE_OK) {
                /* set the crypto engine as default */
                /* only needed for the first time you load
                   a engine a curl object... */
                TRACE("%s", "can't set crypto engine as default\n");
                return NULL;
            }
        }
        /* cert is stored PEM coded in file... */
        /* since PEM is default, we needn't set it for PEM */
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_SSLCERTTYPE, "PEM");
        if (res != CURLE_OK) {
            return NULL;
        }

        /* set the cert for client authentication */
        if (pCertFile) {
            res = curl_easy_setopt(request->GetCurl(), CURLOPT_SSLCERT, pCertFile);
            if (res != CURLE_OK) {
                return NULL;
            }
        }

        /* sorry, for engine we must set the passphrase
           (if the key has one...) */
        if (pPassphrase) {
            res = curl_easy_setopt(request->GetCurl(), CURLOPT_KEYPASSWD, pPassphrase);
            if (res != CURLE_OK) {
                return NULL;
            }
        }

        /* if we use a key stored in a crypto engine,
           we must set the key type to "ENG" */
        if (pKeyType) {
            res = curl_easy_setopt(request->GetCurl(), CURLOPT_SSLKEYTYPE, pKeyType);
            if (res != CURLE_OK) {
                return NULL;
            }
        }

        /* set the private key (file or ID in engine) */
        if (pKeyName) {
            res = curl_easy_setopt(request->GetCurl(), CURLOPT_SSLKEY, pKeyName);
            if (res != CURLE_OK) {
                return NULL;
            }
        }

        /* set the file with the certs vaildating the server */
        if (pCACertFile) {
            res = curl_easy_setopt(request->GetCurl(), CURLOPT_CAINFO, pCACertFile);
            if (res != CURLE_OK) {
                return NULL;
            }
        }
    }
    if (pwszVerb)
    {
        ConvertCstrAssign(pwszVerb, WCTLEN(pwszVerb), request->GetType());
    }
    request->SetSession(connect);
    WinHttpHandleContainer<WinHttpRequestImp>::Instance().Register(srequest);

    return request;
}

BOOLAPI
WinHttpAddRequestHeaders
(
    HINTERNET hRequest,
    LPCTSTR lpszHeaders,
    DWORD dwHeadersLength,
    DWORD dwModifiers
)
{
    CURLcode res;
    WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(hRequest);
    if (!request)
        return FALSE;

    std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
    if (!srequest)
        return FALSE;

    TRACE("%-35s:%-8d:%-16p lpszHeaders = " STRING_LITERAL " dwModifiers: %lu\n", __func__, __LINE__, (void*)request,
        lpszHeaders ? lpszHeaders: TEXT(""), dwModifiers);
    if (lpszHeaders)
    {
        size_t nstringLen;

        if (dwHeadersLength == (DWORD)-1)
        {
            nstringLen = WCTLEN(lpszHeaders) + 1;
        }
        else
        {
            nstringLen = dwHeadersLength + 1;
        }

        std::string &headers = request->GetOutgoingHeaderList();

        ConvertCstrAssign(lpszHeaders, nstringLen, headers);
        request->AddHeader(headers);

        res = curl_easy_setopt(request->GetCurl(), CURLOPT_HTTPHEADER, request->GetHeaderList());
        if (res != CURLE_OK)
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOLAPI WinHttpSendRequest
(
    HINTERNET hRequest,
    LPCTSTR lpszHeaders,
    DWORD dwHeadersLength,
    LPVOID lpOptional,
    DWORD dwOptionalLength,
    DWORD dwTotalLength,
    DWORD_PTR dwContext
)
{
    CURLcode res;
    WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(hRequest);
    if (!request)
        return FALSE;

    WinHttpConnectImp *connect = request->GetSession();
    WinHttpSessionImp *session = connect->GetHandle();

    std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
    if (!srequest)
        return FALSE;

    TSTRING customHeader;

    if (lpszHeaders)
        customHeader.assign(lpszHeaders, WCTLEN(lpszHeaders));

    if ((dwTotalLength == 0) && (dwOptionalLength == 0) && (request->GetType() == "PUT"))
        customHeader += TEXT("Transfer-Encoding: chunked\r\n");

    TRACE("%-35s:%-8d:%-16p lpszHeaders:%p dwHeadersLength:%lu lpOptional:%p dwOptionalLength:%lu dwTotalLength:%lu\n",
        __func__, __LINE__, (void*)request, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength, dwTotalLength);

    if ((customHeader != TEXT("")) && !WinHttpAddRequestHeaders(hRequest, customHeader.c_str(), customHeader.length(), 0))
        return FALSE;

    if (lpOptional)
    {
        if (dwOptionalLength == 0) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        request->SetOptionalData(lpOptional, dwOptionalLength);

        if (request->GetType() == "POST")
        {
            /* Now specify the POST data */
            res = curl_easy_setopt(request->GetCurl(), CURLOPT_POSTFIELDS, request->GetOptionalData().c_str());
            if (res != CURLE_OK)
                return FALSE;
        }
    }

    if ((request->GetType() == "PUT") || (request->GetType() == "POST"))
    {
        if (!request->GetAsync() && (dwTotalLength == 0)) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }

    if (dwOptionalLength == (DWORD)-1)
        dwOptionalLength = request->GetOptionalData().length();

    DWORD totalsize = MAX(dwOptionalLength, dwTotalLength);
    /* provide the size of the upload, we specicially typecast the value
        to curl_off_t since we must be sure to use the correct data size */
    if (request->GetType() == "POST")
    {
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_POSTFIELDSIZE, (long)totalsize);
        if (res != CURLE_OK)
            return FALSE;
    }
    else if (totalsize)
    {
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_INFILESIZE_LARGE, (curl_off_t)totalsize);
        if (res != CURLE_OK)
            return FALSE;
    }

    request->GetTotalLength() = dwTotalLength;
    res = curl_easy_setopt(request->GetCurl(), CURLOPT_READDATA, request);
    if (res != CURLE_OK)
        return FALSE;

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_READFUNCTION, request->ReadCallback);
    if (res != CURLE_OK)
        return FALSE;

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_DEBUGFUNCTION, request->SocketCallback);
    if (res != CURLE_OK)
        return FALSE;

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_DEBUGDATA, request);
    if (res != CURLE_OK)
        return FALSE;

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_WRITEFUNCTION, request->WriteBodyFunction);
    if (res != CURLE_OK)
        return FALSE;

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_WRITEDATA, request);
    if (res != CURLE_OK)
        return FALSE;

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_HEADERFUNCTION, request->WriteHeaderFunction);
    if (res != CURLE_OK)
        return FALSE;

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_HEADERDATA, request);
    if (res != CURLE_OK)
        return FALSE;

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_PRIVATE, request);
    if (res != CURLE_OK)
        return FALSE;

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_FOLLOWLOCATION, 1L);
    if (res != CURLE_OK)
        return FALSE;

    if (winhttp_tracing_verbose)
    {
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_VERBOSE, 1);
        if (res != CURLE_OK)
            return FALSE;
    }

    /* enable TCP keep-alive for this transfer */
    res = curl_easy_setopt(request->GetCurl(), CURLOPT_TCP_KEEPALIVE, 1L);
    if (res != CURLE_OK)
        return FALSE;

    /* keep-alive idle time to 120 seconds */
    res = curl_easy_setopt(request->GetCurl(), CURLOPT_TCP_KEEPIDLE, 120L);
    if (res != CURLE_OK)
        return FALSE;

    /* interval time between keep-alive probes: 60 seconds */
    res = curl_easy_setopt(request->GetCurl(), CURLOPT_TCP_KEEPINTVL, 60L);
    if (res != CURLE_OK)
        return FALSE;

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_SSL_VERIFYPEER, request->VerifyPeer());
    if (res != CURLE_OK) {
        return FALSE;
    }

    res = curl_easy_setopt(request->GetCurl(), CURLOPT_SSL_VERIFYHOST, request->VerifyHost());
    if (res != CURLE_OK) {
        return FALSE;
    }

    DWORD maxConnections = 0;

    if (request->GetMaxConnections())
        maxConnections = request->GetMaxConnections();
    else if (session->GetMaxConnections())
        maxConnections = session->GetMaxConnections();

    if (maxConnections) {
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_MAXCONNECTS, maxConnections);
        if (res != CURLE_OK)
            return FALSE;
    }

    if (dwContext)
        request->SetUserData(reinterpret_cast<void**>(&dwContext));

    DWORD securityProtocols = 0;

    if (request->GetSecureProtocol())
        securityProtocols = request->GetSecureProtocol();
    else if (session->GetSecureProtocol())
        securityProtocols = session->GetSecureProtocol();

    if (securityProtocols) {
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_SSLVERSION, securityProtocols);
        if (res != CURLE_OK)
            return FALSE;
    }

    if (request->GetAsync())
    {
        if (request->GetClosing())
        {
            TRACE("%-35s:%-8d:%-16p \n", __func__, __LINE__, (void*)request);
            return FALSE;
        }
        request->CleanUp();

        TRACE("%-35s:%-8d:%-16p use_count = %lu\n", __func__, __LINE__, (void*)request, srequest.use_count());
        request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_SENDING_REQUEST, 0, NULL, 0, false);

        TRACE("%-35s:%-8d:%-16p use_count = %lu\n", __func__, __LINE__, (void*)request, srequest.use_count());

        if (!ComContainer::GetInstance().AddHandle(srequest, request->GetCurl()))
            return FALSE;

        TRACE("%-35s:%-8d:%-16p use_count = %lu\n", __func__, __LINE__, (void*)request, srequest.use_count());
        ComContainer::GetInstance().KickStart();

        TRACE("%-35s:%-8d:%-16p use_count = %lu\n", __func__, __LINE__, (void*)request, srequest.use_count());
        request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_REQUEST_SENT, 0, NULL, 0, false);

        TRACE("%-35s:%-8d:%-16p use_count = %lu\n", __func__, __LINE__, (void*)request, srequest.use_count());
        request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, 0, NULL, 0, false);

        TRACE("%-35s:%-8d:%-16p use_count = %lu\n", __func__, __LINE__, (void*)request, srequest.use_count());
    }
    else
    {
        if (dwTotalLength && (request->GetType() != "POST"))
        {
            DWORD thread_id;
            request->GetUploadThread() = CREATETHREAD(
                request->UploadThreadFunction,       // thread function name
                request, &thread_id);          // argument to thread function
        }
        else
        {
            /* Perform the request, res will get the return code */
            res = curl_easy_perform(request->GetCurl());
            /* Check for errors */
            if (res != CURLE_OK)
            {
                TRACE("curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
                return FALSE;
            }
        }
    }

    return TRUE;
}

void WinHttpRequestImp::WaitAsyncReceiveCompletion(std::shared_ptr<WinHttpRequestImp> srequest)
{
    bool expected = false;
    bool result = std::atomic_compare_exchange_strong(&GetReceiveResponsePending(), &expected, true);

    TRACE("%-35s:%-8d:%-16p GetReceiveResponsePending() = %d\n", __func__, __LINE__, (void*)this, (int) GetReceiveResponsePending());
    std::unique_lock<std::mutex> lck(GetReceiveCompletionEventMtx());
    while (!GetReceiveCompletionEventCounter())
        GetReceiveCompletionEvent().wait(lck);
    lck.unlock();

    if (result && (GetCompletionCode() == CURLE_OK))
    {
        TRACE("%-35s:%-8d:%-16p HandleReceiveNotifications \n", __func__, __LINE__, (void*)this);
        HandleReceiveNotifications(srequest);
    }
}

WINHTTPAPI
BOOL
WINAPI
WinHttpReceiveResponse
(
    HINTERNET hRequest,
    LPVOID lpReserved
)
{
    WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(hRequest);
    if (!request)
        return FALSE;

    std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
    if (!srequest)
        return FALSE;

    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)request);

    if ((request->GetTotalLength() == 0) && (request->GetOptionalData().length() == 0) && (request->GetType() == "PUT"))
    {
        if (request->GetAsync())
        {
            {
                std::lock_guard<std::mutex> lck(request->GetReadDataEventMtx());
                request->GetReadDataEventCounter()++;
                TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)request);
            }

            ComContainer::GetInstance().ResumeTransfer(request->GetCurl(), CURLPAUSE_CONT);
        }
    }

    if (request->GetAsync())
    {
        TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)request);
        request->WaitAsyncReceiveCompletion(srequest);
    }
    else
    {
        if (request->GetType() == "PUT")
        {
            while (request->GetTotalLength() != request->GetReadLength()) {
                if (request->GetUploadThreadExitStatus()) {
                    if (request->GetTotalLength() != request->GetReadLength()) {
                        SetLastError(ERROR_WINHTTP_OPERATION_CANCELLED);
                        return FALSE;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }

            return TRUE;
        }
        size_t headerLength;
        {
            std::lock_guard<std::mutex> lck(request->GetHeaderStringMutex());
            headerLength = request->GetHeaderString().length();
        }

        return headerLength > 0;
    }
    return TRUE;
}

bool WinHttpRequestImp::HandleQueryDataNotifications(std::shared_ptr<WinHttpRequestImp> srequest, size_t available)
{
    bool expected = true;
    bool result = std::atomic_compare_exchange_strong(&GetQueryDataPending(), &expected, false);
    if (result)
    {
        if (!available)
        {
            size_t length;

            GetBodyStringMutex().lock();
            length = GetResponseString().size();
            available = length;
            GetBodyStringMutex().unlock();
        }

        DWORD lpvStatusInformation = static_cast<DWORD>(available);
        TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:%lu\n", __func__, __LINE__, (void*)this, lpvStatusInformation);
        AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE, sizeof(lpvStatusInformation),
            (LPVOID)&lpvStatusInformation, sizeof(lpvStatusInformation), true);
    }
    return result;
}

void WinHttpRequestImp::WaitAsyncQueryDataCompletion(std::shared_ptr<WinHttpRequestImp> srequest)
{
    bool completed;
    GetQueryDataPending() = true;
    TRACE("%-35s:%-8d:%-16p GetQueryDataPending() = %d\n", __func__, __LINE__, (void*)this, (int)GetQueryDataPending());
    {
        std::lock_guard<std::mutex> lck(GetQueryDataEventMtx());
        completed = GetQueryDataEventState();
    }

    if (completed) {
        TRACE("%-35s:%-8d:%-16p transfer already finished\n", __func__, __LINE__, (void*)this);
        HandleQueryDataNotifications(srequest, 0);
    }
}

BOOLAPI
WinHttpQueryDataAvailable
(
    HINTERNET hRequest,
    LPDWORD lpdwNumberOfBytesAvailable
)
{
    WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(hRequest);
    if (!request)
        return FALSE;

    std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
    if (!srequest)
        return FALSE;

    if (request->GetClosing())
    {
        TRACE("%-35s:%-8d:%-16p \n", __func__, __LINE__, (void*)request);
        return FALSE;
    }

    size_t length;

    request->GetBodyStringMutex().lock();
    length = request->GetResponseString().size();
    size_t available = length;
    request->GetBodyStringMutex().unlock();

    if (request->GetAsync())
    {
        if (available == 0)
        {
            TRACE("%-35s:%-8d:%-16p !!!!!!!\n", __func__, __LINE__, (void*)request);
            request->WaitAsyncQueryDataCompletion(srequest);
            request->GetBodyStringMutex().lock();
            length = request->GetResponseString().size();
            available = length;
            TRACE("%-35s:%-8d:%-16p available = %lu\n", __func__, __LINE__, (void*)request, available);
            request->GetBodyStringMutex().unlock();
        }
        else
        {
            TRACE("%-35s:%-8d:%-16p available = %lu\n", __func__, __LINE__, (void*)request, available);
            DWORD lpvStatusInformation = available;
            TRACE("%-35s:%-8d:%-16p WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:%lu\n", __func__, __LINE__, (void*)request, lpvStatusInformation);
            request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE, sizeof(lpvStatusInformation),
                (LPVOID)&lpvStatusInformation, sizeof(lpvStatusInformation), true);

        }

    }


    if (lpdwNumberOfBytesAvailable)
        *lpdwNumberOfBytesAvailable = available;

    return TRUE;
}

size_t WinHttpRequestImp::WaitDataAvailable()
{
    size_t length, remainingLength, readLength;

    GetBodyStringMutex().lock();
    length = GetResponseString().size();
    readLength = remainingLength = length;
    if (GetAsync() == TRUE)
    {
/* check the size */
        while (((length == 0) || (remainingLength == 0)) &&
            (!GetCompletionStatus()))
        {
            GetBodyStringMutex().unlock();

            TRACE("%s:%d\n", __func__, __LINE__);
            ComContainer::GetInstance().KickStart();

            TRACE("%s:%d\n", __func__, __LINE__);
            {
                std::unique_lock<std::mutex> lck(GetDataAvailableMtx());
                while (!GetDataAvailableEventCounter() && !GetCompletionStatus())
                    GetDataAvailableEvent().wait(lck);
                lck.unlock();

                TRACE("%s:%d\n", __func__, __LINE__);
                GetDataAvailableEventCounter()--;
            }

            GetBodyStringMutex().lock();

            length = GetResponseString().size();
            readLength = remainingLength = length;
            TRACE("%s:%d length:%lu readLength:%lu completion: %d\n", __func__, __LINE__,
                length, readLength, GetCompletionStatus());
        }
    }
    GetBodyStringMutex().unlock();
    return readLength;
}

BOOLAPI
WinHttpReadData
(
    HINTERNET hRequest,
    LPVOID lpBuffer,
    DWORD dwNumberOfBytesToRead,
    LPDWORD lpdwNumberOfBytesRead
)
{
    WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(hRequest);
    if (!request)
        return FALSE;

    std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
    if (!srequest)
        return FALSE;

    if (request->GetClosing())
    {
        TRACE("%-35s:%-8d:%-16p \n", __func__, __LINE__, (void*)request);
        return FALSE;
    }

    size_t readLength;

    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)request);
    if (dwNumberOfBytesToRead == 0)
    {
        if (lpdwNumberOfBytesRead)
            *lpdwNumberOfBytesRead = 0;

        if (request->GetAsync())
        {
            LPVOID StatusInformation = (LPVOID)lpBuffer;

            request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_READ_COMPLETE, 0, (LPVOID)StatusInformation, sizeof(StatusInformation), false);
        }
        return TRUE;
    }

    readLength = request->WaitDataAvailable();

    request->GetBodyStringMutex().lock();
    if (readLength > dwNumberOfBytesToRead)
    {
        readLength = dwNumberOfBytesToRead;
    }
    if (readLength)
    {
        std::copy(request->GetResponseString().begin(), request->GetResponseString().begin() + readLength, reinterpret_cast<char *>(lpBuffer));
        request->GetResponseString().erase(request->GetResponseString().begin(), request->GetResponseString().begin() + readLength);
        request->GetResponseString().shrink_to_fit();
    }
    request->GetBodyStringMutex().unlock();

    if (readLength == 0)
    {
        TRACE("%s", "!!!!!!!!!!done!!!!!!!!!!!!!!!\n");
    }
    TRACE("%s:%d lpBuffer: %p length:%lu\n", __func__, __LINE__, lpBuffer, readLength);
    if (request->GetAsync())
    {
        LPVOID StatusInformation = (LPVOID)lpBuffer;

        request->AsyncQueue(srequest, WINHTTP_CALLBACK_STATUS_READ_COMPLETE, readLength, (LPVOID)StatusInformation, sizeof(StatusInformation), false);
    }

    if (lpdwNumberOfBytesRead)
        *lpdwNumberOfBytesRead = (DWORD)readLength;
    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)request);
    return TRUE;
}

BOOLAPI
WinHttpSetTimeouts
(
    HINTERNET    hInternet,           // Session/Request handle.
    int          nResolveTimeout,
    int          nConnectTimeout,
    int          nSendTimeout,
    int          nReceiveTimeout
)
{
    WinHttpBase *base = static_cast<WinHttpBase *>(hInternet);
    WinHttpSessionImp *session;
    WinHttpRequestImp *request;
    CURLcode res;

    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)base);
    if ((session = dynamic_cast<WinHttpSessionImp *>(base)))
    {
        session->SetTimeout(nReceiveTimeout);
    }
    else if ((request = dynamic_cast<WinHttpRequestImp *>(base)))
    {
        res = curl_easy_setopt(request->GetCurl(), CURLOPT_TIMEOUT_MS, nReceiveTimeout);
        if (res != CURLE_OK)
            return FALSE;
    }
    else
        return FALSE;

    return TRUE;
}

static int NumDigits(DWORD value)
{
    int count = 0;

    while (value != 0)
    {
        // n = n/10
        value /= 10;
        ++count;
    }

    return count;
}

static TSTRING FindRegex(const TSTRING &subject,const TSTRING &regstr)
{
    TSTRING result;
    try {
        TREGEX re(regstr, std::regex_constants::icase);
        TREGEX_MATCH match;
        if (TREGEX_SEARCH(subject, match, re)) {
            result = match.str(0);
        } else {
            result = TSTRING(TEXT(""));
            return TEXT("");
        }
    } catch (std::regex_error&) {
        return TEXT("");
    }
    return result;
}

bool is_newline(char i)
{
	return (i == '\n') || (i == '\r');
}

template<class CharT>
std::basic_string<CharT> nullize_newlines(const std::basic_string<CharT>& str) {
	std::basic_string<CharT> result;
	result.reserve(str.size());

	auto cursor = str.begin();
	const auto end = str.end();
	for (;;) {
		cursor = std::find_if_not(cursor, end, is_newline);
		if (cursor == end) {
			return result;
		}

		const auto nextNewline = std::find_if(cursor, end, is_newline);
		result.append(cursor, nextNewline);
		result.push_back(CharT{});
		cursor = nextNewline;
	}
}

BOOLAPI WinHttpQueryHeaders(
    HINTERNET   hRequest,
    DWORD       dwInfoLevel,
    LPCTSTR     pwszName,
    LPVOID         lpBuffer,
    LPDWORD lpdwBufferLength,
    LPDWORD lpdwIndex
)
{
    WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(hRequest);
    if (!request)
        return FALSE;

    std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
    if (!srequest)
        return FALSE;

    bool returnDWORD = false;

    if (pwszName != WINHTTP_HEADER_NAME_BY_INDEX)
        return FALSE;

    if (lpdwIndex != WINHTTP_NO_HEADER_INDEX)
        return FALSE;

    if (request->GetHeaderString().length() == 0)
        return FALSE;

    if (dwInfoLevel & WINHTTP_QUERY_FLAG_NUMBER)
    {
        dwInfoLevel &= ~WINHTTP_QUERY_FLAG_NUMBER;
        returnDWORD = true;
    }
    TRACE("%-35s:%-8d:%-16p dwInfoLevel = 0x%lx\n", __func__, __LINE__, (void*)request, dwInfoLevel);

    if (returnDWORD && SizeCheck(lpBuffer, lpdwBufferLength, sizeof(DWORD)) == FALSE)
        return FALSE;

    if (dwInfoLevel == WINHTTP_QUERY_VERSION)
    {
        CURLcode res;
        DWORD retValue;

        res = curl_easy_getinfo(request->GetCurl(), CURLINFO_HTTP_VERSION, &retValue);
        if (res != CURLE_OK)
            return FALSE;

        if (!returnDWORD && SizeCheck(lpBuffer, lpdwBufferLength, (NumDigits(retValue) + 1) * sizeof(TCHAR)) == FALSE)
            return FALSE;

        if (!lpBuffer)
            return FALSE;

        if (returnDWORD)
        {
            memcpy(lpBuffer, &retValue, sizeof(retValue));
        }
        else
        {
            TCHAR *outbuf = static_cast<TCHAR*>(lpBuffer);
            STNPRINTF(outbuf, *lpdwBufferLength, TEXT("%lu"), retValue);
            outbuf[*lpdwBufferLength/sizeof(outbuf[0]) - 1] = TEXT('\0');
        }
        return TRUE;
    }

    if (dwInfoLevel == WINHTTP_QUERY_STATUS_CODE)
    {
        CURLcode res;
        DWORD retValue;

        res = curl_easy_getinfo(request->GetCurl(), CURLINFO_RESPONSE_CODE, &retValue);
        if (res != CURLE_OK)
            return FALSE;

        if (!returnDWORD && SizeCheck(lpBuffer, lpdwBufferLength, (NumDigits(retValue) + 1) * sizeof(TCHAR)) == FALSE)
            return FALSE;

        if (!lpBuffer)
            return FALSE;

        if (returnDWORD)
        {
            memcpy(lpBuffer, &retValue, sizeof(retValue));
        }
        else
        {
            TCHAR *outbuf = static_cast<TCHAR*>(lpBuffer);
            STNPRINTF(outbuf, *lpdwBufferLength, TEXT("%lu"), retValue);
            outbuf[*lpdwBufferLength/sizeof(outbuf[0]) - 1] = TEXT('\0');
        }
        TRACE("%-35s:%-8d:%-16p status code :%lu\n", __func__, __LINE__, (void*)request, retValue);
        return TRUE;
    }

    if (dwInfoLevel == WINHTTP_QUERY_STATUS_TEXT)
    {
        CURLcode res;
        DWORD responseCode;
        size_t length;

        res = curl_easy_getinfo(request->GetCurl(), CURLINFO_RESPONSE_CODE, &responseCode);
        if (res != CURLE_OK)
            return FALSE;

        std::lock_guard<std::mutex> lck(request->GetHeaderStringMutex());
        length = request->GetHeaderString().length();

        if (SizeCheck(lpBuffer, lpdwBufferLength, (length + 1) * sizeof(TCHAR)) == FALSE)
            return FALSE;

        if (!lpBuffer)
            return FALSE;

#ifdef UNICODE
        TSTRING subject;

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        subject = conv.from_bytes(request->GetHeaderString());
        subject[request->GetHeaderString().length()] = TEXT('\0');

#else
        TSTRING &subject = request->GetHeaderString();
#endif

        TSTRING regstr;

        regstr.append(TEXT("HTTP.*"));
        regstr.append(TO_STRING(responseCode));

        TSTRING result = FindRegex(subject, regstr);
        if (result != TEXT(""))
        {
            size_t offset = subject.find(result);
            if (offset == std::string::npos)
                return FALSE;

            size_t offsetendofline = subject.find(TEXT("\r\n"), offset);
            if (offsetendofline == std::string::npos)
                return FALSE;

            size_t linelength = offsetendofline - offset;
            if (linelength == std::string::npos)
                return FALSE;

            WCTNCPY((TCHAR*)lpBuffer,
                    subject.c_str() + offset + result.length() + 1,
                    linelength - result.length() - 1);

            ((TCHAR*)lpBuffer)[linelength - result.length() - 1] = TEXT('\0');
            return TRUE;
        }
        else
        {
            TSTRING retStr = StatusCodeMap[responseCode];
            if (retStr == TEXT(""))
                return FALSE;

            const TCHAR *cstr = retStr.c_str();
            WCTCPY((TCHAR*)lpBuffer, cstr);
        }
        return TRUE;
    }


    if (dwInfoLevel == WINHTTP_QUERY_RAW_HEADERS)
    {
        size_t length;
        std::string header;
        TCHAR *wbuffer = static_cast<TCHAR*>(lpBuffer);

        request->GetHeaderStringMutex().lock();
        length = request->GetHeaderString().length();
        header.append(request->GetHeaderString());
        request->GetHeaderStringMutex().unlock();

        if (SizeCheck(lpBuffer, lpdwBufferLength, (length + 1) * sizeof(TCHAR)) == FALSE)
            return FALSE;

        if (!wbuffer)
            return FALSE;

        header = nullize_newlines(header);
        header.resize(header.size() + 1);
        length = header.size();

#ifdef UNICODE
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        std::wstring wc = conv.from_bytes(header);
        std::copy(wc.begin(), wc.end(), wbuffer);
        wbuffer[header.length()] = TEXT('\0');
#else
        strncpy(wbuffer, header.c_str(), length);
#endif
        if (wbuffer[length] != TEXT('\0'))
        {
            if (lpdwBufferLength)
            {
                if ((length + 1) < (*lpdwBufferLength / sizeof(TCHAR)))
                {
                    wbuffer[length] = 0;
                }
                else
                {
                    wbuffer[(*lpdwBufferLength / sizeof(TCHAR)) - 1] = 0;
                }
            }
            else
            {
                wbuffer[length] = 0;
            }
        }
        if (lpdwBufferLength)
            *lpdwBufferLength = (DWORD)length;
        return TRUE;
    }

    if (dwInfoLevel == WINHTTP_QUERY_RAW_HEADERS_CRLF)
    {
        std::lock_guard<std::mutex> lck(request->GetHeaderStringMutex());
        size_t length;
        TCHAR *wbuffer = static_cast<TCHAR*>(lpBuffer);

        length = request->GetHeaderString().length();

        if (SizeCheck(lpBuffer, lpdwBufferLength, (length + 1) * sizeof(TCHAR)) == FALSE)
            return FALSE;

        if (!wbuffer)
            return FALSE;

#ifdef UNICODE
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        std::wstring wc = conv.from_bytes(request->GetHeaderString());
        std::copy(wc.begin(), wc.end(), wbuffer);
        wbuffer[request->GetHeaderString().length()] = TEXT('\0');

#else
        strncpy(wbuffer, request->GetHeaderString().c_str(), length);
#endif
        if (lpdwBufferLength)
        {
            if ((length + 1) < (*lpdwBufferLength / sizeof(TCHAR)))
            {
                wbuffer[length] = 0;
            }
            else
            {
                wbuffer[(*lpdwBufferLength / sizeof(TCHAR)) - 1] = 0;
            }
        }
        else
        {
            wbuffer[length] = 0;
        }
        if (lpdwBufferLength)
            *lpdwBufferLength = (DWORD)(length * sizeof(TCHAR));
        return TRUE;
    }


    return FALSE;
}

DWORD ConvertSecurityProtocol(DWORD offered)
{
    DWORD curlOffered = CURL_SSLVERSION_DEFAULT;

    if (offered & WINHTTP_FLAG_SECURE_PROTOCOL_SSL2)
        curlOffered = CURL_SSLVERSION_SSLv2;

    if (offered & WINHTTP_FLAG_SECURE_PROTOCOL_SSL3)
        curlOffered = CURL_SSLVERSION_SSLv3;

    if (offered & WINHTTP_FLAG_SECURE_PROTOCOL_TLS1) {
        curlOffered = CURL_SSLVERSION_TLSv1;
        curlOffered |= CURL_SSLVERSION_MAX_TLSv1_0;
    }

    if (offered & WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1) {
        curlOffered = CURL_SSLVERSION_TLSv1;
        curlOffered &= ~0xFFFF;
        curlOffered |= CURL_SSLVERSION_MAX_TLSv1_1;
    }

    if (offered & WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2) {
        curlOffered = CURL_SSLVERSION_TLSv1;
        curlOffered &= ~0xFFFF;
        curlOffered |= CURL_SSLVERSION_MAX_TLSv1_2;
    }
    return curlOffered;
}

BOOLAPI WinHttpSetOption(
    HINTERNET hInternet,
    DWORD     dwOption,
    LPVOID    lpBuffer,
    DWORD     dwBufferLength
)
{
    WinHttpBase *base = static_cast<WinHttpBase *>(hInternet);

    TRACE("%-35s:%-8d:%-16p dwOption:%lu\n", __func__, __LINE__, (void*)base, dwOption);

    if (dwOption == WINHTTP_OPTION_MAX_CONNS_PER_SERVER)
    {
        WinHttpSessionImp *session;
        WinHttpRequestImp *request;

        if (dwBufferLength != sizeof(DWORD))
            return FALSE;

        if ((session = dynamic_cast<WinHttpSessionImp *>(base)))
        {
            if (!lpBuffer)
                return FALSE;

            if (!session->SetMaxConnections(static_cast<DWORD*>(lpBuffer)))
                return FALSE;
            return TRUE;
        }
        else if ((request = dynamic_cast<WinHttpRequestImp *>(base)))
        {
            if (!lpBuffer)
                return FALSE;

            if (!request->SetMaxConnections(static_cast<DWORD*>(lpBuffer)))
                return FALSE;
            return TRUE;
        }
        else
            return FALSE;

    }
    else if (dwOption == WINHTTP_OPTION_CONTEXT_VALUE)
    {
        WinHttpConnectImp *connect;
        WinHttpSessionImp *session;
        WinHttpRequestImp *request;

        if (dwBufferLength != sizeof(void*))
            return FALSE;

        if ((connect = dynamic_cast<WinHttpConnectImp *>(base)))
        {
            if (!lpBuffer)
                return FALSE;

            if (!connect->SetUserData(static_cast<void**>(lpBuffer)))
                return FALSE;
            return TRUE;
        }
        else if ((session = dynamic_cast<WinHttpSessionImp *>(base)))
        {
            if (!lpBuffer)
                return FALSE;

            if (!session->SetUserData(static_cast<void**>(lpBuffer)))
                return FALSE;
            return TRUE;
        }
        else if ((request = dynamic_cast<WinHttpRequestImp *>(base)))
        {
            if (!lpBuffer)
                return FALSE;

            if (!request->SetUserData(static_cast<void**>(lpBuffer)))
                return FALSE;
            return TRUE;
        }
        else
            return FALSE;
    }
    else if (dwOption == WINHTTP_OPTION_SECURE_PROTOCOLS)
    {
        WinHttpSessionImp *session;
        WinHttpRequestImp *request;

        if (dwBufferLength != sizeof(DWORD))
            return FALSE;

        if ((session = dynamic_cast<WinHttpSessionImp *>(base)))
        {
            if (!lpBuffer)
                return FALSE;

            DWORD curlOffered = ConvertSecurityProtocol(*static_cast<DWORD*>(lpBuffer));
            if (curlOffered  && !session->SetSecureProtocol(&curlOffered))
                return FALSE;
            return TRUE;
        }
        else if ((request = dynamic_cast<WinHttpRequestImp *>(base)))
        {
            if (!lpBuffer)
                return FALSE;

            DWORD curlOffered = ConvertSecurityProtocol(*static_cast<DWORD*>(lpBuffer));
            if (curlOffered && !request->SetSecureProtocol(&curlOffered))
                return FALSE;
            return TRUE;
        }
        else
            return FALSE;
    }
    else if (dwOption == WINHTTP_OPTION_SECURITY_FLAGS)
    {
        WinHttpRequestImp *request;

        if (dwBufferLength != sizeof(DWORD))
            return FALSE;

        if ((request = dynamic_cast<WinHttpRequestImp *>(base)))
        {
            if (!lpBuffer)
                return FALSE;

            DWORD value = *static_cast<DWORD*>(lpBuffer);
            if (value == (SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE))
            {
                request->VerifyPeer() = 0L;
                request->VerifyHost() = 0L;
            }
            else if (!value)
            {
                request->VerifyPeer() = 1L;
                request->VerifyHost() = 2L;
            }
            else
                return FALSE;

            return TRUE;
        }
        else
            return FALSE;
    }

    return FALSE;
}

WINHTTPAPI
WINHTTP_STATUS_CALLBACK
WINAPI
WinHttpSetStatusCallback
(
    HINTERNET hInternet,
    WINHTTP_STATUS_CALLBACK lpfnInternetCallback,
    DWORD dwNotificationFlags,
    DWORD_PTR dwReserved
)
{
    WinHttpSessionImp *session = static_cast<WinHttpSessionImp *>(hInternet);
    WINHTTP_STATUS_CALLBACK oldcb;
    DWORD olddwNotificationFlags;

    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)session);

    if (hInternet == NULL)
        return WINHTTP_INVALID_STATUS_CALLBACK;

    oldcb = session->GetCallback(&olddwNotificationFlags);
    session->SetCallback(lpfnInternetCallback, dwNotificationFlags);
    return oldcb;
}

BOOLAPI
WinHttpQueryOption
(
    HINTERNET hInternet,
    DWORD dwOption,
    LPVOID lpBuffer,
    LPDWORD lpdwBufferLength
)
{
    WinHttpBase *base = static_cast<WinHttpBase *>(hInternet);
    WinHttpSessionImp *session;

    TRACE("%-35s:%-8d:%-16p\n", __func__, __LINE__, (void*)base);


    if (WINHTTP_OPTION_CONNECT_TIMEOUT == dwOption)
    {
        if (SizeCheck(lpBuffer, lpdwBufferLength, sizeof(DWORD)) == FALSE)
            return FALSE;

        if (!lpBuffer)
            return FALSE;

        session = GetImp(base);
        if (!session)
            return FALSE;

        *static_cast<DWORD *>(lpBuffer) = session->GetTimeout();
    }
    if (WINHTTP_OPTION_CALLBACK == dwOption)
    {
        if (SizeCheck(lpBuffer, lpdwBufferLength, sizeof(LPVOID)) == FALSE)
            return FALSE;

        if (!lpBuffer)
            return FALSE;

        session = GetImp(base);
        if (!session)
            return FALSE;

        DWORD dwNotificationFlags;
        WINHTTP_STATUS_CALLBACK cb = session->GetCallback(&dwNotificationFlags);
        *static_cast<WINHTTP_STATUS_CALLBACK *>(lpBuffer) = cb;
    }
    else if (WINHTTP_OPTION_URL == dwOption)
    {
        WinHttpRequestImp *request;

        if (!(request = dynamic_cast<WinHttpRequestImp *>(base)))
            return FALSE;

        char *url = NULL;
        curl_easy_getinfo(request->GetCurl(), CURLINFO_EFFECTIVE_URL, &url);
        if (!url)
            return FALSE;

        if (SizeCheck(lpBuffer, lpdwBufferLength, (strlen(url) + 1) * sizeof(TCHAR)) == FALSE)
            return FALSE;

        TCHAR *wbuffer = static_cast<TCHAR*>(lpBuffer);
        size_t length = strlen(url);
#ifdef UNICODE
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        std::wstring wc = conv.from_bytes(std::string(url));
        std::copy(wc.begin(), wc.end(), wbuffer);
        wbuffer[strlen(url)] = TEXT('\0');
#else
        strncpy(wbuffer, url, length);
#endif
        if (wbuffer[length] != TEXT('\0'))
        {
            if (lpdwBufferLength)
            {
                if ((length + 1) < (*lpdwBufferLength / sizeof(TCHAR)))
                {
                    wbuffer[length] = 0;
                }
                else
                {
                    wbuffer[(*lpdwBufferLength / sizeof(TCHAR)) - 1] = 0;
                }
            }
            else
            {
                wbuffer[length] = 0;
            }
        }
        if (lpdwBufferLength)
            *lpdwBufferLength = (DWORD)length;
    }
    else if (WINHTTP_OPTION_HTTP_VERSION == dwOption)
    {
        WinHttpRequestImp *request;

        if (!(request = dynamic_cast<WinHttpRequestImp *>(base)))
            return FALSE;

        if (SizeCheck(lpBuffer, lpdwBufferLength, sizeof(HTTP_VERSION_INFO)) == FALSE)
            return FALSE;

        if (!lpBuffer)
            return FALSE;

        long curlversion;
        CURLcode rc;

        rc = curl_easy_getinfo(request->GetCurl(), CURLINFO_HTTP_VERSION, &curlversion);
        if (rc != CURLE_OK)
            return FALSE;

        HTTP_VERSION_INFO version;

        if (curlversion == CURL_HTTP_VERSION_1_0)
        {
            version.dwMinorVersion = 0;
            version.dwMajorVersion = 1;
        }
        else if (curlversion == CURL_HTTP_VERSION_1_1)
        {
            version.dwMinorVersion = 1;
            version.dwMajorVersion = 1;
        }
        else if (curlversion == CURL_HTTP_VERSION_2_0)
        {
            version.dwMinorVersion = 1;
            version.dwMajorVersion = 1;
        }
        else
            return FALSE;

        memcpy(lpBuffer, &version, sizeof(version));
        if (lpdwBufferLength)
            *lpdwBufferLength = (DWORD)sizeof(version);
    }

    return TRUE;
}

#ifdef CURL_SUPPORTS_URL_API
BOOLAPI WinHttpCrackUrl(
    LPCTSTR          pwszUrl,
    DWORD            dwUrlLength,
    DWORD            dwFlags,
    LPURL_COMPONENTS lpUrlComponents
)
{
    DWORD urlLen;

    if (!pwszUrl || !lpUrlComponents)
        return FALSE;

    if (!lpUrlComponents->dwStructSize)
        return FALSE;

    if (lpUrlComponents->dwStructSize != sizeof(*lpUrlComponents))
        return FALSE;

    if (dwUrlLength == 0)
        urlLen = WCTLEN(pwszUrl);
    else
        urlLen = dwUrlLength;

    std::string urlstr;
    ConvertCstrAssign(pwszUrl, urlLen, urlstr);

    CURLUcode rc;
    CURLU *url = curl_url();
    rc = curl_url_set(url, CURLUPART_URL, urlstr.c_str(), 0);
    if (rc)
        return FALSE;

    char *host;
    rc = curl_url_get(url, CURLUPART_HOST, &host, 0);
    if (!rc) {
        size_t pos = urlstr.find(host);
        if (pos != std::string::npos)
        {
            if (lpUrlComponents->dwHostNameLength != (DWORD)-1)
            {
                if (lpUrlComponents->dwHostNameLength >= (strlen(host) + 1)) {
                    WCTNCPY(lpUrlComponents->lpszHostName, (TCHAR*)pwszUrl + pos, strlen(host));
                    lpUrlComponents->lpszHostName[strlen(host)] = TEXT('\0');
                    lpUrlComponents->dwHostNameLength = strlen(host);
                }
            }
            else
            {
                lpUrlComponents->lpszHostName = const_cast<TCHAR*>(pwszUrl) + pos;
                lpUrlComponents->dwHostNameLength = strlen(host);
            }
        }
        curl_free(host);
    }

    char *scheme;
    rc = curl_url_get(url, CURLUPART_SCHEME, &scheme, 0);
    if (!rc) {
        size_t pos = urlstr.find(scheme);
        if (pos != std::string::npos)
        {
            if (lpUrlComponents->dwSchemeLength != (DWORD)-1)
            {
                if (lpUrlComponents->dwSchemeLength >= (strlen(scheme) + 1)) {
                    WCTNCPY(lpUrlComponents->lpszScheme, (TCHAR*)pwszUrl + pos, strlen(scheme));
                    lpUrlComponents->lpszScheme[strlen(scheme)] = TEXT('\0');
                    lpUrlComponents->dwSchemeLength = strlen(scheme);
                }
            }
            else
            {
                lpUrlComponents->lpszScheme = const_cast<TCHAR*>(pwszUrl) + pos;
                lpUrlComponents->dwSchemeLength = strlen(scheme);
            }

            if (strcmp(scheme, "http") == 0) {
                lpUrlComponents->nPort = 80;
                lpUrlComponents->nScheme = INTERNET_SCHEME_HTTP;
            }
            else if (strcmp(scheme, "https") == 0)
            {
                lpUrlComponents->nPort = 443;
                lpUrlComponents->nScheme = INTERNET_SCHEME_HTTPS;
            }
        }
        curl_free(scheme);
    }
    char *path;
    rc = curl_url_get(url, CURLUPART_PATH, &path, 0);
    if (!rc) {
        size_t pos = urlstr.find(path);
        if (pos != std::string::npos)
        {
            if (lpUrlComponents->dwUrlPathLength != (DWORD)-1)
            {
                if (lpUrlComponents->dwUrlPathLength >= (strlen(path) + 1)) {
                    WCTNCPY(lpUrlComponents->lpszUrlPath, (TCHAR*)pwszUrl + pos, strlen(path));
                    lpUrlComponents->lpszUrlPath[strlen(path)] = TEXT('\0');
                    lpUrlComponents->dwUrlPathLength = strlen(path);
                }
            }
            else
            {
                if (strcmp(path, "/") == 0)
                {
                    lpUrlComponents->lpszUrlPath = (LPWSTR)TEXT("");
                    lpUrlComponents->dwUrlPathLength = 0;
                }
                else
                {
                    lpUrlComponents->lpszUrlPath = const_cast<TCHAR*>(pwszUrl) + pos;
                    lpUrlComponents->dwUrlPathLength = strlen(path);
                }
            }
        }
        curl_free(path);
    }
    char *query;
    rc = curl_url_get(url, CURLUPART_QUERY, &query, 0);
    if (!rc) {
        size_t pos = urlstr.find(query);
        if (pos != std::string::npos)
        {
            if (lpUrlComponents->dwExtraInfoLength != (DWORD)-1)
            {
                if (lpUrlComponents->dwExtraInfoLength >= (strlen(query) + 1)) {
                    WCTNCPY(lpUrlComponents->lpszExtraInfo, (TCHAR*)pwszUrl + pos - 1, strlen(query));
                    lpUrlComponents->lpszExtraInfo[strlen(query)] = TEXT('\0');
                    lpUrlComponents->dwExtraInfoLength = strlen(query);
                }
            }
            else
            {
                lpUrlComponents->lpszExtraInfo = const_cast<TCHAR*>(pwszUrl) + pos - 1;
                lpUrlComponents->dwExtraInfoLength = strlen(query) + 1;
            }
        }
        curl_free(query);
    }
    char *user;
    rc = curl_url_get(url, CURLUPART_USER, &user, 0);
    if (!rc) {
        size_t pos = urlstr.find(user);
        if (pos != std::string::npos)
        {
            if (lpUrlComponents->dwUserNameLength != (DWORD)-1)
            {
                if (lpUrlComponents->dwUserNameLength >= (strlen(user) + 1)) {
                    WCTNCPY(lpUrlComponents->lpszUserName, (TCHAR*)pwszUrl + pos - 1, strlen(user));
                    lpUrlComponents->lpszUserName[strlen(user)] = TEXT('\0');
                    lpUrlComponents->dwUserNameLength = strlen(user);
                }
            }
            else
            {
                lpUrlComponents->lpszUserName = const_cast<TCHAR*>(pwszUrl) + pos - 1;
                lpUrlComponents->dwUserNameLength = strlen(user);
            }
        }
        curl_free(user);
    }
    char *pw;
    rc = curl_url_get(url, CURLUPART_PASSWORD, &pw, 0);
    if (!rc) {
        size_t pos = urlstr.find(pw);
        if (pos != std::string::npos)
        {
            if (lpUrlComponents->dwPasswordLength != (DWORD)-1)
            {
                if (lpUrlComponents->dwPasswordLength >= (strlen(pw) + 1)) {
                    WCTNCPY(lpUrlComponents->lpszPassword, (TCHAR*)pwszUrl + pos - 1, strlen(pw));
                    lpUrlComponents->lpszPassword[strlen(pw)] = TEXT('\0');
                    lpUrlComponents->dwPasswordLength = strlen(pw);
                }
            }
            else
            {
                lpUrlComponents->lpszPassword = const_cast<TCHAR*>(pwszUrl) + pos - 1;
                lpUrlComponents->dwPasswordLength = strlen(pw);
            }
        }
        curl_free(pw);
    }
    curl_url_cleanup(url);

    return TRUE;
}

#endif


BOOLAPI WinHttpWriteData
(
    HINTERNET hRequest,
    LPCVOID lpBuffer,
    DWORD dwNumberOfBytesToWrite,
    LPDWORD lpdwNumberOfBytesWritten
)
{
    WinHttpRequestImp *request = static_cast<WinHttpRequestImp *>(hRequest);
    if (!request)
        return FALSE;

    std::shared_ptr<WinHttpRequestImp> srequest = request->shared_from_this();
    if (!srequest)
        return FALSE;

    if (request->GetClosing())
    {
        TRACE("%-35s:%-8d:%-16p \n", __func__, __LINE__, (void*)request);
        return FALSE;
    }

    TRACE("%-35s:%-8d:%-16p dwNumberOfBytesToWrite:%lu\n", __func__, __LINE__, (void*)request, dwNumberOfBytesToWrite);
    if (request->GetAsync())
    {
        {
            std::lock_guard<std::mutex> lck(request->GetReadDataEventMtx());
            request->QueueWriteRequest(lpBuffer, dwNumberOfBytesToWrite);
            request->GetReadDataEventCounter()++;
        }

        ComContainer::GetInstance().ResumeTransfer(request->GetCurl(), CURLPAUSE_CONT);
    }
    else
    {
        request->AppendReadData(lpBuffer, dwNumberOfBytesToWrite);
        {
            std::lock_guard<std::mutex> lck(request->GetReadDataEventMtx());
            request->GetReadDataEventCounter()++;
        }
    }

    if (lpdwNumberOfBytesWritten)
        *lpdwNumberOfBytesWritten = dwNumberOfBytesToWrite;

    return TRUE;
}

