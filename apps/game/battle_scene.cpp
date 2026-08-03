#include "battle_scene.hpp"

#include <algorithm>
#include <vector>

using namespace mm;
using namespace mm::battle;

namespace {

std::vector<std::string> splitBar(const std::string& line) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= line.size()) {
        size_t next = line.find('|', pos);
        if (next == std::string::npos) next = line.size();
        out.push_back(line.substr(pos, next - pos));
        pos = next + 1;
    }
    return out;
}

// "p1a: Nickname" -> "Nickname" / "Foe Nickname"
std::string who(const std::string& tag) {
    size_t c = tag.find(": ");
    std::string name = c == std::string::npos ? tag : tag.substr(c + 2);
    return tag.rfind("p2", 0) == 0 ? "Foe " + name : name;
}

std::string statName(const std::string& s) {
    if (s == "atk") return "Attack";
    if (s == "def") return "Defense";
    if (s == "spa") return "Sp. Atk";
    if (s == "spd") return "Sp. Def";
    if (s == "spe") return "Speed";
    return s;
}

} // namespace

BattleScene::BattleScene(AssetManager& assets, SpriteLibrary& sprites,
                         const Dex& dex, bool autoplay)
    : assets_(assets), sprites_(sprites), dex_(dex), auto_(autoplay) {}

void BattleScene::start(uint64_t seed,
                        std::string playerName, std::vector<MonsterSet> playerTeam,
                        std::string foeName, std::vector<MonsterSet> foeTeam,
                        bool wild) {
    foeName_ = foeName;
    wild_ = wild;
    caught_ = false;
    forceEnd_ = false;
    if (wild && !foeTeam.empty()) wildTemplate_ = foeTeam[0];
    catchRng_ = Prng(seed ^ 0xCA7C42ULL);
    battle_ = std::make_unique<Battle>(dex_, Format{}, seed);
    battle_->setPlayer(0, std::move(playerName), std::move(playerTeam));
    battle_->setPlayer(1, std::move(foeName), std::move(foeTeam));
    battle_->start();
    cursor_ = 0;
    msgs_.clear();
    current_.clear();
    ui_ = Ui::Message;
    done_ = false;
    pump();
    for (auto& s : slots_) s.shown = static_cast<float>(s.hp);
}

void BattleScene::refreshSlots() {
    for (int i = 0; i < 2; ++i) {
        const Side& side = battle_->side(i);
        if (side.active < 0) continue;
        const BattleMonster& m = side.monsters[side.active];
        bool changedMon = slots_[i].name != m.name;
        slots_[i].name = m.name;
        slots_[i].hp = m.hp;
        slots_[i].maxhp = m.stats.hp;
        slots_[i].status = m.status;
        if (changedMon) {
            slots_[i].shown = static_cast<float>(m.hp);
            auto path = sprites_.resolve({.slug = m.species->id, .back = i == 0});
            slots_[i].sprite = assets_.texture(path.string());
        }
    }
}

void BattleScene::pump() {
    const auto& lg = battle_->log();
    for (; cursor_ < lg.size(); ++cursor_) pushHuman(lg[cursor_]);
    refreshSlots();
}

