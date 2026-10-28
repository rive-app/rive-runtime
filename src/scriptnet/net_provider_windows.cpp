/*
 * Copyright 2026 Rive
 */

#if defined(WITH_RIVE_SCRIPTNET) && defined(_WIN32)

#include "rive/scriptnet/net.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

// Script requests on Windows, over WinHTTP in asynchronous mode. As on Apple,
// the session has no ambient authority: cookies are neither kept nor sent,
// automatic authentication is off so neither the user's Windows identity
// (NTLM, Negotiate) nor a stored credential answers a challenge, and WinHTTP
// has no cache.
//
// Redirects are followed here, not by WinHTTP: its own would resend every
// header, Authorization included, and the callback it announces a hop
// through can neither edit them nor reliably stop the hop. Each 3xx is
// instead carried to the next request by NetPolicy::redirect, which rewrites
// it the way the Fetch standard does and runs the policy, and the hop goes
// out as a request of its own.
//
// The address a hop connects to is checked here, from the callback WinHTTP
// makes as it connects, so a public name that resolves to a local or
// private address is refused before the request is sent (see NetPolicy).
// WinHTTP still completes the connection, and for https: its handshake,
// before the close takes effect, as a browser's private network access
// check connects before it blocks; no request bytes follow. Through a
// proxy the address is the proxy's, which WinHTTP announces by resolving
// the proxy's name instead of the host's; what the proxy then reaches is
// the proxy's to judge.
//
// WinHTTP's status callback runs on its own threads and, for the request
// whose call is in progress, may re-enter on the calling thread. Every
// outcome is therefore reported from WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING,
// which WinHTTP guarantees is the last callback for a handle and never
// overlaps another. Ending an exchange, whatever the reason (policy, size,
// timeout, the script), is recording why and closing its handle; a handle is
// never closed while a frame on any thread is inside a WinHTTP call on it,
// which WinHTTP forbids. No WinHTTP call is made with a lock held: the call
// may deliver its completion before returning.

using namespace rive;
using namespace rive::scriptnet;

namespace
{
using Clock = std::chrono::steady_clock;

constexpr DWORD kReadChunkBytes = 64 * 1024;

std::wstring toWide(const std::string& utf8)
{
  if (utf8.empty())
  {
    return std::wstring();
  }
  int length = MultiByteToWideChar(
      CP_UTF8,
      0,
      utf8.data(),
      (int)utf8.size(),
      nullptr,
      0);
  if (length <= 0)
  {
    return std::wstring();
  }
  std::wstring wide((size_t)length, L'\0');
  MultiByteToWideChar(
      CP_UTF8,
      0,
      utf8.data(),
      (int)utf8.size(),
      wide.data(),
      length);
  return wide;
}

std::string toUtf8(const wchar_t* wide, size_t length)
{
  if (length == 0)
  {
    return std::string();
  }
  int size = WideCharToMultiByte(
      CP_UTF8,
      0,
      wide,
      (int)length,
      nullptr,
      0,
      nullptr,
      nullptr);
  if (size <= 0)
  {
    return std::string();
  }
  std::string utf8((size_t)size, '\0');
  WideCharToMultiByte(
      CP_UTF8,
      0,
      wide,
      (int)length,
      utf8.data(),
      size,
      nullptr,
      nullptr);
  return utf8;
}

// Header text, back to the bytes the server sent. WinHTTP decodes headers
// with the system's ANSI code page (byte 0x80 comes back as U+20AC on a
// Windows-1252 machine), so encoding with that code page undoes it, and a
// header carrying raw UTF-8, a Location say, reaches the policy and the
// script as the bytes it was.
std::string headerBytes(const wchar_t* text, size_t length)
{
  if (length == 0)
  {
    return std::string();
  }
  int size = WideCharToMultiByte(
      CP_ACP,
      0,
      text,
      (int)length,
      nullptr,
      0,
      nullptr,
      nullptr);
  if (size <= 0)
  {
    return std::string();
  }
  std::string bytes((size_t)size, '\0');
  WideCharToMultiByte(
      CP_ACP,
      0,
      text,
      (int)length,
      bytes.data(),
      size,
      nullptr,
      nullptr);
  return bytes;
}

// The system's text for a Win32 or WinHTTP error, or a generic line.
std::string errorMessage(DWORD code)
{
  wchar_t* text = nullptr;
  DWORD length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
      GetModuleHandleW(L"winhttp.dll"),
      code,
      0,
      reinterpret_cast<wchar_t*>(&text),
      0,
      nullptr);
  std::string message;
  if (text != nullptr)
  {
    message = toUtf8(text, length);
    LocalFree(text);
  }
  while (!message.empty() && (message.back() == '\r' ||
                              message.back() == '\n' || message.back() == ' '))
  {
    message.pop_back();
  }
  if (message.empty())
  {
    message = "request failed (error " + std::to_string(code) + ")";
  }
  return message;
}

