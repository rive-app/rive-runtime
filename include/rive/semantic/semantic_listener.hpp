#ifndef _RIVE_SEMANTIC_LISTENER_HPP_
#define _RIVE_SEMANTIC_LISTENER_HPP_

namespace rive
{

/// Interface for objects that want to be notified of semantic actions on a
/// SemanticData.
class SemanticListener
{
public:
    virtual ~SemanticListener() = default;

    /// Called when a semantic tap/activate action is fired.
    virtual void onSemanticTap() = 0;

    /// Called when a semantic increase action is fired (e.g. slider step up).
    virtual void onSemanticIncrease() = 0;

    /// Called when a semantic decrease action is fired (e.g. slider step down).
    virtual void onSemanticDecrease() = 0;
};

} // namespace rive

#endif
