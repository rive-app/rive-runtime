/*
 * Copyright 2026 Rive
 */

#if defined(WITH_RIVE_SCRIPTNET) && defined(__ANDROID__)

#include "rive/scriptnet/net.hpp"

#include <jni.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// Script requests on Android, over the platform's HttpURLConnection through
// JNI, so the runtime carries no HTTP stack of its own. Responses are never
// cached, and an app that keeps a process-wide cookie jar gets no script
// requests at all, since a connection cannot opt out of it. Known gap: the
// app's other process-wide defaults still apply, so an Authenticator, SSL
// socket factory or hostname verifier it installs answers for scripts too.
//
// Redirects are followed here, as on Windows: NetPolicy::redirect rewrites
// each 3xx and runs the policy, and the hop goes out as a connection of its
// own. Known gap, as on Apple: the address a host resolves to is not checked,
// since the platform cannot hold a connection to an address it was shown.
// Failures are told in this file's words, since the platform's name the
// address and the certificate.
//
// Known gaps in the platform itself: it buffers a response's head, chunk
// size lines and trailers whole, so a server can fill the app's Java heap
// with one that never ends; before API 26 it may send again a request whose
// connection was cut; and it gives a POST, PUT, PATCH, DELETE or OPTIONS
// with no body and no Content-Type a form one.
//
// HttpURLConnection blocks, so each exchange runs on a thread of its own, and
// backpressure is that thread reading no more. One control thread ends what
// outlives its deadline or is aborted by disconnecting it, the one call the
// platform lets another thread make.

using namespace rive;
using namespace rive::scriptnet;

namespace
{
using Clock = std::chrono::steady_clock;

constexpr jsize kReadChunkBytes = 64 * 1024;

// A disconnect does nothing before the connection has a socket, so the
// control thread asks again this often until the exchange's thread is gone.
constexpr std::chrono::milliseconds kCutRetry(100);

// Each exchange has a thread until the platform call it is in returns, even
// once ended, so the provider holds them all, live or ending, to this.
constexpr int kMostThreads = 256;

std::atomic<JavaVM*> g_vm{nullptr};

// Attaches the calling thread, one of this file's own, for a scope.
class Attached
{
public:
  explicit Attached(JavaVM* vm) : m_vm(vm)
  {
    if (m_vm->AttachCurrentThread(&m_env, nullptr) != JNI_OK)
    {
      m_env = nullptr;
    }
  }
  ~Attached()
  {
    if (m_env != nullptr)
    {
      m_vm->DetachCurrentThread();
    }
  }
  JNIEnv* env() const { return m_env; }

private:
  JavaVM* m_vm;
  JNIEnv* m_env = nullptr;
};

// The Java classes and methods used here, looked up once and kept.
struct Java
{
  bool ok = true;
  jclass url = nullptr;
  jclass connection = nullptr;
  jclass timeout = nullptr;
  jclass unknownHost = nullptr;
  jclass connectFailure = nullptr;
  jclass tlsFailure = nullptr;
  jclass cookieHandler = nullptr;
  jstring utf8 = nullptr;
  jmethodID newUrl = nullptr;
  jmethodID openConnection = nullptr;
  jmethodID defaultCookieHandler = nullptr;
  jmethodID setFollowRedirects = nullptr;
  jmethodID setUseCaches = nullptr;
  jmethodID setConnectTimeout = nullptr;
  jmethodID setRequestMethod = nullptr;
  jmethodID addRequestProperty = nullptr;
  jmethodID setDoOutput = nullptr;
  jmethodID getOutputStream = nullptr;
  jmethodID getResponseCode = nullptr;
  jmethodID getResponseMessage = nullptr;
  jmethodID getHeaderFieldKey = nullptr;
  jmethodID getHeaderField = nullptr;
  jmethodID getInputStream = nullptr;
  jmethodID getErrorStream = nullptr;
  jmethodID disconnect = nullptr;
  jmethodID write = nullptr;
  jmethodID closeOutput = nullptr;
  jmethodID read = nullptr;
  jmethodID closeInput = nullptr;
  jmethodID getClassName = nullptr;
  jmethodID getBytes = nullptr;

  jclass find(JNIEnv* env, const char* name)
  {
    jclass local = env->FindClass(name);
    if (local == nullptr)
    {
      env->ExceptionClear();
      ok = false;
      return nullptr;
    }
    auto kept = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return kept;
  }

