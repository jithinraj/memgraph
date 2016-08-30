#pragma once

#include "utils/iterator/count.hpp"
#include "utils/option.hpp"

class EdgeType;

namespace iter
{
template <class I, class OP>
auto make_map(I &&iter, OP &&op);

template <class I, class OP>
auto make_filter(I &&iter, OP &&op);

template <class I, class C>
void for_all(I &&iter, C &&consumer);

template <class I, class OP>
auto make_flat_map(I &&iter, OP &&op);

template <class I, class OP>
auto make_inspect(I &&iter, OP &&op);

template <class I, class OP>
auto make_limited_map(I &&iter, OP &&op);
}

// Base iterator for next() kind iterator.
// T - type of return value
template <class T>
class IteratorBase
{
public:
    virtual ~IteratorBase(){};

    virtual Option<T> next() = 0;

    virtual Count count() { return Count(0, ~((size_t)0)); }

    template <class OP>
    auto map(OP &&op)
    {
        return iter::make_map<decltype(std::move(*this)), OP>(std::move(*this),
                                                              std::move(op));
    }

    template <class OP>
    auto filter(OP &&op)
    {
        return iter::make_filter<decltype(std::move(*this)), OP>(
            std::move(*this), std::move(op));
    }

    // Replaces every item with item taken from n if it exists.
    template <class R>
    auto replace(Option<R> &n)
    {
        return iter::make_limited_map<decltype(std::move(*this))>(
            std::move(*this), [&](auto v) mutable { return std::move(n); });
    }

    // Maps with call to method to() and filters with call to fill.
    auto to()
    {
        return map([](auto er) { return er.to(); }).fill();
    }

    // Maps with call to method from() and filters with call to fill.
    auto from()
    {
        return map([](auto er) { return er.from(); }).fill();
    }

    // Combines out iterators into one iterator.
    auto out()
    {
        return iter::make_flat_map<decltype(std::move(*this))>(
            std::move(*this), [](auto vr) { return vr.out().fill(); });
    }

    // Filters with label on from vertex.
    template <class LABEL>
    auto from_label(LABEL const &label)
    {
        return filter([&](auto &ra) {
            auto va = ra.from();
            return va.fill() && va.has_label(label);
        });
    }

    // Filters with property under given key
    template <class KEY, class PROP>
    auto has_property(KEY &key, PROP const &prop)
    {
        return filter([&](auto &va) { return va.at(key) == prop; });
    }

    // Copy-s all pasing value to t before they are returned.
    // auto clone_to(Option<T> &t)
    // {
    //     return iter::make_inspect<decltype(std::move(*this))>(
    //         std::move(*this), [&](auto &v) { t = Option<T>(v); });
    // }

    // Copy-s pasing value to t before they are returned.
    auto clone_to(Option<const T> &t)
    {
        return iter::make_inspect<decltype(std::move(*this))>(
            std::move(*this), [&](auto &v) mutable { t = Option<const T>(v); });
    }

    // Filters with call to method fill()
    auto fill()
    {
        return filter([](auto &ra) { return ra.fill(); });
    }

    // Filters with type
    template <class TYPE>
    auto type(TYPE const &type)
    {
        return filter([&](auto &ra) { return ra.edge_type() == type; });
    }

    // Filters with label.
    template <class LABEL>
    auto label(LABEL const &label)
    {
        return filter([&](auto &va) { return va.has_label(label); });
    }

    // Filters out vertices which are connected.
    auto isolated()
    {
        return filter([&](auto &ra) { return ra.isolated(); });
    }

    // For all items calls OP.
    template <class OP>
    void for_all(OP &&op)
    {
        iter::for_all(std::move(*this), std::move(op));
    }
};
