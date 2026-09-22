/*
 * Copyright 2026 Rive
 */

#if defined(RIVE_IOS) || defined(RIVE_IOS_SIMULATOR)

#include "common/rive_ios_app.hpp"

#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

static std::mutex s_windowMutex;
static std::condition_variable s_windowReady;
static CAMetalLayer* s_metalLayer = nil;

static std::mutex s_inputMutex;
static std::queue<TestingWindow::InputEventData> s_inputEvents;

static std::mutex s_lifecycleMutex;
static std::condition_variable s_lifecycleChanged;
static bool s_active = true;
static std::atomic<bool> s_shouldQuit{false};

static void push_touch_event(UIView* view,
                             UITouch* touch,
                             TestingWindow::InputEvent eventType)
{
    if (touch == nil)
    {
        return;
    }
    CGPoint location = [touch locationInView:view];
    CGFloat scale = view.contentScaleFactor;
    std::lock_guard<std::mutex> lock(s_inputMutex);
    s_inputEvents.push(
        TestingWindow::InputEventData(eventType,
                                      static_cast<float>(location.x * scale),
                                      static_cast<float>(location.y * scale)));
}

static void set_active(bool active)
{
    std::lock_guard<std::mutex> lock(s_lifecycleMutex);
    s_active = active;
    s_lifecycleChanged.notify_all();
}

@interface RiveMetalView : UIView
@end

@implementation RiveMetalView

+ (Class)layerClass
{
    return [CAMetalLayer class];
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    push_touch_event(
        self, touches.anyObject, TestingWindow::InputEvent::MouseMove);
    push_touch_event(
        self, touches.anyObject, TestingWindow::InputEvent::MouseDown);
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    push_touch_event(
        self, touches.anyObject, TestingWindow::InputEvent::MouseMove);
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    push_touch_event(
        self, touches.anyObject, TestingWindow::InputEvent::MouseUp);
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    push_touch_event(
        self, touches.anyObject, TestingWindow::InputEvent::MouseUp);
}

@end

@interface GameViewController : UIViewController
@end

@implementation GameViewController

- (void)loadView
{
    self.view =
        [[RiveMetalView alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.view.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];

    if (![self.view.layer isKindOfClass:[CAMetalLayer class]])
    {
        return;
    }

    UIScreen* screen = self.view.window.screen ?: UIScreen.mainScreen;
    CGFloat scale = screen.scale;
    self.view.contentScaleFactor = scale;

    CAMetalLayer* layer = (CAMetalLayer*)self.view.layer;
    CGSize bounds = self.view.bounds.size;
    layer.contentsScale = scale;
    layer.drawableSize =
        CGSizeMake(bounds.width * scale, bounds.height * scale);

    std::lock_guard<std::mutex> lock(s_windowMutex);
    if (s_metalLayer == nil)
    {
        s_metalLayer = layer;
        s_windowReady.notify_all();
    }
}

@end

@interface RiveAppDelegate : UIResponder <UIApplicationDelegate>
@property(strong, nonatomic) UIWindow* window;
@end

@implementation RiveAppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[GameViewController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}

- (void)applicationDidBecomeActive:(UIApplication*)application
{
    set_active(true);
}

- (void)applicationWillResignActive:(UIApplication*)application
{
    set_active(false);
}

- (void)applicationWillTerminate:(UIApplication*)application
{
    s_shouldQuit = true;
    set_active(true);
}

@end

void* rive_ios_app_wait_for_window()
{
    std::unique_lock<std::mutex> lock(s_windowMutex);
    s_windowReady.wait(lock, [] { return s_metalLayer != nil; });
    return (__bridge void*)s_metalLayer;
}

bool rive_ios_app_poll_input_event(TestingWindow::InputEventData& eventData)
{
    std::lock_guard<std::mutex> lock(s_inputMutex);
    if (s_inputEvents.empty())
    {
        return false;
    }
    eventData = s_inputEvents.front();
    s_inputEvents.pop();
    return true;
}

bool rive_ios_app_should_quit() { return s_shouldQuit; }

void rive_ios_app_wait_while_inactive()
{
    std::unique_lock<std::mutex> lock(s_lifecycleMutex);
    s_lifecycleChanged.wait(lock, [] { return s_active || s_shouldQuit; });
}

#endif