// Trims a buffer WinHTTP filled to the string it holds.
void trimAtNul(std::wstring* value)
{
  value->resize(wcsnlen(value->data(), value->size()));
}

bool queryHeader(HINTERNET request, DWORD info, std::wstring* value)
{
  DWORD size = 0;
  WinHttpQueryHeaders(request, info, nullptr, nullptr, &size, nullptr);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
  {
    return false;
  }
  std::wstring buffer(size / sizeof(wchar_t), L'\0');
  if (!WinHttpQueryHeaders(
          request,
          info,
          nullptr,
          buffer.data(),
          &size,
          nullptr))
  {
    return false;
  }
  trimAtNul(&buffer);
  *value = std::move(buffer);
  return true;
}

bool queryNumber(HINTERNET request, DWORD info, DWORD* value)
{
  DWORD size = sizeof(DWORD);
  return WinHttpQueryHeaders(
             request,
             info | WINHTTP_QUERY_FLAG_NUMBER,
             nullptr,
             value,
             &size,
             nullptr) != FALSE;
}

// Splits WINHTTP_QUERY_RAW_HEADERS_CRLF, whose first line is the status
// line, into header lines. Order and duplicates are kept.
void parseHeaders(const std::wstring& raw, HttpHeaders* headers)
{
  size_t start = 0;
  bool statusLine = true;
  while (start < raw.size())
  {
    size_t end = raw.find(L"\r\n", start);
    if (end == std::wstring::npos)
    {
      end = raw.size();
    }
    const wchar_t* line = raw.data() + start;
    size_t length = end - start;
    start = end + 2;
    if (statusLine)
    {
      statusLine = false;
      continue;
    }
    const wchar_t* colon = std::find(line, line + length, L':');
    if (length == 0 || colon == line + length)
    {
      continue;
    }
    size_t valueStart = (size_t)(colon - line) + 1;
    while (valueStart < length &&
           (line[valueStart] == L' ' || line[valueStart] == L'\t'))
    {
      valueStart++;
    }
    size_t valueEnd = length;
    while (valueEnd > valueStart &&
           (line[valueEnd - 1] == L' ' || line[valueEnd - 1] == L'\t'))
    {
      valueEnd--;
    }
    headers->push_back(
        {headerBytes(line, (size_t)(colon - line)),
         headerBytes(line + valueStart, valueEnd - valueStart)});
  }
}

// HEAD answers and null body statuses carry no body, whatever their
// Content-Length says about the resource.
bool expectsBody(bool head, uint16_t status)
{
  if (head)
  {
    return false;
  }
  switch (status)
  {
    case 101:
    case 103:
    case 204:
    case 205:
    case 304:
      return false;
  }
  return true;
}

bool isRedirect(uint16_t status)
{
  return status == 301 || status == 302 || status == 303 || status == 307 ||
         status == 308;
}

const std::string* findHeader(const HttpHeaders& headers, const char* name)
{
  for (const HttpHeader& header : headers)
  {
    if (lowerAscii(header.name) == name)
    {
      return &header.value;
    }
  }
  return nullptr;
}