void BattleScene::pushHuman(const std::string& line) {
    auto f = splitBar(line);   // f[0] = "" before the leading '|'
    if (f.size() < 2) return;
    const std::string& t = f[1];
    auto arg = [&](size_t i) { return i < f.size() ? f[i] : std::string(); };

    if (t == "move") {
        sawMove_ = true;
        msgs_.push_back(who(arg(2)) + " used " + arg(3) + "!");
    } else if (t == "switch") {
        if (arg(2).rfind("p1", 0) == 0) {
            msgs_.push_back("Go! " + who(arg(2)) + "!");
        } else if (wild_) {
            msgs_.push_back("A wild " + who(arg(2)).substr(4) + " appeared!");
        } else {
            msgs_.push_back(foeName_ + " sent out " + who(arg(2)).substr(4) + "!");
        }
    } else if (t == "-damage") {
        std::string from = arg(4);
        if (from == "[from] brn") msgs_.push_back(who(arg(2)) + " is hurt by its burn!");
        else if (from == "[from] psn" || from == "[from] tox")
            msgs_.push_back(who(arg(2)) + " is hurt by poison!");
        else if (from == "[from] confusion")
            msgs_.push_back("It hurt itself in its confusion!");
        else if (from == "[from] recoil")
            msgs_.push_back(who(arg(2)) + " is damaged by recoil!");
        // plain damage: the HP bar animation tells the story
    } else if (t == "-heal") {
        std::string from = arg(4);
        size_t item = from.find("item: ");
        if (item != std::string::npos)
            msgs_.push_back(who(arg(2)) + " restored HP with its " +
                            from.substr(item + 6) + "!");
    } else if (t == "-enditem") {
        msgs_.push_back(who(arg(2)) + " ate its " + arg(3) + "!");
    } else if (t == "-ability") {
        msgs_.push_back(who(arg(2)) + "'s " + arg(3) + "!");
    } else if (t == "-supereffective") {
        msgs_.push_back("It's super effective!");
    } else if (t == "-resisted") {
        msgs_.push_back("It's not very effective...");
    } else if (t == "-crit") {
        msgs_.push_back("A critical hit!");
    } else if (t == "-miss") {
        msgs_.push_back("The attack missed!");
    } else if (t == "-immune") {
        msgs_.push_back("It doesn't affect " + who(arg(2)) + "...");
    } else if (t == "faint") {
        msgs_.push_back(who(arg(2)) + " fainted!");
    } else if (t == "-status") {
        std::string s = arg(3), m;
        if (s == "brn") m = " was burned!";
        else if (s == "par") m = " is paralyzed!";
        else if (s == "psn" || s == "tox") m = " was badly poisoned!";
        else if (s == "slp") m = " fell asleep!";
        else if (s == "frz") m = " was frozen solid!";
        msgs_.push_back(who(arg(2)) + m);
    } else if (t == "-curestatus") {
        std::string s = arg(3);
        msgs_.push_back(who(arg(2)) +
                        (s == "slp" ? " woke up!"
                                    : s == "frz" ? " thawed out!" : " recovered!"));
    } else if (t == "cant") {
        std::string s = arg(3), m;
        if (s == "slp") m = " is fast asleep.";
        else if (s == "frz") m = " is frozen solid!";
        else if (s == "par") m = " is paralyzed! It can't move!";
        else if (s == "flinch") m = " flinched and couldn't move!";
        msgs_.push_back(who(arg(2)) + m);
    } else if (t == "-boost" || t == "-unboost") {
        int n = arg(4).empty() ? 1 : std::stoi(arg(4));
        std::string dir = t == "-boost" ? " rose!" : " fell!";
        if (n >= 2) dir = t == "-boost" ? " rose sharply!" : " fell harshly!";
        msgs_.push_back(who(arg(2)) + "'s " + statName(arg(3)) + dir);
    } else if (t == "-start" && arg(3) == "confusion") {
        msgs_.push_back(who(arg(2)) + " became confused!");
    } else if (t == "-activate" && arg(3) == "confusion") {
        msgs_.push_back(who(arg(2)) + " is confused!");
    } else if (t == "-end" && arg(3) == "confusion") {
        msgs_.push_back(who(arg(2)) + " snapped out of confusion!");
    } else if (t == "-fail") {
        msgs_.push_back("But it failed!");
    } else if (t == "win") {
        msgs_.push_back(arg(2) + " won the battle!");
    }
    // |turn| |player| |gametype| |teamsize| |start| are silent
}

void BattleScene::autoChooseFoe() {
    Request req = battle_->request(1);
    if (req.kind == Request::Kind::Move) {
        int idx = 0;
        for (int i = 0; i < static_cast<int>(req.moves.size()); ++i) {
            if (req.moves[i].pp > 0) { idx = i; break; }
        }
        battle_->choose(1, {ChoiceKind::Move, idx});
    } else if (req.kind == Request::Kind::Switch && !req.switches.empty()) {
        battle_->choose(1, {ChoiceKind::Switch, req.switches.front()});
        pump();
    }
}

void BattleScene::tryCommit() {
    if (battle_->allChoicesIn()) {
        battle_->commitTurn();
        pump();
    }
}

void BattleScene::submitMove(int idx) {
    if (battle_->choose(0, {ChoiceKind::Move, idx})) {
        autoChooseFoe();
        tryCommit();
        ui_ = Ui::Message;
    }
}

void BattleScene::submitSwitch(int listIdx) {
    Request req = battle_->request(0);
    if (listIdx < 0 || listIdx >= static_cast<int>(req.switches.size())) return;
    if (battle_->choose(0, {ChoiceKind::Switch, req.switches[listIdx]})) {
        pump();
        ui_ = Ui::Message;
    }
}