  jmethodID method(
      JNIEnv* env,
      jclass owner,
      const char* name,
      const char* signature,
      bool isStatic = false)
  {
    jmethodID id = owner == nullptr ? nullptr
                   : isStatic ? env->GetStaticMethodID(owner, name, signature)
                              : env->GetMethodID(owner, name, signature);
    if (id == nullptr)
    {
      env->ExceptionClear();
      ok = false;
    }
    return id;
  }

  static Java load(JNIEnv* env)
  {
    Java java;
    java.url = java.find(env, "java/net/URL");
    java.connection = java.find(env, "java/net/HttpURLConnection");
    java.timeout = java.find(env, "java/net/SocketTimeoutException");
    java.unknownHost = java.find(env, "java/net/UnknownHostException");
    java.connectFailure = java.find(env, "java/net/ConnectException");
    java.tlsFailure = java.find(env, "javax/net/ssl/SSLException");
    java.cookieHandler = java.find(env, "java/net/CookieHandler");
    jclass output = java.find(env, "java/io/OutputStream");
    jclass input = java.find(env, "java/io/InputStream");
    jclass classClass = java.find(env, "java/lang/Class");
    jclass string = java.find(env, "java/lang/String");
    jstring utf8 = env->NewStringUTF("UTF-8");
    java.utf8 = utf8 == nullptr ? nullptr
                                : static_cast<jstring>(env->NewGlobalRef(utf8));
    java.ok = java.ok && java.utf8 != nullptr;

    java.newUrl = java.method(env, java.url, "<init>", "(Ljava/lang/String;)V");
    java.openConnection = java.method(
        env,
        java.url,
        "openConnection",
        "()Ljava/net/URLConnection;");
    java.defaultCookieHandler = java.method(
        env,
        java.cookieHandler,
        "getDefault",
        "()Ljava/net/CookieHandler;",
        true);
    jclass c = java.connection;
    java.setFollowRedirects = java.method(
        env,
        c,
        "setInstanceFollowRedirects",
        "(Z)V");
    java.setUseCaches = java.method(env, c, "setUseCaches", "(Z)V");
    java.setConnectTimeout = java.method(env, c, "setConnectTimeout", "(I)V");
    java.setRequestMethod = java.method(
        env,
        c,
        "setRequestMethod",
        "(Ljava/lang/String;)V");
    java.addRequestProperty = java.method(
        env,
        c,
        "addRequestProperty",
        "(Ljava/lang/String;Ljava/lang/String;)V");
    java.setDoOutput = java.method(env, c, "setDoOutput", "(Z)V");
    java.getOutputStream = java.method(
        env,
        c,
        "getOutputStream",
        "()Ljava/io/OutputStream;");
    java.getResponseCode = java.method(env, c, "getResponseCode", "()I");
    java.getResponseMessage = java.method(
        env,
        c,
        "getResponseMessage",
        "()Ljava/lang/String;");
    java.getHeaderFieldKey = java.method(
        env,
        c,
        "getHeaderFieldKey",
        "(I)Ljava/lang/String;");
    java.getHeaderField = java.method(
        env,
        c,
        "getHeaderField",
        "(I)Ljava/lang/String;");
    java.getInputStream = java.method(
        env,
        c,
        "getInputStream",
        "()Ljava/io/InputStream;");
    java.getErrorStream = java.method(
        env,
        c,
        "getErrorStream",
        "()Ljava/io/InputStream;");
    java.disconnect = java.method(env, c, "disconnect", "()V");
    java.write = java.method(env, output, "write", "([B)V");
    java.closeOutput = java.method(env, output, "close", "()V");
    java.read = java.method(env, input, "read", "([B)I");
    java.closeInput = java.method(env, input, "close", "()V");
    java.getClassName = java.method(
        env,
        classClass,
        "getName",
        "()Ljava/lang/String;");
    java.getBytes = java.method(
        env,
        string,
        "getBytes",
        "(Ljava/lang/String;)[B");
    return java;
  }

