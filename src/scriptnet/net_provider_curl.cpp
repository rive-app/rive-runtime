/*
 * Copyright 2026 Rive
 */

#if defined(WITH_RIVE_SCRIPTNET) && defined(WITH_RIVE_SCRIPTNET_CURL)

#include "rive/scriptnet/net.hpp"

#include <curl/curl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// Script requests over our vendored curl, where the platform has no HTTP
// stack of its own. As on Apple and Windows, nothing answers for the user:
// curl keeps no cookies, reads no .netrc and answers no authentication
// challenge. Proxies come from the environment, as Windows takes the
// system's.
//
// Redirects are followed here, as on Windows: NetPolicy::redirect rewrites
// each 3xx and runs the policy, and the hop goes out as a transfer of its
// own. A connection's address is checked before its socket opens and again
// before each request it carries, unless it is a proxy's or the host chose
// it, and a refused hop goes out on none.
//
// One thread runs every transfer and all of curl's callbacks. Calls from
// other threads queue a command for it and wake it.

using namespace rive;
using namespace rive::scriptnet;

namespace
{
using Clock = std::chrono::steady_clock;

// Commands still reach the thread if a wakeup is ever lost.
constexpr int kLongestWaitMs = 1000;

// Our curl may run on a system that keeps its certificates elsewhere than
// the one it was built on. Keep in sync with rive_net's curl_tls.cpp.
const char* systemCertificates()
{
  static const char* const kBundles[] = {
      "/etc/ssl/certs/ca-certificates.crt",
      "/etc/pki/tls/certs/ca-bundle.crt",
      "/etc/ssl/cert.pem",
  };
  for (const char* bundle : kBundles)
  {
    if (access(bundle, R_OK) == 0)
    {
      return bundle;
    }
  }
  return nullptr;
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

std::string trimmed(const char* text, size_t length)
{
  size_t start = 0;
  while (start < length && (text[start] == ' ' || text[start] == '\t'))
  {
    start++;
  }
  while (length > start &&
         (text[length - 1] == ' ' || text[length - 1] == '\t'))
  {
    length--;
  }
  return std::string(text + start, length - start);
}

// One exchange: a request and the hops its redirects lead to, sent one at a
// time on one easy handle. Only the transfer thread touches it.
struct Transfer
{
  RequestId id = 0;
  // The request as last sent: the script's, then each hop's.
  HttpRequest hop;
  uint32_t hops = 0;
  Clock::time_point deadline;
  bool allowLocalAddresses = false;
  CURL* easy = nullptr;
  curl_slist* headers = nullptr;
  char errorText[CURL_ERROR_SIZE] = {};

  // The response whose head is arriving.
  HttpResponse head;
  // scriptnet has the head, and said the timeout no longer covers the body.
  bool responded = false;
  bool untimed = false;
  // receive() asked for no more yet, and curl holds back a run for that.
  bool full = false;
  bool paused = false;
  // The head was a redirect the policy allowed to `next`.
  bool redirected = false;
  HttpRequest next;
  // Why this file stopped the transfer, reported instead of curl's error.
  bool failed = false;
  NetError error;

  ~Transfer()
  {
    curl_slist_free_all(headers);
    curl_easy_cleanup(easy);
  }

  // Records why, for the outcome; the first reason wins.
  void stop(NetError reason)
  {
    if (!failed)
    {
      failed = true;
      error = std::move(reason);
    }
  }
};

size_t onHeader(char* data, size_t size, size_t count, void* user);
size_t onBody(char* data, size_t size, size_t count, void* user);
curl_socket_t onOpenSocket(
    void* user,
    curlsocktype purpose,
    curl_sockaddr* address);
int onConnected(void* user, char*, char*, int, int);

// Points the easy handle at the transfer's hop, from a clean slate.
bool configure(Transfer* transfer)
{
  CURL* easy = transfer->easy;
  curl_easy_reset(easy);
  curl_slist_free_all(transfer->headers);
  transfer->headers = nullptr;
  transfer->head = HttpResponse();
  transfer->errorText[0] = '\0';

  const HttpRequest& hop = transfer->hop;
  bool typed = false;
  std::vector<std::string> lines;
  for (const HttpHeader& header : hop.headers)
  {
    typed = typed || lowerAscii(header.name) == "content-type";
    // curl drops a header with nothing after the colon; this sends it empty.
    lines.push_back(
        header.name + (header.value.empty() ? ";" : ": " + header.value));
  }
  // Otherwise curl adds a form Content-Type to any body, and Expect to a
  // large one.
  if (!typed)
  {
    lines.push_back("Content-Type:");
  }
  lines.push_back("Expect:");
  for (const std::string& line : lines)
  {
    curl_slist* appended = curl_slist_append(transfer->headers, line.c_str());
    if (appended == nullptr)
    {
      return false;
    }
    transfer->headers = appended;
  }

  curl_easy_setopt(easy, CURLOPT_URL, hop.url.c_str());
  curl_easy_setopt(easy, CURLOPT_PRIVATE, transfer);
  curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, transfer->errorText);
  // Name lookups would otherwise time out by signal, which a process with
  // other threads must not take.
  curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
  // Or dropping a transfer waits for a lookup it still has running.
  curl_easy_setopt(easy, CURLOPT_QUICK_EXIT, 1L);
  curl_easy_setopt(easy, CURLOPT_HTTPHEADER, transfer->headers);
  curl_easy_setopt(easy, CURLOPT_USERAGENT, "Rive");
  // Browsers and the other transports hand scripts decoded bodies too.
  curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(easy, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);
  curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, onHeader);
  curl_easy_setopt(easy, CURLOPT_HEADERDATA, transfer);
  curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, onBody);
  curl_easy_setopt(easy, CURLOPT_WRITEDATA, transfer);
  curl_easy_setopt(easy, CURLOPT_OPENSOCKETFUNCTION, onOpenSocket);
  curl_easy_setopt(easy, CURLOPT_OPENSOCKETDATA, transfer);
  curl_easy_setopt(easy, CURLOPT_PREREQFUNCTION, onConnected);
  curl_easy_setopt(easy, CURLOPT_PREREQDATA, transfer);
  static const char* certificates = systemCertificates();
  if (certificates != nullptr)
  {
    curl_easy_setopt(easy, CURLOPT_CAINFO, certificates);
  }

  if (hop.method == "HEAD")
  {
    curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);
  }
  else if (hop.method != "GET")
  {
    // curl reads the body from here as it sends, so it stays a member.
    curl_easy_setopt(
        easy,
        CURLOPT_POSTFIELDSIZE_LARGE,
        (curl_off_t)hop.body.size());
    curl_easy_setopt(
        easy,
        CURLOPT_POSTFIELDS,
        hop.body.empty() ? "" : (const char*)hop.body.data());
    if (hop.method != "POST")
    {
      curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, hop.method.c_str());
    }
  }
  return true;
}

