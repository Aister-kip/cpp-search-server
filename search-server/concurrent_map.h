#pragma once

#include <vector>
#include <map>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        Access() = default;
        Access(Value& value, std::unique_lock<std::mutex>&& mutex_value)
            : ref_to_value(value)
            , guard(std::move(mutex_value))
        {}
        Value& ref_to_value;
        std::unique_lock<std::mutex> guard;
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count)
        , mutexes_(bucket_count)
    {};

    Access operator[](const Key& key) {
        size_t bucket_number = static_cast<uint64_t>(key) % buckets_.size();
        std::unique_lock locker(mutexes_[bucket_number]);
        return Access(buckets_[bucket_number][key], std::move(locker));
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (size_t i = 0; i < buckets_.size(); ++i) {
            mutexes_[i].lock();
            result.insert(buckets_[i].begin(), buckets_[i].end());
            mutexes_[i].unlock();
        }
        return result;
    }

    void erase(const Key& key) {
        size_t bucket_number = static_cast<uint64_t>(key) % buckets_.size();
        mutexes_[bucket_number].lock();
        buckets_[bucket_number].erase(key);
        mutexes_[bucket_number].unlock();
    }

private:
    std::vector<std::map<Key, Value>> buckets_;
    std::vector<std::mutex> mutexes_;

};