  // Null when this process's Java lacks any of them.
  static const Java* get(JNIEnv* env)
  {
    static const Java java = load(env);
    return java.ok ? &java : nullptr;
  }
};

// A Java string as UTF-8, which GetStringUTFChars does not quite give.
std::string utf8(JNIEnv* env, const Java& java, jstring text)
{
  std::string result;
  if (text == nullptr)
  {
    return result;
  }
  auto bytes = static_cast<jbyteArray>(
      env->CallObjectMethod(text, java.getBytes, java.utf8));
  if (env->ExceptionCheck())
  {
    env->ExceptionClear();
    return result;
  }
  jsize length = env->GetArrayLength(bytes);
  result.resize((size_t)length);
  env->GetByteArrayRegion(
      bytes,
      0,
      length,
      reinterpret_cast<jbyte*>(&result[0]));
  env->DeleteLocalRef(bytes);
  return result;
}

// Whether the last Java call threw. Clears it and says why in `error`, by the
// kind of failure and no more: the platform's own text names the address a
// host resolved to and the certificate it showed.
bool threw(JNIEnv* env, const Java& java, NetError* error)
{
  jthrowable thrown = env->ExceptionOccurred();
  if (thrown == nullptr)
  {
    return false;
  }
  env->ExceptionClear();
  if (env->IsInstanceOf(thrown, java.timeout))
  {
    *error = {NetErrorCode::Timeout, "no response before the timeout"};
  }
  else if (env->IsInstanceOf(thrown, java.unknownHost))
  {
    *error = {NetErrorCode::Network, "the host could not be found"};
  }
  else if (env->IsInstanceOf(thrown, java.connectFailure))
  {
    *error = {NetErrorCode::Network, "could not connect to the server"};
  }
  else if (env->IsInstanceOf(thrown, java.tlsFailure))
  {
    *error = {NetErrorCode::Network, "a secure connection could not be made"};
  }
  else
  {
    jclass kind = env->GetObjectClass(thrown);
    auto name = static_cast<jstring>(
        env->CallObjectMethod(kind, java.getClassName));
    env->ExceptionClear();
    *error = {
        NetErrorCode::Network,
        "the request failed (" + utf8(env, java, name) + ")"};
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(kind);
  }
  env->DeleteLocalRef(thrown);
  return true;
}

// Whether the app keeps a process-wide cookie jar, or would not say; `error`
// says which.
bool sharesCookies(JNIEnv* env, const Java& java, NetError* error)
{
  jobject jar = env->CallStaticObjectMethod(
      java.cookieHandler,
      java.defaultCookieHandler);
  if (threw(env, java, error))
  {
    return true;
  }
  if (jar == nullptr)
  {
    return false;
  }
  env->DeleteLocalRef(jar);
  *error = {
      NetErrorCode::Policy,
      "this app shares a cookie jar with its connections, which scripts may "
      "not use"};
  return true;
}

bool isPlatformNote(const std::string& lowerName)
{
  return lowerName == "x-android-sent-millis" ||
         lowerName == "x-android-received-millis" ||
         lowerName == "x-android-selected-protocol" ||
         lowerName == "x-android-response-source";
}

bool addHeader(
    JNIEnv* env,
    const Java& java,
    jobject connection,
    const std::string& name,
    const std::string& value,
    NetError* error)
{
  jstring key = env->NewStringUTF(name.c_str());
  jstring text = env->NewStringUTF(value.c_str());
  env->CallVoidMethod(connection, java.addRequestProperty, key, text);
  env->DeleteLocalRef(key);
  env->DeleteLocalRef(text);
  return !threw(env, java, error);
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

struct Registry;
// Has the control thread look at its deadlines again.
void touch(Registry* registry);

// One exchange: a request and the hops its redirects lead to, each a
// connection of its own, on the exchange's thread. It ends once, by that
// thread, by its deadline or by an abort, whichever comes first. No call
// into scriptnet is made with its lock held: a provider can be released
// under scriptnet's own.
class Exchange : public RefCnt<Exchange>
{
public:
  Exchange(RequestId id, const HttpRequest& request)
      : m_id(id)
      , m_hop(request)
      , m_deadline(Clock::now() + std::chrono::milliseconds(request.timeoutMs))
  {}

  RequestId id() const { return m_id; }

  // Any thread. Ends the exchange; false when it had ended already, so only
  // the first to end it reports an outcome.
  bool close()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_closed)
    {
      return false;
    }
    m_closed = true;
    m_room.notify_all();
    return true;
  }

