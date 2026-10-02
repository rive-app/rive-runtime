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
// NSURLSession hides the server's reason phrase, so report the standard one.
const char* reasonPhrase(NSInteger status)
{
    switch (status)
    {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 202:
            return "Accepted";
        case 204:
            return "No Content";
        case 206:
            return "Partial Content";
        case 301:
            return "Moved Permanently";
        case 302:
            return "Found";
        case 303:
            return "See Other";
        case 304:
            return "Not Modified";
        case 307:
            return "Temporary Redirect";
        case 308:
            return "Permanent Redirect";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 409:
            return "Conflict";
        case 410:
            return "Gone";
        case 413:
            return "Content Too Large";
        case 415:
            return "Unsupported Media Type";
        case 422:
            return "Unprocessable Content";
        case 429:
            return "Too Many Requests";
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        case 502:
            return "Bad Gateway";
        case 503:
            return "Service Unavailable";
        case 504:
            return "Gateway Timeout";
    }
    return "";
}

std::string toString(NSString* value)
{
    const char* utf8 = value.UTF8String;
    return utf8 != nullptr ? std::string(utf8) : std::string();
}

struct TaskState
{
    RequestId id = 0;
    NSURLSessionTask* task = nil;
    uint32_t maxBodyBytes = 0;
    std::vector<uint8_t> body;
    // Set when this file stops the task itself (policy, size, timeout); it is
    // reported instead of the cancellation it causes.
    bool failed = false;
    NetError error;
};
} // namespace

@interface RiveScriptNetDelegate : NSObject <NSURLSessionDataDelegate>
- (void)track:(NSURLSessionTask*)task
              id:(RequestId)requestId
    maxBodyBytes:(uint32_t)maxBodyBytes;
- (void)cancelRequest:(RequestId)requestId;
- (void)stopTask:(NSUInteger)taskIdentifier error:(NetError)error;
@end

@implementation RiveScriptNetDelegate
{
    std::mutex _mutex;
    std::unordered_map<NSUInteger, TaskState> _tasks;
    std::unordered_map<RequestId, NSUInteger> _taskIds;
}

- (void)track:(NSURLSessionTask*)task
              id:(RequestId)requestId
    maxBodyBytes:(uint32_t)maxBodyBytes
{
    std::lock_guard<std::mutex> lock(_mutex);
    TaskState& state = _tasks[task.taskIdentifier];
    state.id = requestId;
    state.task = task;
    state.maxBodyBytes = maxBodyBytes;
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
            task = state->second.task;
        }
    }
    [task cancel];
}

// Records why the task is being stopped, then cancels it; didComplete
// reports the recorded error.
- (void)stopTask:(NSUInteger)taskIdentifier error:(NetError)error
{
    NSURLSessionTask* task = nil;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _tasks.find(taskIdentifier);
        if (it == _tasks.end() || it->second.failed)
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
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler
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
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler
{
    [self answerChallenge:challenge completionHandler:completionHandler];
}

- (void)URLSession:(NSURLSession*)session
                   task:(NSURLSessionTask*)task
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler
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
             error:NetError{NetErrorCode::Policy,
                            "a redirect was refused: " + error.message}];
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:
         (void (^)(NSURLSessionResponseDisposition))completionHandler
{
    long long expected = response.expectedContentLength;
    uint32_t maxBodyBytes = 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _tasks.find(dataTask.taskIdentifier);
        if (it != _tasks.end())
        {
            maxBodyBytes = it->second.maxBodyBytes;
            if (expected > 0 && expected <= maxBodyBytes)
            {
                it->second.body.reserve((size_t)expected);
            }
        }
    }
    if (expected != NSURLResponseUnknownLength && expected > maxBodyBytes)
    {
        completionHandler(NSURLSessionResponseCancel);
        [self stopTask:dataTask.taskIdentifier
                 error:NetError{NetErrorCode::TooLarge,
                                "the response body is larger than " +
                                    std::to_string(maxBodyBytes) + " bytes"}];
        return;
    }
    completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data
{
    bool tooLarge = false;
    uint32_t maxBodyBytes = 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _tasks.find(dataTask.taskIdentifier);
        if (it == _tasks.end() || it->second.failed)
        {
            return;
        }
        TaskState& state = it->second;
        maxBodyBytes = state.maxBodyBytes;
        if (state.body.size() + data.length > maxBodyBytes)
        {
            tooLarge = true;
        }
        else
        {
            std::vector<uint8_t>* body = &state.body;
            [data enumerateByteRangesUsingBlock:^(
                      const void* bytes, NSRange range, BOOL* stop) {
              const uint8_t* begin = static_cast<const uint8_t*>(bytes);
              body->insert(body->end(), begin, begin + range.length);
            }];
        }
    }
    if (tooLarge)
    {
        [self stopTask:dataTask.taskIdentifier
                 error:NetError{NetErrorCode::TooLarge,
                                "the response body is larger than " +
                                    std::to_string(maxBodyBytes) + " bytes"}];
    }
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
        scriptnet::fail(state.id,
                        {code, message.empty() ? "request failed" : message});
        return;
    }
    NSHTTPURLResponse* http =
        [task.response isKindOfClass:[NSHTTPURLResponse class]]
            ? (NSHTTPURLResponse*)task.response
            : nil;
    if (http == nil)
    {
        scriptnet::fail(state.id,
                        {NetErrorCode::Network, "no HTTP response arrived"});
        return;
    }
    HttpResponse response;
    response.status = (uint16_t)http.statusCode;
    response.statusText = reasonPhrase(http.statusCode);
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
    response.body = std::move(state.body);
    scriptnet::complete(state.id, std::move(response));
}
@end

