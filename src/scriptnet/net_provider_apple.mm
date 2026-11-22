/*
 * Copyright 2026 Rive
 */

#ifdef WITH_RIVE_SCRIPTNET

#include "rive/scriptnet/net.hpp"

#import <Foundation/Foundation.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Script requests on Apple platforms. The session is ephemeral and stripped of
// every source of ambient authority: no cookie jar, no credential store, no
// cache, and authentication challenges other than server trust are refused so
// neither keychain identities nor Kerberos/NTLM single sign-on answer for a
// script. Redirect hops are re-checked against NetPolicy, which a browser
// cannot offer.

using namespace rive;
using namespace rive::scriptnet;

namespace
{
std::string toString(NSString* value)
{
  const char* utf8 = value.UTF8String;
  return utf8 != nullptr ? std::string(utf8) : std::string();
}

// A response's status and headers, for scriptnet to have ahead of its body.
HttpResponse headOf(NSHTTPURLResponse* http)
{
  HttpResponse response;
  response.status = (uint16_t)http.statusCode;
  // NSURLSession hides the server's reason phrase.
  response.statusText = standardReasonPhrase(response.status);
  response.url = toString(http.URL.absoluteString);
  HttpHeaders* headers = &response.headers;
  [http.allHeaderFields
      enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL* stop) {
        if ([key isKindOfClass:[NSString class]] &&
            [value isKindOfClass:[NSString class]])
        {
          headers->push_back({toString(key), toString(value)});
        }
      }];
  return response;
}

struct TaskState
{
  RequestId id = 0;
  NSURLSessionTask* task = nil;
  // scriptnet has the status and headers, and gets the body as it arrives.
  bool responded = false;
  // It said the script's timeout no longer covers the body.
  bool untimed = false;
  // Suspended because scriptnet asked for no more of the body yet, and
  // whether it has asked to carry on since the data now arriving came.
  bool suspended = false;
  bool resumed = false;
  // scriptnet aborted it, so the task is cancelled and must stay so.
  bool cancelled = false;
  // Set when this file stops the task itself (policy, timeout, an answer
  // that is not HTTP); it is reported instead of the cancellation it causes.
  bool failed = false;
  NetError error;
};
} // namespace

@interface RiveScriptNetDelegate : NSObject <NSURLSessionDataDelegate>
- (void)track:(NSURLSessionTask*)task id:(RequestId)requestId;
- (void)cancelRequest:(RequestId)requestId;
- (void)resumeRequest:(RequestId)requestId;
- (void)stopTask:(NSUInteger)taskIdentifier
           error:(NetError)error
       timedOnly:(BOOL)timedOnly;
@end

@implementation RiveScriptNetDelegate
{
  std::mutex _mutex;
  std::unordered_map<NSUInteger, TaskState> _tasks;
  std::unordered_map<RequestId, NSUInteger> _taskIds;
}

- (void)track:(NSURLSessionTask*)task id:(RequestId)requestId
{
  std::lock_guard<std::mutex> lock(_mutex);
  TaskState& state = _tasks[task.taskIdentifier];
  state.id = requestId;
  state.task = task;
  _taskIds[requestId] = task.taskIdentifier;
}

- (void)cancelRequest:(RequestId)requestId
{
  NSURLSessionTask* task = nil;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _taskIds.find(requestId);
    if (it == _taskIds.end())
    {
      return;
    }
    auto state = _tasks.find(it->second);
    if (state != _tasks.end())
    {
      state->second.cancelled = true;
      task = state->second.task;
    }
  }
  [task cancel];
}

// scriptnet has room for more of the body: undoes the suspend that asked
// for, and tells data arriving now not to suspend for what it asked before.
- (void)resumeRequest:(RequestId)requestId
{
  NSURLSessionTask* task = nil;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _taskIds.find(requestId);
    auto state = it == _taskIds.end() ? _tasks.end() : _tasks.find(it->second);
    if (state == _tasks.end())
    {
      return;
    }
    state->second.resumed = true;
    if (!state->second.suspended)
    {
      return;
    }
    state->second.suspended = false;
    task = state->second.task;
  }
  [task resume];
}