  // The control thread. Ends the exchange if its deadline has passed, which
  // leaves alone a body scriptnet no longer times; otherwise brings `next`
  // down to a deadline still ahead. Decided in one step, so a head that
  // arrives as the deadline does is either answered or timed out, not both.
  bool expire(Clock::time_point now, Clock::time_point* next)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_closed || m_untimed)
    {
      return false;
    }
    if (m_deadline > now)
    {
      *next = std::min(*next, m_deadline);
      return false;
    }
    m_closed = true;
    m_room.notify_all();
    return true;
  }

  // Any thread. scriptnet has room for more of the body.
  void resume()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resumed = true;
    m_room.notify_all();
  }

  // The control thread. Unblocks the exchange's thread from the Java call it
  // is in, if its connection has got as far as a socket.
  void cut(JNIEnv* env, const Java& java)
  {
    jobject connection = nullptr;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_connection != nullptr)
      {
        connection = env->NewGlobalRef(m_connection);
      }
    }
    if (connection != nullptr)
    {
      env->CallVoidMethod(connection, java.disconnect);
      env->ExceptionClear();
      env->DeleteGlobalRef(connection);
    }
  }

  // The exchange's thread. True when the whole response arrived; otherwise
  // `error` says why not, unless the exchange was closed meanwhile.
  bool run(JNIEnv* env, const Java& java, Registry* registry, NetError* error)
  {
    bool completed = false;
    bool redirected = true;
    while (redirected && !closed())
    {
      env->PushLocalFrame(16);
      redirected = hop(env, java, registry, &completed, error);
      jobject connection = unpublish();
      if (connection != nullptr)
      {
        if (!completed)
        {
          env->CallVoidMethod(connection, java.disconnect);
          env->ExceptionClear();
        }
        env->DeleteGlobalRef(connection);
      }
      env->PopLocalFrame(nullptr);
    }
    return completed;
  }