// The bytes of an address as WinHTTP's callbacks spell it: dotted decimal,
// or IPv6 in brackets, possibly with a scope.
bool parseAddress(const wchar_t* text, uint8_t* bytes, size_t* length)
{
  std::wstring address(text);
  if (!address.empty() && address.front() == L'[')
  {
    address.erase(0, 1);
  }
  size_t end = address.find_first_of(L"]%");
  if (end != std::wstring::npos)
  {
    address.resize(end);
  }
  if (address.find(L':') != std::wstring::npos)
  {
    in6_addr parsed;
    if (InetPtonW(AF_INET6, address.c_str(), &parsed) != 1)
    {
      return false;
    }
    std::memcpy(bytes, &parsed, 16);
    *length = 16;
    return true;
  }
  in_addr parsed;
  if (InetPtonW(AF_INET, address.c_str(), &parsed) != 1)
  {
    return false;
  }
  std::memcpy(bytes, &parsed, 4);
  *length = 4;
  return true;
}

// Host names as WinHTTP reports the one it resolves, case aside.
bool sameHost(const std::wstring& a, const std::wstring& b)
{
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(), [](wchar_t x, wchar_t y) {
           return towlower(x) == towlower(y);
         });
}

// Parses an http: or https: URL into `parts`, which point into `url`.
bool crackUrl(const std::wstring& url, URL_COMPONENTS* parts)
{
  *parts = URL_COMPONENTS{};
  parts->dwStructSize = sizeof(*parts);
  parts->dwSchemeLength = (DWORD)-1;
  parts->dwHostNameLength = (DWORD)-1;
  parts->dwUrlPathLength = (DWORD)-1;
  parts->dwExtraInfoLength = (DWORD)-1;
  return WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, parts) &&
         (parts->nScheme == INTERNET_SCHEME_HTTP ||
          parts->nScheme == INTERNET_SCHEME_HTTPS) &&
         parts->dwHostNameLength != 0;
}

class Request;

// What the provider and its requests share. Each side holds it, since a
// request's last callback can arrive after the provider is gone. The
// session lives here because closing it invalidates every request under
// it, pending ones included: whichever of the provider and the last request
// finishes second closes it.
struct Registry : public RefCnt<Registry>
{
  std::mutex mutex;
  HINTERNET session = nullptr;
  std::unordered_map<RequestId, rcp<Request>> requests;
  // Wakes the watchdog for a new deadline, and to stop.
  std::condition_variable wake;
  // The provider is gone; the last request out closes the session.
  bool stopping = false;

  // With the mutex held: the session, once the provider is gone and no
  // request remains, for the caller to close with the mutex released.
  HINTERNET takeSessionIfDone()
  {
    if (!stopping || !requests.empty() || session == nullptr)
    {
      return nullptr;
    }
    HINTERNET done = session;
    session = nullptr;
    return done;
  }
};

// One exchange: a request and the hops its redirects lead to, each a WinHTTP
// request of its own, one at a time. The registry holds it until the
// exchange ends; WinHTTP holds it as each handle's context until that
// handle's closing callback; and each callback holds it while running, so a
// close inside one is safe.
//
// The handle of the moment is closed exactly once, to end the exchange or
// to move to the next hop, and never while a frame on any thread is inside
// a WinHTTP call on it: such frames hold a CallGuard, and the last one out
// performs a close that was decided meanwhile.
class Request : public RefCnt<Request>
{
public:
  Request(
      rcp<Registry> registry,
      RequestId id,
      const HttpRequest& request,
      const NetLimits& limits)
      : m_registry(std::move(registry))
      , m_id(id)
      , m_hop(request)
      , m_deadline(Clock::now() + std::chrono::milliseconds(request.timeoutMs))
      , m_maxBodyBytes(limits.maxResponseBodyBytes)
      , m_allowLocalAddresses(limits.allowLocalAddresses)
  {}

  Clock::time_point deadline() const { return m_deadline; }

