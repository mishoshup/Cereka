#pragma once

#include <string>
#include <vector>

namespace cereka {

class CerekaEngine;

class CerekaTest {
   public:
    explicit CerekaTest(CerekaEngine &engine);

    int Run(const std::string &specFile);

    std::vector<std::string> ButtonLabels() const;
    bool SelectMenuOption(int idx);

   private:
    struct Command {
        enum Type { Wait, ClickButton, Assert };
        Type type;
        std::string text;
        int index = 0;
        int timeoutSec = 5;
    };

    std::vector<Command> parseSpec(const std::string &path);
    int execute(const std::vector<Command> &commands);

    CerekaEngine &engine_;
};

}  // namespace cereka
