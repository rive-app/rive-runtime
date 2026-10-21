#ifndef _RIVE_TEXT_STYLE_SHARE_CACHE_HPP_
#define _RIVE_TEXT_STYLE_SHARE_CACHE_HPP_
#include "rive/refcnt.hpp"
#include "rive/text/text_style.hpp"
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rive
{
class Artboard;
class ArtboardInstance;
class Factory;
class File;
class TextInterface;
class TextStylePaint;
class TextStyleShareCache;
class ViewModelInstance;

/// What a File knows about one of its global text style artboards.
struct TextStyleShareInfo
{
  /// Every Text using the style can draw the same instance: the style has no
  /// state machine logic, no exposed properties, only plain paints, and binds
  /// that read nothing but its own view model, so its values never change.
  bool shareable = false;
  /// Some bind reads past the style's own view model (the hosting
  /// artboard's data, or globals), so it must follow its host's context.
  bool readsOutsideOwnViewModel = false;
  /// The style's state machine has pointer listeners with no target: they
  /// listen to the runs the style is drawn for, all the runs sharing an
  /// instance as one target (see StateMachineInstance::serveTextStyle).
  bool listensToRuns = false;
  /// The style's default state machine has logic (states, listeners or
  /// binds), so an instance keeps state of its own: each list item a Text
  /// draws with the style gets its own instance (see
  /// Text::globalStyleReference).
  bool hasStateMachine = false;
};

/// The single instance of a shareable text style, drawn by every Text that
/// uses it. Built once (its values are static) and never advanced.
class SharedTextStyle : public TextStyleOwner
{
public:
  SharedTextStyle(const Artboard* source, File* file, Factory* factory);
  ~SharedTextStyle() override;

  TextStylePaint* paint() const { return m_paint; }
  ArtboardInstance* artboardInstance() const { return m_instance.get(); }

  // TextStyleOwner: a font arriving late re-shapes every consumer.
  void styleShapeChanged() override;

private:
  friend class TextStyleShareCache;
  friend class SharedTextStyleLease;
  std::unique_ptr<ArtboardInstance> m_instance;
  rcp<ViewModelInstance> m_viewModelInstance;
  TextStylePaint* m_paint = nullptr;
  std::vector<TextInterface*> m_consumers;
};

/// What a lease reaches its cache through. A File can go before every lease
/// does: an ArtboardInstance holding the File's last reference releases it
/// before deleting its components, and with them their leases. The cache
/// clears [cache] as it goes, leaving those leases with nothing to release.
class TextStyleShareLink : public RefCnt<TextStyleShareLink>
{
public:
  explicit TextStyleShareLink(TextStyleShareCache* value) : cache(value) {}
  TextStyleShareCache* cache;
};

/// A consumer's claim on a [SharedTextStyle]; the last one to go frees it.
class SharedTextStyleLease
{
public:
  SharedTextStyleLease() = default;
  SharedTextStyleLease(
      rcp<TextStyleShareLink> link,
      SharedTextStyle* style,
      TextInterface* consumer)
      : m_link(std::move(link))
      , m_style(style)
      , m_consumer(consumer)
  {}
  SharedTextStyleLease(SharedTextStyleLease&& other) noexcept;
  SharedTextStyleLease& operator=(SharedTextStyleLease&& other) noexcept;
  SharedTextStyleLease(const SharedTextStyleLease&) = delete;
  SharedTextStyleLease& operator=(const SharedTextStyleLease&) = delete;
  ~SharedTextStyleLease() { release(); }

  /// Null once released, or once its cache is gone (with the style).
  SharedTextStyle* get() const
  {
    return m_link != nullptr && m_link->cache != nullptr ? m_style : nullptr;
  }
  explicit operator bool() const { return get() != nullptr; }
  void release();

private:
  rcp<TextStyleShareLink> m_link;
  SharedTextStyle* m_style = nullptr;
  TextInterface* m_consumer = nullptr;
};

/// Decides which of a File's text styles can be shared, and holds the shared
/// instances. Owned by the File; leases may outlive it (see
/// [TextStyleShareLink]).
class TextStyleShareCache
{
public:
  explicit TextStyleShareCache(File* file)
      : m_file(file)
      , m_link(make_rcp<TextStyleShareLink>(this))
  {}
  ~TextStyleShareCache();

  /// Analyzes [source] (a file-owned text style artboard) once.
  TextStyleShareInfo info(const Artboard* source);

  /// The shared instance of [source] drawn with [factory], for [consumer].
  /// Null when [source] isn't shareable.
  SharedTextStyleLease acquire(
      const Artboard* source,
      Factory* factory,
      TextInterface* consumer);

  /// Analyzes [source] owned by [file] without caching.
  static TextStyleShareInfo analyze(const Artboard* source, const File* file);

  /// Whether any of the file's text styles listens to its runs. Texts only
  /// forward pointer events to their runs' styles when one may.
  bool anyListensToRuns();

  /// Drops what's known about [source] (an artboard being replaced).
  void forget(const Artboard* source);

#ifdef TESTING
  size_t liveCount() const { return m_shared.size(); }
#endif

private:
  friend class SharedTextStyleLease;
  void release(SharedTextStyle* style, TextInterface* consumer);

  File* m_file;
  rcp<TextStyleShareLink> m_link;
  std::mutex m_mutex;
  std::unordered_map<const Artboard*, TextStyleShareInfo> m_info;
  // -1 until computed.
  int m_anyListensToRuns = -1;
  std::map<
      std::pair<const Artboard*, Factory*>,
      std::unique_ptr<SharedTextStyle>>
      m_shared;
};
} // namespace rive

#endif