  bool closing()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_closing;
  }

  // Registered with the registry beforehand, so a stop or abort can already
  // reach it.
  void start(HINTERNET session)
  {
    m_session = session;
    sendHop();
  }

  // Any thread. Ends the exchange with `error`, which the closing callback
  // reports instead of the cancellation it causes. The first reason wins.
  void stop(NetError error) { settle(false, &error); }

  // Any thread. The script or the host gave up; the outcome is dropped.
  void abort() { settle(false, nullptr); }

  static void CALLBACK onStatus(
      HINTERNET handle,
      DWORD_PTR context,
      DWORD status,
      LPVOID info,
      DWORD length)
  {
    if (context == 0)
    {
      return;
    }
    rcp<Request> request = ref_rcp(reinterpret_cast<Request*>(context));
    if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)
    {
      request->onClosing();
      return;
    }
    // Nothing below matters once the handle is on its way out, and the
    // guard keeps it open under the calls made here.
    CallGuard guard(request.get());
    if (!guard.held())
    {
      return;
    }
    switch (status)
    {
      case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
        request->onResolving(static_cast<const wchar_t*>(info));
        break;
      case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
        request->onConnecting(static_cast<const wchar_t*>(info));
        break;
      case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
        if (!WinHttpReceiveResponse(handle, nullptr))
        {
          request->stop({NetErrorCode::Network, errorMessage(GetLastError())});
        }
        break;
      case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
        request->onHeaders(handle);
        break;
      case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
        request->onRead(handle, length);
        break;
      case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
        request->onError(static_cast<WINHTTP_ASYNC_RESULT*>(info)->dwError);
        break;
    }
  }

