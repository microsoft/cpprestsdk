typedef void VOID;
typedef void *LPVOID;
typedef unsigned long       DWORD;
typedef const void* LPCVOID;
typedef long LONG;
typedef unsigned char BYTE;
#define  __int3264 long int
typedef unsigned __int3264 ULONG_PTR;
typedef ULONG_PTR DWORD_PTR;
typedef DWORD *PDWORD;
typedef DWORD *LPDWORD;
typedef char *LPSTR;
typedef const char *LPCSTR;

#ifdef UNICODE
typedef wchar_t TCHAR;
typedef const wchar_t* LPCTSTR;
typedef wchar_t* LPTSTR, *PTSTR;
typedef const wchar_t* LPCTSTR, *PCTSTR;
#else
typedef char TCHAR;
typedef const char* LPCTSTR;
typedef char* LPTSTR, *PTSTR;    
typedef const char* LPCTSTR, *PCTSTR;    
#endif
 
typedef LPVOID HINTERNET;
typedef bool BOOL;

typedef VOID (* WINHTTP_STATUS_CALLBACK)(
    HINTERNET hInternet,
    DWORD_PTR dwContext,
    DWORD dwInternetStatus,
    LPVOID lpvStatusInformation,
    DWORD dwStatusInformationLength
);

enum
{
    API_RECEIVE_RESPONSE = 1,
    API_QUERY_DATA_AVAILABLE,
    API_READ_DATA,
    API_WRITE_DATA,
    API_SEND_REQUEST,
};

typedef struct
{
    DWORD_PTR dwResult;
    DWORD dwError;
}
WINHTTP_ASYNC_RESULT, * LPWINHTTP_ASYNC_RESULT;

typedef struct
{
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
}
HTTP_VERSION_INFO;

#define WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH 0

enum
{
    WINHTTP_QUERY_RAW_HEADERS,
    WINHTTP_QUERY_STATUS_CODE,
    WINHTTP_QUERY_VERSION,
    WINHTTP_QUERY_RAW_HEADERS_CRLF,
    WINHTTP_QUERY_STATUS_TEXT,
    WINHTTP_QUERY_FLAG_NUMBER = 0x80000000,
};

enum
{
    WINHTTP_AUTH_SCHEME_NEGOTIATE,
    WINHTTP_AUTH_SCHEME_NTLM,
    WINHTTP_AUTH_SCHEME_PASSPORT,
    WINHTTP_AUTH_SCHEME_DIGEST,
    WINHTTP_AUTH_SCHEME_BASIC,
    WINHTTP_AUTH_TARGET_PROXY,
    WINHTTP_AUTH_TARGET_SERVER,
};

#define WINHTTP_INVALID_STATUS_CALLBACK (WINHTTP_STATUS_CALLBACK)-1

#define WINHTTP_NO_REFERER              NULL
#define WINHTTP_DEFAULT_ACCEPT_TYPES    NULL
#define WINHTTP_NO_ADDITIONAL_HEADERS   NULL
#define WINHTTP_NO_REQUEST_DATA         NULL
#define WINHTTP_NO_PROXY_NAME           NULL
#define WINHTTP_NO_PROXY_BYPASS         NULL
#define WINHTTP_NO_HEADER_INDEX         NULL
#define WINHTTP_HEADER_NAME_BY_INDEX    NULL
#define WINHTTP_NO_OUTPUT_BUFFER        NULL

#define WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH 0
#define WINHTTP_ADDREQ_FLAG_ADD 0
#define WINHTTP_ENABLE_SSL_REVOCATION 0

enum
{
    WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2  = 0x00080000,
    WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1  = 0x00100000,
    WINHTTP_FLAG_SECURE_PROTOCOL_TLS1  = 0x00200000,
    WINHTTP_FLAG_SECURE_PROTOCOL_SSL3  = 0x00400000,
    WINHTTP_FLAG_SECURE_PROTOCOL_SSL2  = 0x00040000,
    WINHTTP_FLAG_SECURE  = 0x00800000,
    WINHTTP_FLAG_ASYNC   = 0x10000000,
    WINHTTP_FLAG_REFRESH = 0x00000100,
    WINHTTP_FLAG_ESCAPE_DISABLE = 0x20000000,
};