void BattleScene::attemptCatch() {
    const auto& wildMon = battle_->side(1).monsters[battle_->side(1).active];
    msgs_.push_back("You threw a Meat Trap!");

    // Classic-style catch value from HP fraction, species rate, and status.
    int maxHp = wildMon.stats.hp;
    int a = (3 * maxHp - 2 * wildMon.hp) * wildMon.species->catchRate / (3 * maxHp);
    if (wildMon.status == "slp" || wildMon.status == "frz") a = a * 2;
    else if (!wildMon.status.empty()) a = a * 3 / 2;
    a = std::clamp(a, 1, 255);

    if (static_cast<int>(catchRng_.next(255)) < a) {
        msgs_.push_back("Gotcha! " + wildMon.name + " was caught!");
        caught_ = true;
        caughtSet_ = wildTemplate_;
        caughtSet_.hp = wildMon.hp;
        caughtSet_.status = wildMon.status;
        forceEnd_ = true;
        ui_ = Ui::Message;
        return;
    }

    msgs_.push_back("Oh no! It broke free!");
    // The throw costs the turn: pass, let the wild monster act.
    battle_->choose(0, {ChoiceKind::Pass, 0});
    autoChooseFoe();
    tryCommit();
    ui_ = Ui::Message;
}

void BattleScene::handleEvent(const SDL_Event& ev) {
    if (ev.type != SDL_EVENT_KEY_DOWN || ev.key.repeat || done_) return;
    SDL_Keycode k = ev.key.key;
    if (k == SDLK_ESCAPE) { done_ = true; return; }   // flee (demo only)

    bool confirm = k == SDLK_Z || k == SDLK_RETURN || k == SDLK_SPACE;

    if (ui_ == Ui::Message) {
        if (confirm && !current_.empty()) current_.clear();   // fast-forward
        return;
    }
    if (ui_ == Ui::MoveMenu) {
        int count = static_cast<int>(battle_->request(0).moves.size());
        if (count <= 0) return;
        int catchIdx = wild_ ? count : -1;      // extra "CATCH" entry
        if (sel_ == catchIdx) {
            if (k == SDLK_UP) sel_ = std::min(2, count - 1);
        } else {
            if (k == SDLK_LEFT || k == SDLK_RIGHT) sel_ ^= 1;
            if (k == SDLK_UP) sel_ &= ~2;
            if (k == SDLK_DOWN) {
                if (sel_ < 2 && count > 2) sel_ |= 2;
                else if (wild_) sel_ = catchIdx;
            }
            if (sel_ != catchIdx) sel_ = std::clamp(sel_, 0, count - 1);
        }
        if (confirm) {
            if (sel_ == catchIdx) attemptCatch();
            else submitMove(sel_);
        }
    } else if (ui_ == Ui::SwitchMenu) {
        int count = static_cast<int>(battle_->request(0).switches.size());
        if (count <= 0) return;
        if (k == SDLK_UP) sel_ = (sel_ + count - 1) % count;
        if (k == SDLK_DOWN) sel_ = (sel_ + 1) % count;
        if (confirm) submitSwitch(sel_);
    }
}

void BattleScene::update() {
    if (done_) return;
    autoChooseFoe();
    tryCommit();

    for (auto& s : slots_) {
        float target = static_cast<float>(s.hp);
        s.shown += (target - s.shown) * 0.15f;
        if (std::abs(s.shown - target) < 0.5f) s.shown = target;
    }

    if (ui_ == Ui::Message) {
        if (current_.empty()) {
            if (!msgs_.empty()) {
                current_ = msgs_.front();
                msgs_.pop_front();
                msgTimer_ = 0;
            } else if (battle_->ended() || forceEnd_) {
                done_ = true;
            } else {
                Request req = battle_->request(0);
                if (req.kind == Request::Kind::Move) { ui_ = Ui::MoveMenu; sel_ = 0; }
                else if (req.kind == Request::Kind::Switch) { ui_ = Ui::SwitchMenu; sel_ = 0; }
            }
        } else if (++msgTimer_ > (auto_ ? 8 : 48)) {
            current_.clear();
        }
    } else if (auto_) {
        if (ui_ == Ui::MoveMenu) submitMove(0);
        else if (ui_ == Ui::SwitchMenu) submitSwitch(0);
    }
}