private:
  // Held by every frame inside a WinHTTP call on the handle of the moment,
  // on any thread. Not granted once that handle is to be closed; the last
  // holder out performs the close.
  class CallGuard
  {
  public:
    explicit CallGuard(Request* request) : m_request(request)
    {
      std::lock_guard<std::mutex> lock(m_request->m_mutex);
      m_held = !m_request->m_closing && !m_request->m_hopping;
      if (m_held)
      {
        m_request->m_calls++;
      }
    }
    ~CallGuard()
    {
      if (!m_held)
      {
        return;
      }
      HINTERNET toClose = nullptr;
      {
        std::lock_guard<std::mutex> lock(m_request->m_mutex);
        m_request->m_calls--;
        toClose = m_request->takeCloseLocked();
      }
      if (toClose != nullptr)
      {
        WinHttpCloseHandle(toClose);
      }
    }
    bool held() const { return m_held; }

  private:
    Request* m_request;
    bool m_held = false;
  };

  // With m_mutex held: the handle to close now, if the exchange has ended
  // or the next hop is due, the handle exists, no frame is inside a call on
  // it, and it has not been closed already.
  HINTERNET takeCloseLocked()
  {
    if (!(m_closing || m_hopping) || m_closeSent || m_request == nullptr ||
        m_calls != 0)
    {
      return nullptr;
    }
    m_closeSent = true;
    return m_request;
  }

  // Any thread. Ends the exchange once: with `error`, or as completed, or
  // as cancelled when neither. Later calls change nothing, so the first
  // reason recorded is the one reported. The handle is closed here unless
  // a frame is inside a WinHTTP call on it, or there is none at the
  // moment; then that frame, or the one opening the next, closes it.
  void settle(bool completed, const NetError* error)
  {
    HINTERNET toClose = nullptr;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_closing)
      {
        return;
      }
      m_closing = true;
      if (error != nullptr)
      {
        m_failed = true;
        m_error = *error;
      }
      else
      {
        m_completed = completed;
      }
      toClose = takeCloseLocked();
    }
    if (toClose != nullptr)
    {
      WinHttpCloseHandle(toClose);
    }
  }

  // Opens and sends m_hop as a WinHTTP request, the exchange's first or the
  // one a redirect led to. The guard keeps a stop or abort from closing the
  // handle under the calls made here; one decided meanwhile closes it on
  // the way out, and the closing callback reports it.
  void sendHop()
  {
    CallGuard guard(this);
    if (!guard.held())
    {
      // Stopped between hops, with no handle to report through.
      reportWithoutHandle();
      return;
    }
    URL_COMPONENTS parts;
    std::wstring url = toWide(m_hop.url);
    if (!crackUrl(url, &parts))
    {
      failBeforeSending({NetErrorCode::Policy, "the URL could not be parsed"});
      return;
    }
    std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    m_hopHost = host;
    m_proxied = false;
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (path.empty())
    {
      path = L"/";
    }

    m_connection = WinHttpConnect(m_session, host.c_str(), parts.nPort, 0);
    if (m_connection == nullptr)
    {
      failBeforeSending({NetErrorCode::Network, errorMessage(GetLastError())});
      return;
    }
    DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE
                                                         : 0;
    HINTERNET request = WinHttpOpenRequest(
        m_connection,
        toWide(m_hop.method).c_str(),
        path.c_str(),
        nullptr,
        nullptr,
        nullptr,
        flags);
    if (request == nullptr)
    {
      failBeforeSending({NetErrorCode::Network, errorMessage(GetLastError())});
      return;
    }

    // From here this hop's closing callback reports or continues the
    // exchange, and WinHTTP holds this request until then.
    ref();
    DWORD_PTR context = reinterpret_cast<DWORD_PTR>(this);
    if (!WinHttpSetOption(
            request,
            WINHTTP_OPTION_CONTEXT_VALUE,
            &context,
            sizeof(context)))
    {
      DWORD error = GetLastError();
      unref();
      WinHttpCloseHandle(request);
      failBeforeSending({NetErrorCode::Network, errorMessage(error)});
      return;
    }
    bool stopped;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_request = request;
      stopped = m_closing;
    }
    if (stopped)
    {
      // The watchdog gave up before the handle existed. The guard closes
      // it on the way out, and the closing callback reports the timeout.
      return;
    }

    DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    DWORD autologon = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    // Redirects are this file's to follow; see the top.
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(
            request,
            WINHTTP_OPTION_DISABLE_FEATURE,
            &disabled,
            sizeof(disabled)) ||
        !WinHttpSetOption(
            request,
            WINHTTP_OPTION_AUTOLOGON_POLICY,
            &autologon,
            sizeof(autologon)) ||
        !WinHttpSetOption(
            request,
            WINHTTP_OPTION_REDIRECT_POLICY,
            &redirects,
            sizeof(redirects)))
    {
      stop({NetErrorCode::Network, errorMessage(GetLastError())});
      return;
    }
    // Each phase gets the whole timeout; the watchdog bounds the exchange.
    int timeout = (int)m_hop.timeoutMs;
    WinHttpSetTimeouts(request, timeout, timeout, timeout, timeout);
    for (const HttpHeader& header : m_hop.headers)
    {
      std::wstring line = toWide(header.name) + L": " + toWide(header.value);
      if (!WinHttpAddRequestHeaders(
              request,
              line.c_str(),
              (DWORD)-1,
              WINHTTP_ADDREQ_FLAG_ADD |
                  WINHTTP_ADDREQ_FLAG_COALESCE_WITH_COMMA))
      {
        stop(
            {NetErrorCode::Network,
             "a header could not be set: " + errorMessage(GetLastError())});
        return;
      }
    }
    // The body must outlive the send, so it stays a member.
    DWORD bodySize = (DWORD)m_hop.body.size();
    if (!WinHttpSendRequest(
            request,
            nullptr,
            0,
            bodySize == 0 ? nullptr : m_hop.body.data(),
            bodySize,
            bodySize,
            context))
    {
      stop({NetErrorCode::Network, errorMessage(GetLastError())});
    }
  }

  // Before WinHTTP holds this hop, there is no closing callback to report
  // through: the exchange ends here, with `error` unless a reason (a
  // timeout, say) was recorded first.
  void failBeforeSending(NetError error)
  {
    if (m_connection != nullptr)
    {
      WinHttpCloseHandle(m_connection);
      m_connection = nullptr;
    }
    endWithoutHandle(&error);
  }

  // The exchange ended while no handle existed to close: report what was
  // recorded.
  void reportWithoutHandle() { endWithoutHandle(nullptr); }

  void endWithoutHandle(const NetError* error)
  {
    bool failed;
    NetError recorded;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (!m_closing)
      {
        m_closing = true;
        if (error != nullptr)
        {
          m_failed = true;
          m_error = *error;
        }
      }
      failed = m_failed;
      recorded = m_error;
    }
    forget();
    if (failed)
    {
      fail(m_id, std::move(recorded));
    }
    else
    {
      fail(m_id, {NetErrorCode::Aborted, "the request was cancelled"});
    }
  }

  // Leaves the registry: the exchange is over.
  void forget()
  {
    HINTERNET session = nullptr;
    {
      std::lock_guard<std::mutex> lock(m_registry->mutex);
      m_registry->requests.erase(m_id);
      session = m_registry->takeSessionIfDone();
    }
    if (session != nullptr)
    {
      WinHttpCloseHandle(session);
    }
  }

  // WinHTTP is resolving the name it will connect to: the hop's host, or
  // a proxy's.
  void onResolving(const wchar_t* name)
  {
    m_proxied = !sameHost(name, m_hopHost);
  }

  // As the connection is made: the address the host resolved to is the
  // policy's to judge, unless it is a proxy's. A refusal closes the handle
  // before the request goes out.
  void onConnecting(const wchar_t* address)
  {
    if (m_proxied || m_allowLocalAddresses)
    {
      return;
    }
    uint8_t bytes[16];
    size_t length = 0;
    std::string reason;
    if (!parseAddress(address, bytes, &length))
    {
      reason = "the host resolves to an address that could not be checked";
    }
    else if (NetPolicy::checkAddress(bytes, length, &reason))
    {
      return;
    }
    stop(
        {NetErrorCode::Policy,
         m_hops == 0 ? reason : "a redirect was refused: " + reason});
  }

  void onHeaders(HINTERNET handle)
  {
    DWORD status = 0;
    if (!queryNumber(handle, WINHTTP_QUERY_STATUS_CODE, &status) ||
        status > 999)
    {
      stop({NetErrorCode::Network, "no HTTP response arrived"});
      return;
    }
    HttpResponse response;
    response.status = (uint16_t)status;
    std::wstring text;
    if (queryHeader(handle, WINHTTP_QUERY_STATUS_TEXT, &text))
    {
      response.statusText = headerBytes(text.data(), text.size());
    }

    if (response.statusText.empty())
    {
      // HTTP/2 and later carry no reason phrase.
      response.statusText = standardReasonPhrase(response.status);
    }
    std::wstring raw;
    if (queryHeader(handle, WINHTTP_QUERY_RAW_HEADERS_CRLF, &raw))
    {
      parseHeaders(raw, &response.headers);
    }
    response.url = m_hop.url;

    // A redirect: the policy decides and rewrites, and this hop's handle
    // closes once the guard lets go; its closing callback sends the next.
    // A 3xx without a Location is an answer like any other.
    const std::string* location = isRedirect(response.status)
                                      ? findHeader(response.headers, "location")
                                      : nullptr;
    if (location != nullptr)
    {
      HttpRequest next = m_hop;
      NetError error;
      if (!NetPolicy::redirect(
              next,
              response.status,
              *location,
              m_hops,
              scriptnet::limits(),
              &error))
      {
        stop({error.code, "a redirect was refused: " + error.message});
        return;
      }
      m_hop = std::move(next);
      m_hops++;
      std::lock_guard<std::mutex> lock(m_mutex);
      if (!m_closing)
      {
        m_hopping = true;
      }
      return;
    }

    // Without a body to expect, the length is no reason to stop or reserve;
    // onRead still counts what does arrive.
    if (expectsBody(m_hop.method == "HEAD", response.status))
    {
      const std::string* length = findHeader(
          response.headers,
          "content-length");
      unsigned long long expected = length == nullptr ? 0
                                                      : std::strtoull(
                                                            length->c_str(),
                                                            nullptr,
                                                            10);
      if (expected > m_maxBodyBytes)
      {
        stop(
            {NetErrorCode::TooLarge,
             "the response body is larger than " +
                 std::to_string(m_maxBodyBytes) + " bytes"});
        return;
      }
      m_body.reserve((size_t)expected);
    }
    m_response = std::move(response);
    m_chunk.resize(kReadChunkBytes);
    if (!WinHttpReadData(handle, m_chunk.data(), kReadChunkBytes, nullptr))
    {
      stop({NetErrorCode::Network, errorMessage(GetLastError())});
    }
  }

  void onRead(HINTERNET handle, DWORD length)
  {
    if (length == 0)
    {
      settle(true, nullptr);
      return;
    }
    if (m_body.size() + length > m_maxBodyBytes)
    {
      stop(
          {NetErrorCode::TooLarge,
           "the response body is larger than " +
               std::to_string(m_maxBodyBytes) + " bytes"});
      return;
    }
    m_body.insert(m_body.end(), m_chunk.begin(), m_chunk.begin() + length);
    if (!WinHttpReadData(handle, m_chunk.data(), kReadChunkBytes, nullptr))
    {
      stop({NetErrorCode::Network, errorMessage(GetLastError())});
    }
  }

  void onError(DWORD code)
  {
    // A reason recorded by stop() outranks the cancellation it caused.
    switch (code)
    {
      case ERROR_WINHTTP_OPERATION_CANCELLED:
        stop({NetErrorCode::Aborted, "the request was cancelled"});
        break;
      case ERROR_WINHTTP_TIMEOUT:
        stop({NetErrorCode::Timeout, "no response before the timeout"});
        break;
      default:
        stop({NetErrorCode::Network, errorMessage(code)});
        break;
    }
  }

  // The last callback for a hop's handle: send the next hop, or report.
  void onClosing()
  {
    // The connection, a parent of the handle just closed, goes before the
    // session, its own parent.
    if (m_connection != nullptr)
    {
      WinHttpCloseHandle(m_connection);
      m_connection = nullptr;
    }
    bool hop;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_request = nullptr;
      m_closeSent = false;
      hop = m_hopping && !m_closing;
      m_hopping = false;
    }
    if (hop)
    {
      sendHop();
      // WinHTTP's hold on the hop just closed, taken in sendHop().
      unref();
      return;
    }
    forget();
    bool failed;
    bool completed;
    NetError error;
    HttpResponse response;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      failed = m_failed;
      completed = m_completed;
      error = std::move(m_error);
      response = std::move(m_response);
    }
    if (failed)
    {
      fail(m_id, std::move(error));
    }
    else if (completed)
    {
      response.body = std::move(m_body);
      complete(m_id, std::move(response));
    }
    else
    {
      fail(m_id, {NetErrorCode::Aborted, "the request was cancelled"});
    }
    unref();
  }

  rcp<Registry> m_registry;
  RequestId m_id;
  HINTERNET m_session = nullptr;
  // The request as last sent: the script's, then each hop's.
  HttpRequest m_hop;
  uint32_t m_hops = 0;
  Clock::time_point m_deadline;
  uint32_t m_maxBodyBytes;
  bool m_allowLocalAddresses;

  // Only the callback thread of the moment touches these; WinHTTP never
  // runs two callbacks for one handle at once, and hops run one at a time.
  HINTERNET m_connection = nullptr;
  // The hop's host as sent, and whether WinHTTP is reaching it through a
  // proxy, which it resolves instead.
  std::wstring m_hopHost;
  bool m_proxied = false;
  std::vector<uint8_t> m_chunk;
  std::vector<uint8_t> m_body;

  std::mutex m_mutex;
  // The hop's handle, from its creation to its closing callback.
  HINTERNET m_request = nullptr;
  // Frames inside a WinHTTP call on m_request.
  int m_calls = 0;
  // The exchange has ended; what follows is reported from the closing
  // callback.
  bool m_closing = false;
  // m_request is to close so the next hop can be sent.
  bool m_hopping = false;
  bool m_closeSent = false;
  bool m_failed = false;
  NetError m_error;
  bool m_completed = false;
  HttpResponse m_response;
};