// Records why the task is being stopped, then cancels it; didComplete
// reports the recorded error. timedOnly is the script's timeout, which
// leaves alone a body scriptnet no longer times.
- (void)stopTask:(NSUInteger)taskIdentifier
           error:(NetError)error
       timedOnly:(BOOL)timedOnly
{
  NSURLSessionTask* task = nil;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _tasks.find(taskIdentifier);
    if (it == _tasks.end() || it->second.failed ||
        (timedOnly && it->second.untimed))
    {
      return;
    }
    it->second.failed = true;
    it->second.error = std::move(error);
    task = it->second.task;
  }
  [task cancel];
}

- (void)answerChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:
          (void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))
              completionHandler
{
  if ([challenge.protectionSpace.authenticationMethod
          isEqualToString:NSURLAuthenticationMethodServerTrust])
  {
    completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
    return;
  }
  // Basic/Digest/NTLM/Negotiate/client certificates: answer with nothing,
  // so the script sees the server's 401 instead of the user's identity.
  completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
}

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:
          (void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))
              completionHandler
{
  [self answerChallenge:challenge completionHandler:completionHandler];
}

- (void)URLSession:(NSURLSession*)session
                   task:(NSURLSessionTask*)task
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:
          (void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*))
              completionHandler
{
  [self answerChallenge:challenge completionHandler:completionHandler];
}

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler
{
  HttpRequest hop;
  hop.url = toString(request.URL.absoluteString);
  hop.method = toString(request.HTTPMethod);
  NetError error;
  if (NetPolicy::check(hop, scriptnet::limits(), &error))
  {
    completionHandler(request);
    return;
  }
  completionHandler(nil);
  [self stopTask:task.taskIdentifier
           error:NetError{
                     NetErrorCode::Policy,
                     "a redirect was refused: " + error.message}
       timedOnly:NO];
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:
         (void (^)(NSURLSessionResponseDisposition))completionHandler
{
  NSHTTPURLResponse* http = [response isKindOfClass:[NSHTTPURLResponse class]]
                                ? (NSHTTPURLResponse*)response
                                : nil;
  if (http == nil)
  {
    [self stopTask:dataTask.taskIdentifier
             error:NetError{NetErrorCode::Network, "no HTTP response arrived"}
         timedOnly:NO];
    completionHandler(NSURLSessionResponseCancel);
    return;
  }
  {
    // Held across respond(), so the timeout sees its answer with it.
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _tasks.find(dataTask.taskIdentifier);
    if (it != _tasks.end() && !it->second.failed && !it->second.responded)
    {
      it->second.responded = true;
      it->second.untimed = !scriptnet::respond(it->second.id, headOf(http));
    }
  }
  completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data
{
  RequestId requestId = 0;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _tasks.find(dataTask.taskIdentifier);
    if (it == _tasks.end() || it->second.failed || !it->second.responded)
    {
      return;
    }
    requestId = it->second.id;
    // Only an ask to carry on that comes from here on is about this data.
    it->second.resumed = false;
  }
  __block bool more = true;
  [data enumerateByteRangesUsingBlock:^(
            const void* bytes, NSRange range, BOOL* stop) {
    more = scriptnet::receive(
               requestId, static_cast<const uint8_t*>(bytes), range.length) &&
           more;
  }];
  if (more)
  {
    return;
  }
  // scriptnet wants no more yet, unless it has since asked to carry on or
  // has aborted the request.
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _tasks.find(dataTask.taskIdentifier);
  if (it == _tasks.end() || it->second.suspended || it->second.resumed ||
      it->second.cancelled || it->second.failed)
  {
    return;
  }
  it->second.suspended = true;
  [dataTask suspend];
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error
{
  TaskState state;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _tasks.find(task.taskIdentifier);
    if (it == _tasks.end())
    {
      return;
    }
    state = std::move(it->second);
    _tasks.erase(it);
    _taskIds.erase(state.id);
  }
  if (state.failed)
  {
    scriptnet::fail(state.id, std::move(state.error));
    return;
  }
  if (error != nil)
  {
    NetErrorCode code = NetErrorCode::Network;
    if ([error.domain isEqualToString:NSURLErrorDomain])
    {
      if (error.code == NSURLErrorTimedOut)
      {
        code = NetErrorCode::Timeout;
      }
      else if (error.code == NSURLErrorCancelled)
      {
        code = NetErrorCode::Aborted;
      }
    }
    std::string message = toString(error.localizedDescription);
    scriptnet::fail(
        state.id, {code, message.empty() ? "request failed" : message});
    return;
  }
  if (!state.responded)
  {
    scriptnet::fail(
        state.id, {NetErrorCode::Network, "no HTTP response arrived"});
    return;
  }
  scriptnet::finish(state.id);
}
@end

namespace
{
class AppleProvider : public Provider
{
public:
  AppleProvider()
  {
    NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration
        ephemeralSessionConfiguration];
    configuration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
    configuration.HTTPShouldSetCookies = NO;
    configuration.HTTPCookieStorage = nil;
    configuration.URLCredentialStorage = nil;
    configuration.URLCache = nil;
    configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;

    NSOperationQueue* queue = [[NSOperationQueue alloc] init];
    queue.maxConcurrentOperationCount = 1;
    queue.name = @"app.rive.scriptnet";
    m_delegate = [[RiveScriptNetDelegate alloc] init];
    m_session = [NSURLSession sessionWithConfiguration:configuration
                                              delegate:m_delegate
                                         delegateQueue:queue];
  }

  // The session retains its delegate until invalidated.
  ~AppleProvider() override { [m_session invalidateAndCancel]; }

  void start(RequestId id, const HttpRequest& request) override
  {
    @autoreleasepool
    {
      NSString* urlString = [NSString stringWithUTF8String:request.url.c_str()];
      NSURL* url = urlString != nil ? [NSURL URLWithString:urlString] : nil;
      if (url == nil)
      {
        fail(id, {NetErrorCode::Policy, "the URL could not be parsed"});
        return;
      }
      NSMutableURLRequest* urlRequest = [NSMutableURLRequest
          requestWithURL:url];
      urlRequest.HTTPMethod = [NSString
          stringWithUTF8String:request.method.c_str()];
      urlRequest.HTTPShouldHandleCookies = NO;
      for (const HttpHeader& header : request.headers)
      {
        NSString* name = [NSString stringWithUTF8String:header.name.c_str()];
        NSString* value = [NSString stringWithUTF8String:header.value.c_str()];
        if (name != nil && value != nil)
        {
          [urlRequest addValue:value forHTTPHeaderField:name];
        }
      }
      if (!request.body.empty())
      {
        urlRequest.HTTPBody = [NSData dataWithBytes:request.body.data()
                                             length:request.body.size()];
      }

      NSURLSessionDataTask* task = [m_session dataTaskWithRequest:urlRequest];
      [m_delegate track:task id:id];

      // Not yet done (see NetPolicy): resolving the host on a background
      // queue before this resume, and before each redirect, to refuse
      // local and private addresses through NetPolicy::checkAddress, as
      // the Windows transport does.

      // The script's timeout covers the exchange, or only the wait for the
      // response when its body is read as it arrives. Such a body's stall
      // is the session's own idle timeout to catch.
      RiveScriptNetDelegate* delegate = m_delegate;
      NSUInteger taskIdentifier = task.taskIdentifier;
      dispatch_after(
          dispatch_time(
              DISPATCH_TIME_NOW, (int64_t)(request.timeoutMs * NSEC_PER_MSEC)),
          dispatch_get_global_queue(QOS_CLASS_UTILITY, 0),
          ^{
            [delegate stopTask:taskIdentifier
                         error:NetError{
                                   NetErrorCode::Timeout,
                                   "no response before the timeout"}
                     timedOnly:YES];
          });
      [task resume];
    }
  }

  void abort(RequestId id) override { [m_delegate cancelRequest:id]; }

  void resume(RequestId id) override { [m_delegate resumeRequest:id]; }

private:
  NSURLSession* m_session = nil;
  RiveScriptNetDelegate* m_delegate = nil;
};
} // namespace

rcp<Provider> rive::scriptnet::makePlatformProvider()
{
  return make_rcp<AppleProvider>();
}

#endif // WITH_RIVE_SCRIPTNET