private:
  bool closed()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_closed;
  }

  void publish(JNIEnv* env, jobject connection)
  {
    jobject kept = env->NewGlobalRef(connection);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connection = kept;
  }

  jobject unpublish()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::exchange(m_connection, nullptr);
  }

  // Sends m_hop and takes its answer. True when that was a redirect the
  // policy allowed, with m_hop now the request to send next.
  bool hop(
      JNIEnv* env,
      const Java& java,
      Registry* registry,
      bool* completed,
      NetError* error)
  {
    // Asked for each hop, on both sides of the call the platform reads the
    // jar in.
    if (sharesCookies(env, java, error))
    {
      return false;
    }
    jstring address = env->NewStringUTF(m_hop.url.c_str());
    jobject url = env->NewObject(java.url, java.newUrl, address);
    if (threw(env, java, error))
    {
      *error = {NetErrorCode::Policy, "the URL could not be parsed"};
      return false;
    }
    jobject connection = env->CallObjectMethod(url, java.openConnection);
    if (threw(env, java, error) || sharesCookies(env, java, error))
    {
      return false;
    }
    if (!env->IsInstanceOf(connection, java.connection))
    {
      *error = {NetErrorCode::Network, "the URL has no HTTP connection"};
      return false;
    }
    publish(env, connection);
    if (closed())
    {
      return false;
    }

    if (!send(env, java, connection, error))
    {
      return false;
    }

    // Blocks until the response's head has arrived.
    jint status = env->CallIntMethod(connection, java.getResponseCode);
    if (threw(env, java, error))
    {
      return false;
    }
    if (status < 100 || status > 999)
    {
      *error = {NetErrorCode::Network, "no HTTP response arrived"};
      return false;
    }
    if (status < 200)
    {
      // The platform takes an interim answer for the response and leaves
      // the real one unread on the connection.
      *error = {
          NetErrorCode::Network,
          "the server sent an interim response this platform cannot follow"};
      return false;
    }
    HttpResponse head;
    head.status = (uint16_t)status;
    head.url = m_hop.url;
    readHead(env, java, connection, &head);

    // A 3xx without a Location is an answer like any other.
    const std::string* location = isRedirect(head.status)
                                      ? findHeader(head.headers, "location")
                                      : nullptr;
    if (location != nullptr)
    {
      HttpRequest next = m_hop;
      NetError refusal;
      if (!NetPolicy::redirect(
              next,
              head.status,
              *location,
              m_hops,
              scriptnet::limits(),
              &refusal))
      {
        *error = {refusal.code, "a redirect was refused: " + refusal.message};
        return false;
      }
      m_hop = std::move(next);
      m_hops++;
      return true;
    }

    {
      // No deadline ends it while scriptnet says whether one still applies.
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_closed)
      {
        return false;
      }
      m_untimed = true;
    }
    if (respond(m_id, std::move(head)))
    {
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_untimed = false;
      }
      touch(registry);
    }

    // An error status has its body on a stream of its own, or none.
    jobject stream = env->CallObjectMethod(
        connection,
        status >= 400 ? java.getErrorStream : java.getInputStream);
    if (threw(env, java, error))
    {
      return false;
    }
    if (stream != nullptr)
    {
      // These never have a body, yet the platform waits for the one a
      // Content-Length on them names.
      bool bodiless = status == 204 || status == 304;
      if (!bodiless && !readBody(env, java, stream, error))
      {
        return false;
      }
      env->CallVoidMethod(stream, java.closeInput);
      env->ExceptionClear();
    }
    *completed = true;
    return false;
  }

  // Configures the connection for m_hop and writes its body, which is what
  // connects it.
  bool send(JNIEnv* env, const Java& java, jobject connection, NetError* error)
  {
    env->CallVoidMethod(connection, java.setFollowRedirects, JNI_FALSE);
    env->CallVoidMethod(connection, java.setUseCaches, JNI_FALSE);
    // Reads are left untimed, as a body read as it arrives must be; the
    // control thread ends what outlives the deadline.
    auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                    m_deadline - Clock::now())
                    .count();
    env->CallVoidMethod(
        connection,
        java.setConnectTimeout,
        (jint)std::min<long long>(std::max<long long>(left, 1), 0x7fffffff));
    env->CallVoidMethod(
        connection,
        java.setRequestMethod,
        env->NewStringUTF(m_hop.method.c_str()));
    if (threw(env, java, error))
    {
      return false;
    }
    bool typed = false;
    bool agent = false;
    for (const HttpHeader& header : m_hop.headers)
    {
      std::string lower = lowerAscii(header.name);
      typed = typed || lower == "content-type";
      agent = agent || lower == "user-agent";
      // The platform takes X-Android-Protocols and X-Android-Transports as
      // settings of its own, by exact case; in lower case they are headers.
      bool settingLike = lower.rfind("x-android-", 0) == 0;
      if (!addHeader(
              env,
              java,
              connection,
              settingLike ? lower : header.name,
              header.value,
              error))
      {
        return false;
      }
    }
    // As the Windows and curl transports say, not the device's model and
    // build.
    if (!agent &&
        !addHeader(env, java, connection, "User-Agent", "Rive", error))
    {
      return false;
    }
    if (m_hop.body.empty())
    {
      return true;
    }
    // The platform gives every method that may have a body a form
    // Content-Type and cannot be told to leave it out. A body nobody typed
    // is octets; with no body the platform's stays.
    if (!typed && !addHeader(
                      env,
                      java,
                      connection,
                      "Content-Type",
                      "application/octet-stream",
                      error))
    {
      return false;
    }
    env->CallVoidMethod(connection, java.setDoOutput, JNI_TRUE);
    jobject output = env->CallObjectMethod(connection, java.getOutputStream);
    if (threw(env, java, error))
    {
      return false;
    }
    jbyteArray body = env->NewByteArray((jsize)m_hop.body.size());
    if (body == nullptr)
    {
      env->ExceptionClear();
      *error = {NetErrorCode::Network, "no memory for the request body"};
      return false;
    }
    env->SetByteArrayRegion(
        body,
        0,
        (jsize)m_hop.body.size(),
        reinterpret_cast<const jbyte*>(m_hop.body.data()));
    env->CallVoidMethod(output, java.write, body);
    if (threw(env, java, error))
    {
      return false;
    }
    env->CallVoidMethod(output, java.closeOutput);
    return !threw(env, java, error);
  }

  void readHead(
      JNIEnv* env,
      const Java& java,
      jobject connection,
      HttpResponse* head)
  {
    auto text = static_cast<jstring>(
        env->CallObjectMethod(connection, java.getResponseMessage));
    env->ExceptionClear();
    head->statusText = utf8(env, java, text);
    if (head->statusText.empty())
    {
      // A status line may carry no reason phrase.
      head->statusText = standardReasonPhrase(head->status);
    }
    for (jint index = 0;; index++)
    {
      auto value = static_cast<jstring>(
          env->CallObjectMethod(connection, java.getHeaderField, index));
      env->ExceptionClear();
      auto name = static_cast<jstring>(
          env->CallObjectMethod(connection, java.getHeaderFieldKey, index));
      env->ExceptionClear();
      if (value == nullptr)
      {
        break;
      }
      // Some stacks list the status line first, with no name, and the
      // platform adds four notes of its own.
      std::string key = utf8(env, java, name);
      if (!key.empty() && !isPlatformNote(lowerAscii(key)))
      {
        head->headers.push_back({std::move(key), utf8(env, java, value)});
      }
      env->DeleteLocalRef(name);
      env->DeleteLocalRef(value);
    }
  }

  // Hands the body to scriptnet as it arrives, reading no more while it has
  // no room. True at the body's end.
  bool readBody(JNIEnv* env, const Java& java, jobject stream, NetError* error)
  {
    jbyteArray buffer = env->NewByteArray(kReadChunkBytes);
    if (buffer == nullptr)
    {
      env->ExceptionClear();
      *error = {NetErrorCode::Network, "no memory for the response body"};
      return false;
    }
    std::vector<uint8_t> run((size_t)kReadChunkBytes);
    while (!closed())
    {
      jint count = env->CallIntMethod(stream, java.read, buffer);
      if (threw(env, java, error))
      {
        return false;
      }
      if (count < 0)
      {
        return true;
      }
      env->GetByteArrayRegion(
          buffer,
          0,
          count,
          reinterpret_cast<jbyte*>(run.data()));
      {
        // Only an ask to carry on that comes from here on is about this run.
        std::lock_guard<std::mutex> lock(m_mutex);
        m_resumed = false;
      }
      if (!receive(m_id, run.data(), (size_t)count))
      {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_resumed && !m_closed)
        {
          m_room.wait(lock);
        }
      }
    }
    return false;
  }

  const RequestId m_id;
  // The request as last sent: the script's, then each hop's. Only the
  // exchange's thread touches these.
  HttpRequest m_hop;
  uint32_t m_hops = 0;
  const Clock::time_point m_deadline;

  std::mutex m_mutex;
  // Wakes a body waiting for room.
  std::condition_variable m_room;
  bool m_closed = false;
  // scriptnet said the script's timeout no longer covers the body.
  bool m_untimed = false;
  // scriptnet asked to carry on since the run now being handed over.
  bool m_resumed = false;
  // The hop's connection, a global reference, while its thread is in it.
  jobject m_connection = nullptr;
};