class WindowsProvider : public Provider
{
public:
  WindowsProvider() : m_registry(make_rcp<Registry>())
  {
    // The system's proxy settings, as the other transports use, found the
    // way Windows 8.1 and later recommend; older WinHTTP falls back to its
    // own machine-wide setting.
    HINTERNET session = WinHttpOpen(
        L"Rive",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        nullptr,
        nullptr,
        WINHTTP_FLAG_ASYNC);
    if (session == nullptr)
    {
      session = WinHttpOpen(
          L"Rive",
          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
          nullptr,
          nullptr,
          WINHTTP_FLAG_ASYNC);
    }
    if (session != nullptr)
    {
      // Browsers and NSURLSession hand scripts decoded bodies too.
      DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
      WinHttpSetOption(
          session,
          WINHTTP_OPTION_DECOMPRESSION,
          &decompression,
          sizeof(decompression));
      // HTTP/2 where the server offers it, as NSURLSession; older WinHTTP
      // declines the option and stays on HTTP/1.1.
      DWORD protocols = WINHTTP_PROTOCOL_FLAG_HTTP2;
      WinHttpSetOption(
          session,
          WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL,
          &protocols,
          sizeof(protocols));

      if (WinHttpSetStatusCallback(
              session,
              &Request::onStatus,
              WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS |
                  WINHTTP_CALLBACK_FLAG_HANDLES |
                  WINHTTP_CALLBACK_STATUS_RESOLVING_NAME |
                  WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER,
              0) == WINHTTP_INVALID_STATUS_CALLBACK)
      {
        WinHttpCloseHandle(session);
        session = nullptr;
      }
    }
    m_registry->session = session;
    m_watchdog = std::thread([registry = m_registry] { watch(registry); });
  }

