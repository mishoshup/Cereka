#include "cereka_rollback_manager.hpp"
#include "cereka_engine_impl.hpp"
#include <algorithm>

namespace cereka {

void RollbackManager::setCapacity(size_t cap)
{
    capacity_ = cap;
    buffer_.resize(cap);
    if (cap == 0) {
        enabled_ = false;
        clear();
    } else {
        enabled_ = true;
        if (count_ > cap) count_ = cap;
    }
}

void RollbackManager::capture(const CerekaImpl &impl)
{
    if (capacity_ == 0 || !enabled_) return;

    auto &slot = buffer_[head_];
    slot = {};
    const auto &si = impl.scriptInterpreter;
    const auto &scene = impl.scene;
    const auto &dialogue = impl.dialogue;

    slot.programCounter = si.pc;
    slot.callStack = si.callStack;
    slot.variables = si.variables;
    slot.numVariables = si.numVariables;
    slot.skipMode = si.skipMode;
    slot.skipDepth = si.skipDepth;

    slot.background = scene.BgPath();
    slot.characters.clear();
    for (const auto &[id, entry] : scene.Characters()) {
        auto &ch = slot.characters.emplace_back();
        ch.id = id;
        auto it = scene.CharPaths().find(id);
        ch.file = (it != scene.CharPaths().end()) ? it->second : "";
        ch.position = "center";
    }

    slot.bgm = impl.audio.BgmPath();

    slot.speaker = dialogue.Speaker();
    slot.name = dialogue.Name();
    slot.text = dialogue.Text();
    slot.displayedChars = dialogue.DisplayedChars();

    slot.state = CerekaStateMachine::stateLabel(si.scriptFinished
        ? CerekaState::Finished
        : (impl.m_stateMachine.hasOverlays()
            ? impl.m_stateMachine.effectiveState()
            : impl.m_stateMachine.currentType()));

    dialogueTexts_.resize(buffer_.size());
    dialogueTexts_[head_] = dialogue.Text();

    head_ = (head_ + 1) % capacity_;
    if (count_ < capacity_) ++count_;
}

bool RollbackManager::restore(CerekaImpl &impl)
{
    if (count_ == 0) return false;
    return goTo(impl, prevIndex());
}

bool RollbackManager::goTo(CerekaImpl &impl, size_t index)
{
    if (index >= capacity_ || count_ == 0) return false;

    size_t bufIdx;
    if (count_ < capacity_) {
        bufIdx = index;
    } else {
        bufIdx = (head_ - 1 - index + capacity_) % capacity_;
    }

    const auto &data = buffer_[bufIdx];
    auto &si = impl.scriptInterpreter;
    if (data.programCounter >= si.program.size())
        return false;

    si.pc = data.programCounter;
    si.callStack = data.callStack;
    if (si.callStack.size() > 32)
        si.callStack.resize(32);

    si.variables = data.variables;
    si.numVariables = data.numVariables;
    si.skipMode = data.skipMode;
    si.skipDepth = data.skipDepth;
    si.scriptFinished = false;

    if (!data.background.empty())
        impl.scene.ShowBackground(data.background);
    else
        impl.scene.Clear();

    for (const auto &ch : data.characters)
        impl.scene.ShowCharacter(ch.id, ch.file, ch.position);

    if (!data.bgm.empty())
        impl.audio.PlayBGM(data.bgm);

    impl.dialogue.SetText(data.text);
    impl.dialogue.SetDisplayedChars(data.displayedChars);
    if (!data.speaker.empty()) {
        impl.dialogue.SetSpeaker(data.speaker);
        impl.dialogue.SetName(data.name);
    } else {
        impl.dialogue.Clear();
    }

    impl.m_stateMachine.clearOverlays();
    CerekaState savedState = CerekaState::Running;
    std::string st = data.state;
    if (st == "WaitingForInput") savedState = CerekaState::WaitingForInput;
    else if (st == "InMenu") savedState = CerekaState::InMenu;
    else if (st == "Fading") savedState = CerekaState::Fading;
    else if (st == "Finished") savedState = CerekaState::Finished;
    impl.m_stateMachine.changeState(savedState);

    return true;
}

std::vector<std::string> RollbackManager::historyTexts() const
{
    std::vector<std::string> texts;
    texts.reserve(count_);

    if (count_ < capacity_) {
        for (size_t i = 0; i < count_; ++i)
            texts.push_back(dialogueTexts_[i]);
    } else {
        for (size_t i = 0; i < capacity_; ++i) {
            size_t idx = (head_ + i) % capacity_;
            texts.push_back(dialogueTexts_[idx]);
        }
    }
    std::reverse(texts.begin(), texts.end());
    return texts;
}

size_t RollbackManager::prevIndex() const
{
    if (count_ == 0) return 0;
    return (head_ - 1 + capacity_) % capacity_;
}

}  // namespace cereka