namespace
{
class AppleProvider : public Provider
{
public:
    AppleProvider()
    {
        NSURLSessionConfiguration* configuration =
            [NSURLSessionConfiguration ephemeralSessionConfiguration];
        configuration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
        configuration.HTTPShouldSetCookies = NO;
        configuration.HTTPCookieStorage = nil;
        configuration.URLCredentialStorage = nil;
        configuration.URLCache = nil;
        configuration.requestCachePolicy =
            NSURLRequestReloadIgnoringLocalCacheData;

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
            NSString* urlString =
                [NSString stringWithUTF8String:request.url.c_str()];
            NSURL* url =
                urlString != nil ? [NSURL URLWithString:urlString] : nil;
            if (url == nil)
            {
                fail(id, {NetErrorCode::Policy, "the URL could not be parsed"});
                return;
            }
            NSMutableURLRequest* urlRequest =
                [NSMutableURLRequest requestWithURL:url];
            urlRequest.HTTPMethod =
                [NSString stringWithUTF8String:request.method.c_str()];
            urlRequest.HTTPShouldHandleCookies = NO;
            urlRequest.timeoutInterval = request.timeoutMs / 1000.0;
            for (const HttpHeader& header : request.headers)
            {
                NSString* name =
                    [NSString stringWithUTF8String:header.name.c_str()];
                NSString* value =
                    [NSString stringWithUTF8String:header.value.c_str()];
                if (name != nil && value != nil)
                {
                    [urlRequest addValue:value forHTTPHeaderField:name];
                }
            }
            if (!request.body.empty())
            {
                urlRequest.HTTPBody =
                    [NSData dataWithBytes:request.body.data()
                                   length:request.body.size()];
            }

            NSURLSessionDataTask* task =
                [m_session dataTaskWithRequest:urlRequest];
            [m_delegate track:task
                           id:id
                 maxBodyBytes:scriptnet::limits().maxResponseBodyBytes];

            // Not yet done (see NetPolicy): resolving the host on a background
            // queue before this resume, and before each redirect, to refuse
            // local and private addresses.

            // The request's timeoutInterval is an idle timeout; the script's
            // timeout covers the whole exchange.
            RiveScriptNetDelegate* delegate = m_delegate;
            NSUInteger taskIdentifier = task.taskIdentifier;
            dispatch_after(
                dispatch_time(DISPATCH_TIME_NOW,
                              (int64_t)(request.timeoutMs * NSEC_PER_MSEC)),
                dispatch_get_global_queue(QOS_CLASS_UTILITY, 0),
                ^{
                  [delegate
                      stopTask:taskIdentifier
                         error:NetError{NetErrorCode::Timeout,
                                        "no response before the timeout"}];
                });
            [task resume];
        }
    }

    void abort(RequestId id) override { [m_delegate cancelRequest:id]; }

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