// The head is complete: carries a redirect to its hop, or hands scriptnet
// the response. False ends the transfer.
bool endHead(Transfer* transfer)
{
  HttpResponse& head = transfer->head;
  if (head.status >= 100 && head.status < 200)
  {
    // An interim answer; the real one follows.
    return true;
  }
  if (head.status < 100 || head.status > 999)
  {
    transfer->stop({NetErrorCode::Network, "no HTTP response arrived"});
    return false;
  }
  if (head.statusText.empty())
  {
    // HTTP/2 and later carry no reason phrase.
    head.statusText = standardReasonPhrase(head.status);
  }
  head.url = transfer->hop.url;

  // A 3xx without a Location is an answer like any other.
  const std::string* location = isRedirect(head.status)
                                    ? findHeader(head.headers, "location")
                                    : nullptr;
  if (location != nullptr)
  {
    HttpRequest next = transfer->hop;
    NetError error;
    if (!NetPolicy::redirect(
            next,
            head.status,
            *location,
            transfer->hops,
            scriptnet::limits(),
            &error))
    {
      transfer->stop({error.code, "a redirect was refused: " + error.message});
      return false;
    }
    // Sent once this transfer has ended, which it does now.
    transfer->next = std::move(next);
    transfer->redirected = true;
    return false;
  }

  transfer->untimed = !respond(transfer->id, std::move(head));
  transfer->responded = true;
  return true;
}