void BattleScene::render(SDL_Renderer* r, const Font& font) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 40, 48, 72, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawColor(r, 52, 64, 92, 255);
    SDL_FRect ground{0, 120, 320, 72};
    SDL_RenderFillRect(r, &ground);

    // sprites
    if (slots_[1].sprite) {
        SDL_FRect d{242, 16, 48, 48};
        SDL_RenderTexture(r, slots_[1].sprite->handle, nullptr, &d);
    }
    if (slots_[0].sprite) {
        SDL_FRect d{28, 84, 48, 48};
        SDL_RenderTexture(r, slots_[0].sprite->handle, nullptr, &d);
    }

    // info boxes: foe top-left, player mid-right
    auto infoBox = [&](const Slot& s, float x, float y, bool showNumbers) {
        SDL_SetRenderDrawColor(r, 20, 24, 36, 235);
        SDL_FRect box{x, y, 140, showNumbers ? 46.f : 34.f};
        SDL_RenderFillRect(r, &box);
        std::string title = s.name;
        if (!s.status.empty()) title += " [" + s.status + "]";
        font.draw(r, title, x + 5, y + 3);
        float frac = s.maxhp > 0 ? std::clamp(s.shown / s.maxhp, 0.f, 1.f) : 0.f;
        SDL_SetRenderDrawColor(r, 10, 12, 18, 255);
        SDL_FRect barBg{x + 5, y + 20, 130, 6};
        SDL_RenderFillRect(r, &barBg);
        if (frac > 0.5f) SDL_SetRenderDrawColor(r, 88, 200, 96, 255);
        else if (frac > 0.2f) SDL_SetRenderDrawColor(r, 232, 196, 64, 255);
        else SDL_SetRenderDrawColor(r, 224, 80, 72, 255);
        SDL_FRect bar{x + 6, y + 21, 128 * frac, 4};
        SDL_RenderFillRect(r, &bar);
        if (showNumbers) {
            font.draw(r, std::to_string(s.hp) + "/" + std::to_string(s.maxhp),
                      x + 5, y + 29, {200, 208, 224, 255});
        }
    };
    infoBox(slots_[1], 8, 10, false);
    infoBox(slots_[0], 172, 122, true);

    // bottom panel: message / menus
    SDL_SetRenderDrawColor(r, 16, 18, 28, 245);
    SDL_FRect panel{4, 158, 312, 30};
    if (ui_ != Ui::Message) panel = {4, 140, 312, 48};
    SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawColor(r, 120, 132, 168, 255);
    SDL_RenderRect(r, &panel);

    if (ui_ == Ui::Message) {
        font.draw(r, wrapText(current_, 37), 10, 160);
    } else if (ui_ == Ui::MoveMenu) {
        Request req = battle_->request(0);
        for (int i = 0; i < static_cast<int>(req.moves.size()) && i < 4; ++i) {
            const Move* mv = dex_.move(req.moves[i].id);
            std::string name = mv ? mv->name : "Struggle";
            float x = 24 + (i % 2) * 152;
            float y = 145 + (i / 2) * 15;
            if (i == sel_) font.draw(r, ">", x - 10, y, {255, 224, 96, 255});
            font.draw(r, name, x, y);
        }
        if (wild_) {
            int catchIdx = static_cast<int>(req.moves.size());
            if (sel_ == catchIdx) font.draw(r, ">", 14, 175, {255, 224, 96, 255});
            font.draw(r, "CATCH", 24, 175, {120, 224, 160, 255});
        }
        if (sel_ < static_cast<int>(req.moves.size())) {
            const auto& slot = req.moves[sel_];
            font.draw(r, "PP " + std::to_string(slot.pp) + "/" +
                          std::to_string(slot.maxpp),
                      248, 175, {160, 172, 200, 255});
        }
    } else if (ui_ == Ui::SwitchMenu) {
        font.draw(r, "Choose next monster:", 10, 143, {160, 172, 200, 255});
        Request req = battle_->request(0);
        const Side& side = battle_->side(0);
        // Only two rows fit, but the cursor cycles every eligible switch-in
        // (a full party offers five). Drawing a fixed first-two window meant
        // confirming a monster that was never on screen — scroll the window
        // so the selected entry is always the one being shown.
        const int total = static_cast<int>(req.switches.size());
        constexpr int kVisible = 2;
        int first = 0;
        if (sel_ >= kVisible) first = std::min(sel_ - kVisible + 1, std::max(0, total - kVisible));
        for (int row = 0; row < kVisible && first + row < total; ++row) {
            const int i = first + row;
            const BattleMonster& m = side.monsters[req.switches[i]];
            float y = 158 + row * 14;
            if (i == sel_) font.draw(r, ">", 12, y, {255, 224, 96, 255});
            font.draw(r, m.name + "  " + std::to_string(m.hp) + "/" +
                          std::to_string(m.stats.hp),
                      24, y);
        }
        if (total > kVisible) {
            font.draw(r, std::to_string(sel_ + 1) + "/" + std::to_string(total), 250, 186,
                      {160, 172, 200, 255});
        }
    }
}
