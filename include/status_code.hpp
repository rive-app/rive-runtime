#ifndef _RIVE_STATUS_CODE_HPP_
#define _RIVE_STATUS_CODE_HPP_
namespace rive {
    enum class StatusCode : unsigned char {
        Ok,
        MissingObject,
        InvalidObject,
        FailedInversion
    };
}
#endif