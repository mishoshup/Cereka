#pragma once

#include <string>

namespace cereka {

class DialogueSystem {
   public:
    void Show(std::string speaker, std::string name, std::string text);
    void Tick(float dt);
    void Clear();

    const std::string &Speaker() const { return speaker; }
    const std::string &Name() const { return name; }
    const std::string &Text() const { return text; }
    int DisplayedChars() const { return displayedChars; }

    /// Text speed in characters per second (default 60.0)
    void SetTextSpeed(float cps) { charsPerSecond = cps; }
    float TextSpeed() const { return charsPerSecond; }

    // Mutators for save/load round-trip.
    void SetSpeaker(std::string s) { speaker = std::move(s); }
    void SetName(std::string s) { name = std::move(s); }
    void SetText(std::string s) { text = std::move(s); }
    void SetDisplayedChars(int n) { displayedChars = n; }

   private:
    float charsPerSecond = 60.0f;

    std::string speaker;
    std::string name;
    std::string text;
    float typewriterTimer = 0.0f;
    int displayedChars = 0;
};

}  // namespace cereka