size_t onHeader(char* data, size_t size, size_t count, void* user)
{
  auto* transfer = static_cast<Transfer*>(user);
  size_t length = size * count;
  if (transfer->responded)
  {
    // Trailers.
    return length;
  }
  size_t end = length;
  while (end > 0 && (data[end - 1] == '\r' || data[end - 1] == '\n'))
  {
    end--;
  }
  HttpResponse& head = transfer->head;
  if (end == 0)
  {
    return endHead(transfer) ? length : 0;
  }
  if (end >= 5 && std::memcmp(data, "HTTP/", 5) == 0)
  {
    // "HTTP/1.1 200 OK", or "HTTP/2 200" with no reason.
    head = HttpResponse();
    std::string line(data, end);
    size_t code = line.find(' ');
    if (code != std::string::npos)
    {
      head.status = (uint16_t)std::min<unsigned long>(
          std::strtoul(line.c_str() + code + 1, nullptr, 10),
          1000);
      size_t reason = line.find(' ', code + 1);
      if (reason != std::string::npos)
      {
        head.statusText = line.substr(reason + 1);
      }
    }
    return length;
  }
  if (data[0] == ' ' || data[0] == '\t')
  {
    // A folded line continues the header before it.
    if (!head.headers.empty())
    {
      head.headers.back().value += " " + trimmed(data, end);
    }
    return length;
  }
  const char* colon = static_cast<const char*>(std::memchr(data, ':', end));
  if (colon != nullptr && colon != data)
  {
    size_t name = (size_t)(colon - data);
    head.headers.push_back(
        {std::string(data, name), trimmed(colon + 1, end - name - 1)});
  }
  return length;
}

size_t onBody(char* data, size_t size, size_t count, void* user)
{
  auto* transfer = static_cast<Transfer*>(user);
  if (transfer->full)
  {
    // curl keeps this run and brings it again once resumed.
    transfer->paused = true;
    return CURL_WRITEFUNC_PAUSE;
  }
  size_t length = size * count;
  transfer->full = !receive(
      transfer->id,
      reinterpret_cast<const uint8_t*>(data),
      length);
  return length;
}

// Whether the hop may reach `address`, recording why not. A proxy's address
// and the host's own endpoint are not the policy's to judge.
bool reachable(Transfer* transfer, int family, const void* address)
{
  long proxied = 0;
  // Known from the moment curl picks the connection, before it opens one.
  curl_easy_getinfo(transfer->easy, CURLINFO_USED_PROXY, &proxied);
  if (proxied != 0 || transfer->allowLocalAddresses || transfer->hop.hostChosen)
  {
    return true;
  }
  size_t length = family == AF_INET ? 4 : family == AF_INET6 ? 16 : 0;
  std::string reason;
  if (length == 0)
  {
    reason = "the host resolves to an address that could not be checked";
  }
  else if (NetPolicy::checkAddress(
               static_cast<const uint8_t*>(address),
               length,
               &reason))
  {
    return true;
  }
  transfer->stop(
      {NetErrorCode::Policy,
       transfer->hops == 0 ? reason : "a redirect was refused: " + reason});
  return false;
}

// A refusal opens nothing.
curl_socket_t onOpenSocket(
    void* user,
    curlsocktype purpose,
    curl_sockaddr* address)
{
  auto* transfer = static_cast<Transfer*>(user);
  if (purpose == CURLSOCKTYPE_IPCXN)
  {
    const sockaddr* socketAddress = &address->addr;
    const void* bytes = address->family == AF_INET
                            ? (const void*)&((const sockaddr_in*)socketAddress)
                                  ->sin_addr
                            : (const void*)&((const sockaddr_in6*)socketAddress)
                                  ->sin6_addr;
    reachable(transfer, address->family, bytes);
  }
  // Once one address is refused, so are the rest curl would try.
  if (transfer->failed)
  {
    return CURL_SOCKET_BAD;
  }
  return socket(address->family, address->socktype, address->protocol);
}