enum
{
    WINHTTP_CALLBACK_STATUS_READ_COMPLETE = (1 << 0),
    WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE = (1 << 1),
    WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE = (1 << 2),
    WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE = (1 << 3),
    WINHTTP_CALLBACK_STATUS_REQUEST_ERROR = (1 << 4),
    WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING = (1 << 5),
    WINHTTP_CALLBACK_STATUS_SECURE_FAILURE = (1 << 6),
    WINHTTP_CALLBACK_STATUS_SENDING_REQUEST = (1 << 7),
    WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE = (1 << 8),
    WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED = (1 << 9),
    WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT = (1 << 10),
    WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED = (1 << 11),
    WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA = (1 << 12),
    WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID = (1 << 13),
    WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID = (1 << 14),
    WINHTTP_CALLBACK_STATUS_FLAG_SECURITY_CHANNEL_ERROR = (1 << 15),
    WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE = (1 << 16),
    WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED = (1 << 17),
    WINHTTP_CALLBACK_STATUS_REQUEST_SENT = (1 << 18),
    WINHTTP_CALLBACK_STATUS_REDIRECT = (1 << 19),
    WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS = (1 << 24),
    WINHTTP_CALLBACK_FLAG_HANDLES = (1 << 25),
    WINHTTP_CALLBACK_FLAG_SECURE_FAILURE = (1 << 26),
    WINHTTP_CALLBACK_FLAG_SEND_REQUEST = (1 << 27),
};

enum
{
    WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS = (1 << 0),
};

enum
{
    WINHTTP_OPTION_CONTEXT_VALUE = 1,
    WINHTTP_OPTION_CONNECT_TIMEOUT,
    WINHTTP_OPTION_CALLBACK,
    WINHTTP_OPTION_URL,
    WINHTTP_OPTION_HTTP_VERSION,
    WINHTTP_OPTION_MAX_CONNS_PER_SERVER,
    WINHTTP_OPTION_SECURE_PROTOCOLS,
    WINHTTP_OPTION_PROXY,
    WINHTTP_OPTION_AUTOLOGON_POLICY,
    WINHTTP_OPTION_ENABLE_FEATURE,
    WINHTTP_OPTION_SECURITY_FLAGS,
};

enum
{
    SECURITY_FLAG_IGNORE_UNKNOWN_CA,
    SECURITY_FLAG_IGNORE_CERT_DATE_INVALID,
    SECURITY_FLAG_IGNORE_CERT_CN_INVALID,
    SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE,
};
enum
{
    ERROR_WINHTTP_OPERATION_CANCELLED = 12017,
};

enum
{
    WINHTTP_ACCESS_TYPE_NAMED_PROXY = 1,
    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY =2,
    WINHTTP_ACCESS_TYPE_NO_PROXY = 3,
    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY = 4,
};

typedef unsigned int INTERNET_PORT;

#define INTERNET_DEFAULT_HTTP_PORT 80


BOOL WinHttpCloseHandle(
    HINTERNET hInternet
);


BOOL
WinHttpReceiveResponse
(
    HINTERNET hRequest,
    LPVOID lpReserved
);

BOOL
WinHttpSetTimeouts
(
    HINTERNET    hInternet,           // Session/Request handle.
    int          nResolveTimeout,
    int          nConnectTimeout,
    int          nSendTimeout,
    int          nReceiveTimeout
);


WINHTTP_STATUS_CALLBACK
WinHttpSetStatusCallback
(
    HINTERNET hInternet,
    WINHTTP_STATUS_CALLBACK lpfnInternetCallback,
    DWORD dwNotificationFlags,
    DWORD_PTR dwReserved
);

BOOL WinHttpSetOption(
    HINTERNET hInternet,
    DWORD     dwOption,
    LPVOID    lpBuffer,
    DWORD     dwBufferLength
);

BOOL WinHttpSendRequest
(
    HINTERNET hRequest,
    LPCTSTR lpszHeaders,
    DWORD dwHeadersLength,
    LPVOID lpOptional,
    DWORD dwOptionalLength,
    DWORD dwTotalLength,
    DWORD_PTR dwContext
);

BOOL
WinHttpReadData
(
    HINTERNET hRequest,
    LPVOID lpBuffer,
    DWORD dwNumberOfBytesToRead,
    LPDWORD lpdwNumberOfBytesRead
);

BOOL WinHttpQueryHeaders(
    HINTERNET   hRequest,
    DWORD       dwInfoLevel,
    LPCTSTR     pwszName,
    LPVOID      lpBuffer,
    LPDWORD lpdwBufferLength,
    LPDWORD lpdwIndex
);


BOOL
WinHttpQueryDataAvailable
(
    HINTERNET hRequest,
    LPDWORD lpdwNumberOfBytesAvailable
);


HINTERNET WinHttpOpenRequest
(
    HINTERNET hConnect,
    LPCTSTR pwszVerb,
    LPCTSTR pwszObjectName,
    LPCTSTR pwszVersion,
    LPCTSTR pwszReferrer,
    LPCTSTR * ppwszAcceptTypes,
    DWORD dwFlags
);

