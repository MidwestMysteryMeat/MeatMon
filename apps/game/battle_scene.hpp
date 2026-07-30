#pragma once

// In-window battle scene: a pure *view* over the battle protocol log plus a
// choice UI driven by the sim's Request objects. The scene never mutates
// battle internals directly — it submits Choices exactly like a network
// client would (ARCHITECTURE.md section 8).

#include <meatmon/assets.hpp>
#include <meatmon/font.hpp>
#include <meatmon/sprites.hpp>
#include <meatmon/battle/battle.hpp>

#include <SDL3/SDL.h>

#include <deque>
#include <memory>
#include <string>

class BattleScene {
public:
    BattleScene(mm::AssetManager& assets, mm::SpriteLibrary& sprites,
                const mm::battle::Dex& dex, bool autoplay = false);

    // wild = true enables the catch option; foeTeam[0] is the wild monster.
    void start(uint64_t seed,
               std::string playerName, std::vector<mm::battle::MonsterSet> playerTeam,
               std::string foeName, std::vector<mm::battle::MonsterSet> foeTeam,
               bool wild = false);
    void handleEvent(const SDL_Event& ev);
    void update();
    void render(SDL_Renderer* r, const mm::Font& font);

    bool finished() const { return done_; }
    bool playerWon() const { return battle_ && battle_->winner() == 0; }
    const mm::battle::Battle* battle() const { return battle_.get(); }
    bool caught() const { return caught_; }
    const mm::battle::MonsterSet& caughtSet() const { return caughtSet_; }
    bool sawMove() const { return sawMove_; }   // selftest probe

private:
    enum class Ui { Message, MoveMenu, SwitchMenu };

    void pump();                                // humanize new log lines
    void pushHuman(const std::string& line);
    void refreshSlots();
    void autoChooseFoe();
    void tryCommit();
    void submitMove(int idx);
    void submitSwitch(int listIdx);
    void attemptCatch();

    struct Slot {
        std::string name;
        int hp = 0, maxhp = 1;
        float shown = 0;                        // animated HP bar value
        std::string status;
        mm::Texture* sprite = nullptr;
    };

    mm::AssetManager& assets_;
    mm::SpriteLibrary& sprites_;
    const mm::battle::Dex& dex_;
    bool auto_ = false;

    std::unique_ptr<mm::battle::Battle> battle_;
    size_t cursor_ = 0;
    std::deque<std::string> msgs_;
    std::string current_;
    int msgTimer_ = 0;
    Ui ui_ = Ui::Message;
    int sel_ = 0;
    bool done_ = false;
    bool sawMove_ = false;
    std::string foeName_ = "Foe";
    bool wild_ = false;
    bool caught_ = false;
    bool forceEnd_ = false;                     // catch success ends the fight
    mm::battle::MonsterSet wildTemplate_;
    mm::battle::MonsterSet caughtSet_;
    mm::battle::Prng catchRng_{1};              // app-side RNG, not the sim's
    Slot slots_[2];                             // 0 = player, 1 = foe
};
