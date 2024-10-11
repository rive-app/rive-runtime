#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

// Copied from
// https://morestina.net/blog/1400/minimalistic-blocking-bounded-queue-for-c
template <typename T> class queue
{
    std::deque<T> content;
    size_t capacity;

    std::mutex mutex;
    std::condition_variable not_empty;
    std::condition_variable not_full;

    queue(const queue&) = delete;
    queue(queue&&) = delete;
    queue& operator=(const queue&) = delete;
    queue& operator=(queue&&) = delete;

public:
    queue(size_t capacity) : capacity(capacity) {}

    void push(T&& item)
    {
        {
            std::unique_lock<std::mutex> lk(mutex);
            not_full.wait(lk, [this]() { return content.size() < capacity; });
            content.push_back(std::move(item));
        }
        not_empty.notify_one();
    }

    bool try_push(T&& item)
    {
        {
            std::unique_lock<std::mutex> lk(mutex);
            if (content.size() == capacity)
                return false;
            content.push_back(std::move(item));
        }
        not_empty.notify_one();
        return true;
    }

    void pop(T& item)
    {
        {
            std::unique_lock<std::mutex> lk(mutex);
            not_empty.wait(lk, [this]() { return !content.empty(); });
            item = std::move(content.front());
            content.pop_front();
        }
        not_full.notify_one();
    }

    bool try_pop(T& item)
    {
        {
            std::unique_lock<std::mutex> lk(mutex);
            if (content.empty())
                return false;
            item = std::move(content.front());
            content.pop_front();
        }
        not_full.notify_one();
        return true;
    }
};
