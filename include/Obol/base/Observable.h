#ifndef OBOL_BASE_OBSERVABLE_H
#define OBOL_BASE_OBSERVABLE_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
\**************************************************************************/

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace obol {

using ObserverId = uint64_t;

template <typename T>
struct ValueChange {
    T previous;
    T value;
    std::string fieldName;
};

template <typename T>
class ObservableValue {
public:
    using Callback = std::function<void(const ValueChange<T> &)>;

    ObservableValue() = default;

    explicit ObservableValue(T value)
        : value_(std::move(value))
    {
    }

    const T & get() const
    {
        return value_;
    }

    void set(T value, std::string fieldName = std::string())
    {
        ValueChange<T> change;
        change.previous = value_;
        change.value = value;
        change.fieldName = std::move(fieldName);
        value_ = std::move(value);
        notify(change);
    }

    ObserverId addObserver(Callback callback)
    {
        if (!callback) {
            return 0;
        }
        const ObserverId id = nextObserverId_++;
        observers_.push_back({id, std::move(callback)});
        return id;
    }

    bool removeObserver(ObserverId id)
    {
        for (auto it = observers_.begin(); it != observers_.end(); ++it) {
            if (it->id == id) {
                observers_.erase(it);
                return true;
            }
        }
        return false;
    }

    std::size_t observerCount() const
    {
        return observers_.size();
    }

private:
    struct Observer {
        ObserverId id = 0;
        Callback callback;
    };

    void notify(const ValueChange<T> & change)
    {
        const std::vector<Observer> observers = observers_;
        for (const Observer & observer : observers) {
            observer.callback(change);
        }
    }

    T value_{};
    ObserverId nextObserverId_ = 1;
    std::vector<Observer> observers_;
};

} // namespace obol

#endif // OBOL_BASE_OBSERVABLE_H
