#pragma once
// A deliberately small entity-component system.
//
//  * Entity = 32-bit id: 20 bits of slot index + 12 bits of generation. Destroyed slots go on a
//    free list and are reused with a bumped generation, so stale handles are detected instead of
//    silently pointing at a new object. This is the object pool for platforms: after warm-up,
//    streaming platforms in and out of the tower allocates nothing.
//  * Each component type lives in its own SparseSet: a dense, contiguous array of components plus
//    a sparse index from entity slot to dense position. Iteration is a linear walk over memory;
//    add/remove/lookup are O(1); removal swaps the last element into the hole.

#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tj {

using Entity = uint32_t;
constexpr Entity kNullEntity = std::numeric_limits<Entity>::max();

namespace ecs_detail {
constexpr uint32_t kIndexBits = 20;
constexpr uint32_t kIndexMask = (1u << kIndexBits) - 1u;
constexpr uint32_t kGenerationMask = (1u << (32 - kIndexBits)) - 1u;
inline uint32_t indexOf(Entity e) { return e & kIndexMask; }
inline uint32_t generationOf(Entity e) { return e >> kIndexBits; }
inline Entity make(uint32_t index, uint32_t generation) { return (generation << kIndexBits) | index; }
}  // namespace ecs_detail

class ISparseSet {
public:
    virtual ~ISparseSet() = default;
    virtual void remove(Entity e) = 0;
    virtual bool contains(Entity e) const = 0;
};

template <typename T>
class SparseSet final : public ISparseSet {
public:
    void reserve(size_t n) {
        dense_.reserve(n);
        owners_.reserve(n);
    }

    template <typename... Args>
    T& emplace(Entity e, Args&&... args) {
        const uint32_t i = ecs_detail::indexOf(e);
        if (i >= sparse_.size()) sparse_.resize(i + 1, kAbsent);
        assert(sparse_[i] == kAbsent && "component already present");
        sparse_[i] = static_cast<uint32_t>(dense_.size());
        owners_.push_back(e);
        dense_.push_back(T{std::forward<Args>(args)...});
        return dense_.back();
    }

    void remove(Entity e) override {
        if (!contains(e)) return;
        const uint32_t i = ecs_detail::indexOf(e);
        const uint32_t pos = sparse_[i];
        const uint32_t last = static_cast<uint32_t>(dense_.size() - 1);
        if (pos != last) {
            dense_[pos] = std::move(dense_[last]);
            owners_[pos] = owners_[last];
            sparse_[ecs_detail::indexOf(owners_[pos])] = pos;
        }
        dense_.pop_back();
        owners_.pop_back();
        sparse_[i] = kAbsent;
    }

    bool contains(Entity e) const override {
        const uint32_t i = ecs_detail::indexOf(e);
        return i < sparse_.size() && sparse_[i] != kAbsent && owners_[sparse_[i]] == e;
    }

    T& get(Entity e) {
        assert(contains(e));
        return dense_[sparse_[ecs_detail::indexOf(e)]];
    }
    T* tryGet(Entity e) { return contains(e) ? &dense_[sparse_[ecs_detail::indexOf(e)]] : nullptr; }

    size_t size() const { return dense_.size(); }
    size_t capacity() const { return dense_.capacity(); }
    const std::vector<Entity>& entities() const { return owners_; }
    std::vector<T>& components() { return dense_; }

private:
    static constexpr uint32_t kAbsent = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> sparse_;
    std::vector<Entity> owners_;  // owners_[k] is the entity that owns dense_[k]
    std::vector<T> dense_;
};

class Registry {
public:
    Entity create() {
        if (!freeList_.empty()) {
            const uint32_t i = freeList_.back();
            freeList_.pop_back();
            return ecs_detail::make(i, generations_[i]);
        }
        const uint32_t i = static_cast<uint32_t>(generations_.size());
        assert(i <= ecs_detail::kIndexMask && "too many entities");
        generations_.push_back(0);
        return ecs_detail::make(i, 0);
    }

    void destroy(Entity e) {
        if (!alive(e)) return;
        for (auto& [type, set] : sets_) set->remove(e);
        const uint32_t i = ecs_detail::indexOf(e);
        generations_[i] = (generations_[i] + 1) & ecs_detail::kGenerationMask;
        freeList_.push_back(i);
    }

    bool alive(Entity e) const {
        const uint32_t i = ecs_detail::indexOf(e);
        return e != kNullEntity && i < generations_.size() && generations_[i] == ecs_detail::generationOf(e);
    }

    template <typename T>
    SparseSet<T>& storage() {
        auto it = sets_.find(std::type_index(typeid(T)));
        if (it == sets_.end()) it = sets_.emplace(std::type_index(typeid(T)), std::make_unique<SparseSet<T>>()).first;
        return static_cast<SparseSet<T>&>(*it->second);
    }

    template <typename T, typename... Args>
    T& add(Entity e, Args&&... args) {
        assert(alive(e));
        return storage<T>().emplace(e, std::forward<Args>(args)...);
    }
    template <typename T>
    T& get(Entity e) { return storage<T>().get(e); }
    template <typename T>
    T* tryGet(Entity e) { return storage<T>().tryGet(e); }
    template <typename T>
    bool has(Entity e) { return storage<T>().contains(e); }

    // Visits every entity that has both A and B. Walks the smaller set (by A here, which callers
    // choose to be the rarer component).
    template <typename A, typename B, typename Fn>
    void each(Fn&& fn) {
        SparseSet<A>& a = storage<A>();
        SparseSet<B>& b = storage<B>();
        for (size_t k = 0; k < a.size(); ++k) {
            const Entity e = a.entities()[k];
            if (B* cb = b.tryGet(e)) fn(e, a.components()[k], *cb);
        }
    }

    size_t aliveCount() const { return generations_.size() - freeList_.size(); }
    size_t slotCount() const { return generations_.size(); }

    void clear() {
        sets_.clear();
        generations_.clear();
        freeList_.clear();
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<ISparseSet>> sets_;
    std::vector<uint32_t> generations_;
    std::vector<uint32_t> freeList_;
};

}  // namespace tj
