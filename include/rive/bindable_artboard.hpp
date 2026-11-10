#ifndef _RIVE_BINDABLE_ARTBOARD_HPP_
#define _RIVE_BINDABLE_ARTBOARD_HPP_
#include "rive/refcnt.hpp"
#include "rive/file.hpp"
#include "rive/artboard.hpp"

namespace rive
{

class BindableArtboard : public RefCnt<BindableArtboard>
{
public:
  BindableArtboard(
      rcp<const File> file,
      std::unique_ptr<ArtboardInstance> artboard);
  /// Over an artboard its file owns, with no instance of its own: the host
  /// mounting it instances it then, as it does an authored artboard id. A
  /// script's bindable of its own file is made this way, since instancing
  /// at lookup would run that artboard's scripts' init inside the lookup,
  /// and so re-enter the script asking, or two artboards asking for each
  /// other, without end.
  explicit BindableArtboard(Artboard* source);
  /// The instance held, or null for a bindable over a source artboard.
  ArtboardInstance* artboard() { return m_artboard.get(); }
  /// The artboard a host instances when mounting: the instance held, else
  /// the source. Never null.
  Artboard* source() const
  {
    return m_artboard != nullptr ? m_artboard.get() : m_source;
  }
  /// The file the artboard comes from; null when the creator did not pin
  /// one.
  const File* file() const { return m_file.get(); }

private:
  rcp<const File> m_file;
  std::unique_ptr<ArtboardInstance> m_artboard;
  Artboard* m_source = nullptr;
};
} // namespace rive

#endif