  ~WindowsProvider() override
  {
    // Requests still out answer with a cancellation, which no one is
    // listening for. Their closing callbacks keep the registry alive, and
    // the last of them closes the session; with none out, it goes now.
    std::vector<rcp<Request>> requests;
    HINTERNET session = nullptr;
    {
      std::lock_guard<std::mutex> lock(m_registry->mutex);
      m_registry->stopping = true;
      for (auto& entry : m_registry->requests)
      {
        requests.push_back(entry.second);
      }
      session = m_registry->takeSessionIfDone();
    }
    if (session != nullptr)
    {
      WinHttpCloseHandle(session);
    }
    m_registry->wake.notify_all();

    m_watchdog.join();
    for (const rcp<Request>& request : requests)
    {
      request->abort();
    }
  }

  void start(RequestId id, const HttpRequest& request) override
  {
    // The session is fixed from the constructor until the registry closes
    // it, which only happens once this provider is gone.
    HINTERNET session = m_registry->session;
    if (session == nullptr)
    {
      fail(id, {NetErrorCode::Network, "WinHTTP is unavailable"});
      return;
    }
    auto entry = make_rcp<Request>(
        m_registry,
        id,
        request,
        scriptnet::limits());
    {
      std::lock_guard<std::mutex> lock(m_registry->mutex);
      m_registry->requests[id] = entry;
    }
    m_registry->wake.notify_all();
    entry->start(session);
  }

