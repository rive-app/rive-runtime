#ifndef _RIVE_KEYED_CALLBACK_REPORTER_HPP_
#define _RIVE_KEYED_CALLBACK_REPORTER_HPP_

namespace rive
{
class KeyedCallbackReporter
{
public:
    virtual ~KeyedCallbackReporter() {}
    virtual void reportKeyedCallback(uint32_t objectId,
                                     uint32_t propertyKey,
                                     float elapsedSeconds) = 0;
};
} // namespace rive

#endif