// What the provider and its threads share. None is ever waited for, since
// the provider can be released under the lock their calls into scriptnet
// take, so each holds this.
struct Registry : public RefCnt<Registry>
{
  JavaVM* vm = nullptr;
  std::mutex mutex;
  // Wakes the control thread, with `changed` set.
  std::condition_variable wake;
  bool changed = false;
  // The provider is gone; the control thread leaves once nothing is ending.
  bool stopping = false;
  std::unordered_map<RequestId, rcp<Exchange>> live;
  // Ended by a deadline or an abort, with a thread still to unblock.
  std::vector<rcp<Exchange>> ending;
  // Exchange threads yet to leave.
  int threads = 0;

  // With the mutex held.
  void touch()
  {
    changed = true;
    wake.notify_all();
  }

  rcp<Exchange> find(RequestId id)
  {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = live.find(id);
    return it == live.end() ? nullptr : it->second;
  }

  // With the mutex held: `exchange` no longer answers to its id.
  void retire(const rcp<Exchange>& exchange)
  {
    auto it = live.find(exchange->id());
    if (it != live.end() && it->second == exchange)
    {
      live.erase(it);
    }
  }

  // The exchange's thread is done with it.
  void leave(const rcp<Exchange>& exchange)
  {
    std::lock_guard<std::mutex> lock(mutex);
    retire(exchange);
    ending.erase(
        std::remove(ending.begin(), ending.end(), exchange),
        ending.end());
    threads--;
    touch();
  }
};

void touch(Registry* registry)
{
  std::lock_guard<std::mutex> lock(registry->mutex);
  registry->touch();
}