  void abort(RequestId id) override
  {
    rcp<Request> entry;
    {
      std::lock_guard<std::mutex> lock(m_registry->mutex);
      auto it = m_registry->requests.find(id);
      if (it != m_registry->requests.end())
      {
        entry = it->second;
      }
    }
    if (entry != nullptr)
    {
      entry->abort();
    }
  }

private:
  // WinHTTP's timeouts are per phase; the script's covers the whole
  // exchange, so a thread stops whatever outlives its deadline.
  static void watch(rcp<Registry> registry)
  {
    std::unique_lock<std::mutex> lock(registry->mutex);
    while (!registry->stopping)
    {
      Clock::time_point now = Clock::now();
      Clock::time_point next = Clock::time_point::max();
      std::vector<rcp<Request>> expired;
      for (auto& entry : registry->requests)
      {
        const rcp<Request>& request = entry.second;
        if (request->closing())
        {
          continue;
        }
        if (request->deadline() <= now)
        {
          expired.push_back(request);
        }
        else
        {
          next = std::min(next, request->deadline());
        }
      }
      if (!expired.empty())
      {
        lock.unlock();
        for (const rcp<Request>& request : expired)
        {
          request->stop(
              {NetErrorCode::Timeout, "no response before the timeout"});
        }
        lock.lock();
        continue;
      }
      if (next == Clock::time_point::max())
      {
        registry->wake.wait(lock);
      }
      else
      {
        registry->wake.wait_until(lock, next);
      }
    }
  }

  rcp<Registry> m_registry;
  std::thread m_watchdog;
};
} // namespace

rcp<Provider> rive::scriptnet::makePlatformProvider()
{
  return make_rcp<WindowsProvider>();
}

#endif // WITH_RIVE_SCRIPTNET && _WIN32