// Each request checks its connection's address again, since a reused one
// opens no socket, and curl races the two address families, so one opened
// before the other was refused can still complete.
int onConnected(void* user, char* address, char*, int, int)
{
  auto* transfer = static_cast<Transfer*>(user);
  if (!transfer->failed)
  {
    std::string text(address);
    text = text.substr(0, text.find('%'));
    uint8_t bytes[16];
    int family = text.find(':') == std::string::npos ? AF_INET : AF_INET6;
    if (inet_pton(family, text.c_str(), bytes) != 1)
    {
      family = AF_UNSPEC;
    }
    reachable(transfer, family, bytes);
  }
  return transfer->failed ? CURL_PREREQFUNC_ABORT : CURL_PREREQFUNC_OK;
}

struct Command
{
  enum class Kind : uint8_t
  {
    start,
    abort,
    resume,
  };
  Kind kind;
  RequestId id;
  HttpRequest request;
};

// What the provider and its thread share. The provider never waits for the
// thread, since it can be released under the lock the thread's callbacks
// take, so the thread holds this too.
class Engine : public RefCnt<Engine>
{
public:
  Engine()
  {
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    m_multi = curl_multi_init();
  }

  ~Engine()
  {
    if (m_multi != nullptr)
    {
      curl_multi_cleanup(m_multi);
    }
  }

  bool available() const { return m_multi != nullptr; }

  // Any thread.
  void post(Command command)
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_commands.push_back(std::move(command));
    }
    curl_multi_wakeup(m_multi);
  }

  void stop()
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_stopping = true;
    }
    curl_multi_wakeup(m_multi);
  }

  void run()
  {
    for (;;)
    {
      std::vector<Command> commands;
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping)
        {
          break;
        }
        commands.swap(m_commands);
      }
      for (Command& command : commands)
      {
        apply(command);
      }
      // Ahead of curl, so a response it holds for an overdue request is late.
      int wait = expire();
      int running = 0;
      curl_multi_perform(m_multi, &running);
      collect();
      curl_multi_poll(m_multi, nullptr, 0, wait, nullptr);
    }
    // Each request holds its provider, so scriptnet has forgotten these.
    while (!m_transfers.empty())
    {
      forget(m_transfers.begin()->second.get());
    }
  }

