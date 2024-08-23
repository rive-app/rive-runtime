#pragma once

#include "rive/rive_types.hpp"

#ifdef RIVE_BUILD_FOR_APPLE

#if defined(RIVE_BUILD_FOR_OSX)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(RIVE_BUILD_FOR_IOS)
#include <CoreFoundation/CoreFoundation.h>
#endif

template <typename T> class AutoCF
{
    T m_obj;

public:
    AutoCF(T obj = nullptr) : m_obj(obj) {}
    AutoCF(const AutoCF& other)
    {
        if (other.m_obj)
        {
            CFRetain(other.m_obj);
        }
        m_obj = other.m_obj;
    }
    AutoCF(AutoCF&& other)
    {
        m_obj = other.m_obj;
        other.m_obj = nullptr;
    }
    ~AutoCF()
    {
        if (m_obj)
        {
            CFRelease(m_obj);
        }
    }

    AutoCF& operator=(const AutoCF& other)
    {
        if (m_obj != other.m_obj)
        {
            if (other.m_obj)
            {
                CFRetain(other.m_obj);
            }
            if (m_obj)
            {
                CFRelease(m_obj);
            }
            m_obj = other.m_obj;
        }
        return *this;
    }

    void reset(T obj)
    {
        if (obj != m_obj)
        {
            if (m_obj)
            {
                CFRelease(m_obj);
            }
            m_obj = obj;
        }
    }

    operator T() const { return m_obj; }
    operator bool() const { return m_obj != nullptr; }
    T get() const { return m_obj; }
};

#endif // RIVE_BUILD_FOR_APPLE
