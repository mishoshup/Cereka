#include "dialogue_system.hpp"

namespace cereka {

void DialogueSystem::Show(std::string speaker_, std::string name_, std::string text_)
{
    speaker = std::move(speaker_);
    name = std::move(name_);
    text = std::move(text_);
    typewriterTimer = 0.0f;
    displayedChars = 0;
}

void DialogueSystem::Tick(float dt)
{
    if (text.empty() || displayedChars >= (int)text.length())
        return;
    typewriterTimer += dt;
    int charsToAdd = (int)(typewriterTimer * CHARS_PER_SECOND);
    if (charsToAdd <= 0)
        return;
    displayedChars += charsToAdd;
    typewriterTimer -= charsToAdd / CHARS_PER_SECOND;
    if (displayedChars > (int)text.length())
        displayedChars = (int)text.length();
}

void DialogueSystem::Clear()
{
    speaker.clear();
    name.clear();
    text.clear();
    typewriterTimer = 0.0f;
    displayedChars = 0;
}

}  // namespace cereka
