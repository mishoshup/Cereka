#pragma once
#include "cereka_save_data.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace cereka {

class CerekaImpl;

class RollbackManager {
public:
    explicit RollbackManager(size_t capacity = 200)
        : buffer_(capacity), capacity_(capacity) {}

    void setCapacity(size_t cap);
    void capture(const CerekaImpl &impl);
    bool restore(CerekaImpl &impl);
    bool goTo(CerekaImpl &impl, size_t index);

    std::vector<std::string> historyTexts() const;

    bool canRollback() const { return count_ > 0 && enabled_; }
    void clear() { head_ = count_ = 0; }
    size_t capacity() const { return capacity_; }
    size_t count() const { return count_; }

private:
    std::vector<SerializableSaveData> buffer_;
    size_t capacity_;
    size_t head_ = 0;
    size_t count_ = 0;
    bool enabled_ = true;
    std::vector<std::string> dialogueTexts_;
    size_t prevIndex() const;
};

}  // namespace cereka
