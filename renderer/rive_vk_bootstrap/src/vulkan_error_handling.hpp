#pragma once

#include "logging.hpp"

// Test that the condition is true and, if not, return the given error
// value.
#define CONFIRM_OR_RETURN_VALUE(condition, errorValue)                         \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            return errorValue;                                                 \
        }                                                                      \
    } while (false)

// Test that the condition is true and, if not, returns. Useful for constructors
// which cannot return a value.
#define CONFIRM_OR_RETURN(condition) CONFIRM_OR_RETURN_VALUE(condition, )

// Test that the condition is true and, if not, log an error message and return
// the given error value.
#define CONFIRM_OR_RETURN_VALUE_MSG(condition, errorValue, message, ...)       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            /* C++20 has __VA_OPT__ that would handle this, but since we       \
             aren't on C++20, this is a little lambda trick to not leave a     \
             trailing comma before the arguments in LOG_ERROR_LINE in the      \
             event that __VA_ARGS__ are empty */                               \
            [&](auto&&... args) {                                              \
                LOG_ERROR_LINE(message,                                        \
                               std::forward<decltype(args)>(args)...);         \
            }(__VA_ARGS__);                                                    \
                                                                               \
            return errorValue;                                                 \
        }                                                                      \
    } while (false)

// Test that the condition is true and, if not, log an error message and return.
// Useful for constructors, which cannot return a value.
#define CONFIRM_OR_RETURN_MSG(condition, message, ...)                         \
    CONFIRM_OR_RETURN_VALUE_MSG(condition, , message, __VA_ARGS__)

// If the result of the given expression is not VK_SUCCESS, return its result.
#define VK_RETURN_RESULT_ON_ERROR(x)                                           \
    do                                                                         \
    {                                                                          \
        if (auto vkResult = (x); vkResult != VK_SUCCESS)                       \
        {                                                                      \
            return vkResult;                                                   \
        }                                                                      \
    } while (false)

// If the result of the given expression is not VK_SUCCESS, log an error message
// (containing the failed VkResult) then return its result.
#define VK_RETURN_RESULT_ON_ERROR_MSG(x, message, ...)                         \
    do                                                                         \
    {                                                                          \
        if (auto vkResult = (x); vkResult != VK_SUCCESS)                       \
        {                                                                      \
            [&](auto&&... args) {                                              \
                LOG_VULKAN_ERROR_LINE(vkResult,                                \
                                      message,                                 \
                                      std::forward<decltype(args)>(args)...);  \
            }(__VA_ARGS__);                                                    \
            return vkResult;                                                   \
        }                                                                      \
    } while (false)

// If the result of the given expression is not VK_SUCCESS, return the specified
// failure value.
#define VK_CONFIRM_OR_RETURN_VALUE(x, failureValue)                            \
    do                                                                         \
    {                                                                          \
        if (auto vkResult = (x); vkResult != VK_SUCCESS)                       \
        {                                                                      \
            return failureValue;                                               \
        }                                                                      \
    } while (false)

// If the result of the given expression is not VK_SUCCESS, returns. Useful for
// constructors, which cannot return a value.
#define VK_CONFIRM_OR_RETURN(x) VK_CONFIRM_OR_RETURN_VALUE(x, )

// If the result of the given expression is not VK_SUCCESS, log an error message
// containing the failed VkResult, then return the specified failure value.
#define VK_CONFIRM_OR_RETURN_VALUE_MSG(x, failureValue, message, ...)          \
    do                                                                         \
    {                                                                          \
        if (auto vkResult = (x); vkResult != VK_SUCCESS)                       \
        {                                                                      \
            [&](auto&&... args) {                                              \
                LOG_VULKAN_ERROR_LINE(vkResult,                                \
                                      message,                                 \
                                      std::forward<decltype(args)>(args)...);  \
            }(__VA_ARGS__);                                                    \
            return failureValue;                                               \
        }                                                                      \
    } while (false)

// If the result of the given expression is not VK_SUCCESS, log an error message
// containing the failed VkResult, then return. Useful for constructors, which
// cannot return a value.
#define VK_CONFIRM_OR_RETURN_MSG(x, message, ...)                              \
    VK_CONFIRM_OR_RETURN_VALUE_MSG(x, , message, __VA_ARGS__)

// Define a variable named `name` that the Vulkan function of the same name is
// loaded into. On failure, log an error and return the given error value.
#define DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN_VALUE(name, obj, errorRetVal)  \
    DEFINE_AND_LOAD_INSTANCE_FUNC(name, obj);                                  \
    CONFIRM_OR_RETURN_VALUE_MSG(name != nullptr,                               \
                                errorRetVal,                                   \
                                "Failed to load Vulkan function '" #name "'")

// Define a variable named `name` that the Vulkan function of the same name is
// loaded into. On failure, log an error and return. Useful for constructors,
// which cannot return a value.
#define DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN(name, obj)                     \
    DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN_VALUE(name, obj, )

// Loads the vulkan function `name` into pInstance->m_`name`. On failure, log an
// error and return the given error value.
#define LOAD_MEMBER_INSTANCE_FUNC_OR_RETURN_VALUE(pInstance,                   \
                                                  name,                        \
                                                  obj,                         \
                                                  errorRetVal)                 \
    LOAD_MEMBER_INSTANCE_FUNC(pInstance, name, obj);                           \
    CONFIRM_OR_RETURN_VALUE_MSG(pInstance->m_##name != nullptr,                \
                                errorRetVal,                                   \
                                "Failed to load Vulkan function '" #name "'")

// Loads the vulkan function `name` into pInstance->m_`name`. On failure, log an
// error and return the given error value. Useful for constructors, which cannot
// return a value.
#define LOAD_MEMBER_INSTANCE_FUNC_OR_RETURN(pInstance, name, obj)              \
    LOAD_MEMBER_INSTANCE_FUNC_OR_RETURN_VALUE(pInstance, name, obj, )