private:
  void apply(Command& command)
  {
    if (command.kind == Command::Kind::start)
    {
      begin(command.id, std::move(command.request));
      return;
    }
    auto it = m_transfers.find(command.id);
    if (it == m_transfers.end())
    {
      return;
    }
    Transfer* transfer = it->second.get();
    if (command.kind == Command::Kind::abort)
    {
      // The outcome is dropped either way.
      forget(transfer);
      return;
    }
    transfer->full = false;
    if (transfer->paused)
    {
      transfer->paused = false;
      curl_easy_pause(transfer->easy, CURLPAUSE_CONT);
    }
  }

  void begin(RequestId id, HttpRequest request)
  {
    auto transfer = std::make_unique<Transfer>();
    transfer->id = id;
    transfer->deadline = Clock::now() +
                         std::chrono::milliseconds(request.timeoutMs);
    transfer->hop = std::move(request);
    transfer->allowLocalAddresses = scriptnet::limits().allowLocalAddresses;
    transfer->easy = curl_easy_init();
    if (transfer->easy == nullptr)
    {
      fail(id, {NetErrorCode::Network, "curl could not start the request"});
      return;
    }
    Transfer* sent = transfer.get();
    m_transfers[id] = std::move(transfer);
    send(sent);
  }

  void send(Transfer* transfer)
  {
    if (!configure(transfer) ||
        curl_multi_add_handle(m_multi, transfer->easy) != CURLM_OK)
    {
      end(transfer, {NetErrorCode::Network, "curl could not send the request"});
    }
  }

  // Reports each transfer curl has finished, or sends the hop it led to.
  void collect()
  {
    int left = 0;
    while (CURLMsg* message = curl_multi_info_read(m_multi, &left))
    {
      if (message->msg != CURLMSG_DONE)
      {
        continue;
      }
      CURLcode result = message->data.result;
      void* owner = nullptr;
      curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &owner);
      auto* transfer = static_cast<Transfer*>(owner);
      curl_multi_remove_handle(m_multi, transfer->easy);
      if (transfer->redirected && !transfer->failed)
      {
        transfer->hop = std::move(transfer->next);
        transfer->hops++;
        transfer->redirected = false;
        send(transfer);
      }
      else if (transfer->failed)
      {
        end(transfer, std::move(transfer->error));
      }
      else if (result != CURLE_OK)
      {
        std::string text = transfer->errorText;
        end(transfer,
            {NetErrorCode::Network,
             text.empty() ? curl_easy_strerror(result) : text});
      }
      else if (!transfer->responded)
      {
        end(transfer, {NetErrorCode::Network, "no HTTP response arrived"});
      }
      else
      {
        RequestId id = transfer->id;
        forget(transfer);
        finish(id);
      }
    }
  }

  // Times out what its deadline has passed; how long until the next one.
  int expire()
  {
    Clock::time_point now = Clock::now();
    Clock::time_point next = now + std::chrono::milliseconds(kLongestWaitMs);
    std::vector<Transfer*> overdue;
    for (auto& entry : m_transfers)
    {
      Transfer* transfer = entry.second.get();
      if (transfer->untimed)
      {
        continue;
      }
      if (transfer->deadline <= now)
      {
        overdue.push_back(transfer);
      }
      else
      {
        next = std::min(next, transfer->deadline);
      }
    }
    for (Transfer* transfer : overdue)
    {
      end(transfer, {NetErrorCode::Timeout, "no response before the timeout"});
    }
    return (int)std::chrono::ceil<std::chrono::milliseconds>(next - now)
        .count();
  }

  void end(Transfer* transfer, NetError error)
  {
    RequestId id = transfer->id;
    forget(transfer);
    fail(id, std::move(error));
  }

  void forget(Transfer* transfer)
  {
    curl_multi_remove_handle(m_multi, transfer->easy);
    m_transfers.erase(transfer->id);
  }

  CURLM* m_multi = nullptr;
  std::mutex m_mutex;
  std::vector<Command> m_commands;
  bool m_stopping = false;
  // Only the transfer thread touches these.
  std::unordered_map<RequestId, std::unique_ptr<Transfer>> m_transfers;
};

class CurlProvider : public Provider
{
public:
  CurlProvider() : m_engine(make_rcp<Engine>())
  {
    if (m_engine->available())
    {
      std::thread([engine = m_engine] { engine->run(); }).detach();
    }
  }

  ~CurlProvider() override
  {
    if (m_engine->available())
    {
      m_engine->stop();
    }
  }

  void start(RequestId id, const HttpRequest& request) override
  {
    if (!m_engine->available())
    {
      fail(id, {NetErrorCode::Network, "curl is unavailable"});
      return;
    }
    m_engine->post({Command::Kind::start, id, request});
  }

  void abort(RequestId id) override { post(Command::Kind::abort, id); }

  void resume(RequestId id) override { post(Command::Kind::resume, id); }

private:
  void post(Command::Kind kind, RequestId id)
  {
    if (m_engine->available())
    {
      m_engine->post({kind, id, HttpRequest()});
    }
  }

  rcp<Engine> m_engine;
};
} // namespace

rcp<Provider> rive::scriptnet::makePlatformProvider()
{
  return make_rcp<CurlProvider>();
}

#endif // WITH_RIVE_SCRIPTNET && WITH_RIVE_SCRIPTNET_CURL