void runExchange(rcp<Registry> registry, rcp<Exchange> exchange)
{
  Attached thread(registry->vm);
  const Java* java = thread.env() == nullptr ? nullptr
                                             : Java::get(thread.env());
  NetError error{NetErrorCode::Network, "Java networking is unavailable"};
  bool completed = java != nullptr &&
                   exchange->run(thread.env(), *java, registry.get(), &error);
  // Before the outcome, so a request scriptnet then starts under this id
  // finds none here.
  registry->leave(exchange);
  if (!exchange->close())
  {
    return;
  }
  if (completed)
  {
    finish(exchange->id());
  }
  else
  {
    fail(exchange->id(), std::move(error));
  }
}

// Ends what outlives its deadline, and disconnects whatever a deadline or an
// abort ended until its thread has gone.
void control(rcp<Registry> registry)
{
  Attached thread(registry->vm);
  const Java* java = thread.env() == nullptr ? nullptr
                                             : Java::get(thread.env());
  std::unique_lock<std::mutex> lock(registry->mutex);
  for (;;)
  {
    registry->changed = false;
    Clock::time_point now = Clock::now();
    Clock::time_point next = Clock::time_point::max();
    std::vector<rcp<Exchange>> expired;
    for (auto& entry : registry->live)
    {
      if (entry.second->expire(now, &next))
      {
        expired.push_back(entry.second);
      }
    }
    for (const rcp<Exchange>& exchange : expired)
    {
      registry->retire(exchange);
      registry->ending.push_back(exchange);
    }
    std::vector<rcp<Exchange>> ending = registry->ending;
    if (ending.empty() && registry->stopping)
    {
      return;
    }
    if (!ending.empty())
    {
      next = std::min(next, now + kCutRetry);
      lock.unlock();
      for (const rcp<Exchange>& exchange : expired)
      {
        fail(
            exchange->id(),
            {NetErrorCode::Timeout, "no response before the timeout"});
      }
      if (java != nullptr)
      {
        for (const rcp<Exchange>& exchange : ending)
        {
          exchange->cut(thread.env(), *java);
        }
      }
      lock.lock();
    }
    if (registry->changed)
    {
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

class AndroidProvider : public Provider
{
public:
  explicit AndroidProvider(JavaVM* vm) : m_registry(make_rcp<Registry>())
  {
    m_registry->vm = vm;
    std::thread(control, m_registry).detach();
  }

  ~AndroidProvider() override
  {
    std::lock_guard<std::mutex> lock(m_registry->mutex);
    m_registry->stopping = true;
    for (auto& entry : m_registry->live)
    {
      entry.second->close();
      m_registry->ending.push_back(entry.second);
    }
    m_registry->live.clear();
    m_registry->touch();
  }

  void start(RequestId id, const HttpRequest& request) override
  {
    auto exchange = make_rcp<Exchange>(id, request);
    bool room;
    {
      std::lock_guard<std::mutex> lock(m_registry->mutex);
      room = m_registry->threads < kMostThreads;
      if (room)
      {
        m_registry->threads++;
        m_registry->live[id] = exchange;
        m_registry->touch();
      }
    }
    if (!room)
    {
      fail(
          id,
          {NetErrorCode::RateLimited,
           "too many requests are in flight or still ending"});
      return;
    }
    std::thread(runExchange, m_registry, exchange).detach();
  }

  void abort(RequestId id) override
  {
    // One lock for all of it, so an exchange whose thread has left is never
    // listed as ending.
    std::lock_guard<std::mutex> lock(m_registry->mutex);
    auto it = m_registry->live.find(id);
    if (it == m_registry->live.end())
    {
      return;
    }
    it->second->close();
    m_registry->ending.push_back(it->second);
    m_registry->live.erase(it);
    m_registry->touch();
  }

  void resume(RequestId id) override
  {
    rcp<Exchange> exchange = m_registry->find(id);
    if (exchange != nullptr)
    {
      exchange->resume();
    }
  }

private:
  rcp<Registry> m_registry;
};
} // namespace

void rive::scriptnet::setJavaVM(void* javaVM)
{
  g_vm = static_cast<JavaVM*>(javaVM);
}

rcp<Provider> rive::scriptnet::makePlatformProvider()
{
  JavaVM* vm = g_vm;
  return vm == nullptr ? nullptr : make_rcp<AndroidProvider>(vm);
}

#endif // WITH_RIVE_SCRIPTNET && __ANDROID__