HINTERNET WinHttpOpen
(
    LPCTSTR pszAgentW,
    DWORD dwAccessType,
    LPCTSTR pszProxyW,
    LPCTSTR pszProxyBypassW,
    DWORD dwFlags
);

HINTERNET WinHttpConnect
(
    HINTERNET hSession,
    LPCTSTR pswzServerName,
    INTERNET_PORT nServerPort,
    DWORD dwReserved
);

BOOL
WinHttpAddRequestHeaders
(
    HINTERNET hRequest,
    LPCTSTR lpszHeaders,
    DWORD dwHeadersLength,
    DWORD dwModifiers
);

BOOL
WinHttpQueryOption
(
    HINTERNET hInternet,
    DWORD dwOption,
    LPVOID lpBuffer,
    LPDWORD lpdwBufferLength
);

BOOL WinHttpWriteData(
    HINTERNET hRequest,
    LPCVOID      lpBuffer,
    DWORD     dwNumberOfBytesToWrite,
    LPDWORD  lpdwNumberOfBytesWritten
);

#define ARRAYSIZE(n) sizeof(n)/sizeof(n[0])

#define GetLastError() errno
#define CALLBACK


typedef struct {
    DWORD  dwAccessType;
    LPTSTR lpszProxy;
    LPTSTR lpszProxyBypass;
} WINHTTP_PROXY_INFO, *LPWINHTTP_PROXY_INFO;

typedef struct {
    BOOL   fAutoDetect;
    LPTSTR lpszAutoConfigUrl;
    LPTSTR lpszProxy;
    LPTSTR lpszProxyBypass;
} WINHTTP_CURRENT_USER_IE_PROXY_CONFIG;

typedef struct {
    DWORD   dwFlags;
    DWORD   dwAutoDetectFlags;
    LPCTSTR lpszAutoConfigUrl;
    LPVOID  lpvReserved;
    DWORD   dwReserved;
    BOOL    fAutoLogonIfChallenged;
} WINHTTP_AUTOPROXY_OPTIONS;

BOOL WinHttpGetProxyForUrl(
    HINTERNET                 hSession,
    LPCTSTR                   lpcwszUrl,
    WINHTTP_AUTOPROXY_OPTIONS *pAutoProxyOptions,
    WINHTTP_PROXY_INFO       *pProxyInfo
);


BOOL WinHttpQueryAuthSchemes(
    HINTERNET hRequest,
    LPDWORD  lpdwSupportedSchemes,
    LPDWORD  lpdwFirstScheme,
    LPDWORD  pdwAuthTarget
);

BOOL WinHttpSetCredentials(
    HINTERNET hRequest,
    DWORD     AuthTargets,
    DWORD     AuthScheme,
    LPCTSTR   pwszUserName,
    LPCTSTR   pwszPassword,
    LPVOID    pAuthParams
);

enum
{
    WINHTTP_AUTOPROXY_AUTO_DETECT,
    WINHTTP_AUTO_DETECT_TYPE_DHCP,
    WINHTTP_AUTO_DETECT_TYPE_DNS_A,
    WINHTTP_AUTOPROXY_CONFIG_URL,
};


#define GlobalFree free

#define FALSE 0
#define TRUE 1

#define INTERNET_DEFAULT_HTTPS_PORT 443
#define S_OK 0

#define ERROR_INSUFFICIENT_BUFFER 	ENOMEM
#define ERROR_WINHTTP_RESEND_REQUEST 	EBUSY
#define ERROR_SUCCESS 			0
#define ERROR_OPERATION_ABORTED		EINVAL
#define ERROR_NOT_ENOUGH_MEMORY		ENOMEM
#define ERROR_WINHTTP_TIMEOUT		ETIMEDOUT
#define ERROR_INVALID_PARAMETER		EINVAL

#include <memory>

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

#define BOOLAPI BOOL
#define SetLastError(val) errno = val
#define WINHTTPAPI
#define WINAPI

typedef enum {
	INTERNET_SCHEME_HTTP = 1,
	INTERNET_SCHEME_HTTPS,
} INTERNET_SCHEME;

typedef struct {
  DWORD           dwStructSize;
  LPTSTR          lpszScheme;
  DWORD           dwSchemeLength;
  INTERNET_SCHEME nScheme;
  LPTSTR          lpszHostName;
  DWORD           dwHostNameLength;
  INTERNET_PORT   nPort;
  LPTSTR          lpszUserName;
  DWORD           dwUserNameLength;
  LPTSTR          lpszPassword;
  DWORD           dwPasswordLength;
  LPTSTR          lpszUrlPath;
  DWORD           dwUrlPathLength;
  LPTSTR          lpszExtraInfo;
  DWORD           dwExtraInfoLength;
} URL_COMPONENTS, *LPURL_COMPONENTS;
