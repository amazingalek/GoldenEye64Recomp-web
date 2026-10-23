// The launcher and the menus' colours, kept as this game has always had them, on RecompFrontend's UI:
// - the theme's colours are this game's palette (purple), for the launcher and every menu;
// - the launcher is the title at the top left, the background on the right sliding in, and the options at the bottom
//   right (Select ROM or Start game, Setup controls, Settings, Exit), each with its bullet. RecompFrontend builds its UI
//   in code, so these are its elements with the classes assets/recomp.rcss styles, the layout the launcher always had.
// - the test harness (tools/harness.sh, GE_HARNESS_STAGE) starts the game once the launcher is up.

#include <chrono>
#include <cstdlib>
#include <functional>
#include <string>

#include "recompui/recompui.h"
#include "recompui/config.h"
#include "base/ui_launcher.h"
#include "elements/ui_theme.h"
#include "util/file.h"
#include "librecomp/game.hpp"
#include "ultramodern/ultramodern.hpp"
#include "zelda_config.h"

extern std::vector<recomp::GameEntry> supported_games;

namespace {
    using recompui::Element;
    using recompui::ResourceId;

    // An element styled by the stylesheet's classes, not by the inline display RecompFrontend gives every element.
    class StyledElement : public Element {
    protected:
        std::string_view get_type_name() override { return "StyledElement"; }
    public:
        StyledElement(ResourceId rid, Element* parent, std::string tag, std::string classes, std::string text = {}, uint32_t events = 0)
            : Element(rid, parent, events, tag, !text.empty()) {
            remove_property(Rml::PropertyId::Display);
            set_attribute("class", classes);
            if (!text.empty()) {
                set_text(text);
            }
        }
    };

    // A launcher option: its label with a bullet beside it.
    class MenuListItem : public StyledElement {
        std::function<void()> callback;
        Element* label;
    protected:
        void process_event(const recompui::Event& e) override {
            if (e.type == recompui::EventType::Click && is_enabled()) {
                callback();
            }
        }
        std::string_view get_type_name() override { return "MenuListItem"; }
    public:
        MenuListItem(ResourceId rid, Element* parent, std::string title, std::function<void()> callback)
            : StyledElement(rid, parent, "button", "menu-list-item", {}, recompui::Events(recompui::EventType::Click)),
              callback(std::move(callback)) {
            enable_focus();
            recompui::ContextId context = recompui::get_current_context();
            context.create_element<StyledElement>(this, "div", "menu-list-item__bullet", "\xE2\x80\xA2");
            label = context.create_element<StyledElement>(this, "div", "menu-list-item__label", title);
        }

        void set_title(const std::string& title) {
            label->set_text(title);
        }

        void set_callback(std::function<void()> new_callback) {
            callback = std::move(new_callback);
        }
    };

    const recomp::GameEntry& game() {
        return supported_games[0];
    }

    void select_rom(std::function<void()> on_selected) {
        recompui::file::open_file_dialog([on_selected](bool success, const std::filesystem::path& path) {
            if (!success) {
                return;
            }
            switch (recomp::select_rom(path, game().game_id)) {
                case recomp::RomValidationError::Good:
                    on_selected();
                    break;
                case recomp::RomValidationError::FailedToOpen:
                    recompui::message_box("Failed to open ROM file.");
                    break;
                case recomp::RomValidationError::NotARom:
                    recompui::message_box("This is not a valid ROM file.");
                    break;
                case recomp::RomValidationError::IncorrectRom:
                    recompui::message_box("This ROM is not the correct game.");
                    break;
                case recomp::RomValidationError::NotYet:
                    recompui::message_box("This game isn't supported yet.");
                    break;
                case recomp::RomValidationError::IncorrectVersion:
                    recompui::message_box(
                            "This ROM is the correct game, but the wrong version.\nThis project requires the NTSC-U N64 version of the game.");
                    break;
                case recomp::RomValidationError::OtherError:
                    recompui::message_box("An unknown error has occurred.");
                    break;
            }
        });
    }

    const char* start_title() {
        return recomp::is_rom_valid(game().game_id) ? "Start game" : "Select ROM";
    }

    void start_game() {
        recompui::update_game_mod_id(game().mod_game_id);
        recomp::start_game(game().game_id, {});
        recompui::hide_all_contexts();
    }

    void open_config(const std::string& tab) {
        recompui::update_game_mod_id(game().mod_game_id);
        recompui::config::set_tab(tab);
        recompui::hide_all_contexts();
        recompui::config::open();
    }

    void on_launcher_init(recompui::LauncherMenu* menu) {
        using recompui::NavigationType;
        recompui::ContextId context = recompui::get_current_context();
        menu->remove_default_title();
        recompui::update_game_mod_id(game().mod_game_id);

        auto launcher = context.create_element<StyledElement>(menu, "div", "launcher");

        auto left = context.create_element<StyledElement>(launcher, "div", "launcher__vertical-split");
        {
            auto title_quadrant = context.create_element<StyledElement>(left, "div", "launcher__title-quadrant");
            auto title = context.create_element<StyledElement>(title_quadrant, "button", "subtitle-title");
            context.create_element<StyledElement>(title, "div", "launcher__title", std::string{ zelda64::program_name });
            context.create_element<StyledElement>(left, "div", "launcher__content-quadrant");
        }

        auto right = context.create_element<StyledElement>(launcher, "div", "launcher__vertical-split launcher__vertical-split--right");
        {
            auto background_wrapper = context.create_element<StyledElement>(right, "div", "launcher__background-wrapper");
            auto background = context.create_element<StyledElement>(background_wrapper, "svg", "launcher__background-mm");
            background->set_src("mm-clipped.svg");
            context.create_element<StyledElement>(right, "div", "launcher__title-quadrant launcher__title-quadrant--right");

            auto options = context.create_element<StyledElement>(right, "div", "launcher__content-quadrant");
            options->set_as_navigation_container(NavigationType::Vertical);
            options->set_nav_wrapping(true);

            MenuListItem* start = context.create_element<MenuListItem>(options, std::string{ start_title() }, std::function<void()>{});
            start->set_callback([start]() {
                if (recomp::is_rom_valid(game().game_id)) {
                    start_game();
                    return;
                }
                select_rom([start]() {
                    recompui::ContextId launcher_context = recompui::get_launcher_context_id();
                    bool opened = launcher_context.open_if_not_already();
                    start->set_title(start_title());
                    if (opened) {
                        launcher_context.close();
                    }
                });
            });
            start->set_as_primary_focus(true);
            context.set_autofocus_element(start);

            context.create_element<MenuListItem>(options, std::string{ "Setup controls" }, std::function<void()>{ []() {
                open_config(recompui::config::controls::id);
            } });
            context.create_element<MenuListItem>(options, std::string{ "Settings" }, std::function<void()>{ []() {
                open_config(recompui::config::general::id);
            } });
            context.create_element<MenuListItem>(options, std::string{ "Exit" }, std::function<void()>{ []() {
                ultramodern::quit();
            } });
        }
    }

    // Test harness (tools/harness.sh): press "Start game" two seconds after the launcher is up.
    void on_launcher_update(recompui::LauncherMenu*) {
        static bool harness = getenv("GE_HARNESS_STAGE") != nullptr;
        static std::chrono::steady_clock::time_point first_update{};
        if (!harness || !recomp::is_rom_valid(game().game_id)) {
            return;
        }
        auto now = std::chrono::steady_clock::now();
        if (first_update == std::chrono::steady_clock::time_point{}) {
            first_update = now;
        }
        else if (now - first_update >= std::chrono::seconds(2)) {
            harness = false;
            start_game();
        }
    }
}

namespace zelda64 {
    void init_theme() {
        using recompui::theme::color;
        using recompui::theme::set_theme_color;
        struct ThemeColor {
            color id;
            recompui::Color value;
        };
        // This game's palette (Zelda 64: Recompiled 1.1's, which its menus were made from).
        static const ThemeColor palette[] = {
            { color::Background1, { 8, 7, 13, 255 } },
            { color::Background2, { 18, 16, 24, 255 } },
            { color::Background3, { 25, 22, 34, 255 } },
            { color::BGOverlay, { 190, 184, 219, 26 } },
            { color::ModalOverlay, { 8, 7, 13, 230 } },
            { color::BGShadow, { 0, 0, 0, 89 } },
            { color::BGShadow2, { 8, 7, 13, 184 } },
            { color::Text, { 242, 242, 242, 255 } },
            { color::TextDim, { 204, 204, 204, 255 } },
            { color::TextInactive, { 255, 255, 255, 153 } },
            { color::Primary, { 185, 125, 242, 255 } },
            { color::PrimaryL, { 218, 186, 247, 255 } },
            { color::PrimaryD, { 122, 42, 198, 255 } },
            { color::PrimaryA5, { 185, 125, 242, 13 } },
            { color::PrimaryA20, { 185, 125, 242, 51 } },
            { color::PrimaryA30, { 185, 125, 242, 76 } },
            { color::PrimaryA50, { 185, 125, 242, 128 } },
            { color::PrimaryA80, { 185, 125, 242, 204 } },
            { color::Secondary, { 23, 214, 232, 255 } },
            { color::SecondaryL, { 162, 239, 246, 255 } },
            { color::SecondaryD, { 37, 161, 173, 255 } },
            { color::SecondaryA5, { 23, 214, 232, 13 } },
            { color::SecondaryA20, { 23, 214, 232, 51 } },
            { color::SecondaryA30, { 23, 214, 232, 76 } },
            { color::SecondaryA50, { 23, 214, 232, 128 } },
            { color::SecondaryA80, { 23, 214, 232, 204 } },
            { color::Warning, { 233, 205, 53, 255 } },
            { color::WarningL, { 249, 229, 124, 255 } },
            { color::WarningD, { 197, 170, 22, 255 } },
            { color::WarningA5, { 233, 205, 53, 13 } },
            { color::WarningA20, { 233, 205, 53, 51 } },
            { color::WarningA30, { 233, 205, 53, 76 } },
            { color::WarningA50, { 233, 205, 53, 128 } },
            { color::WarningA80, { 233, 205, 53, 204 } },
            { color::Danger, { 248, 96, 57, 255 } },
            { color::DangerL, { 254, 134, 103, 255 } },
            { color::DangerD, { 178, 57, 25, 255 } },
            { color::DangerA5, { 248, 96, 57, 13 } },
            { color::DangerA20, { 248, 96, 57, 51 } },
            { color::DangerA30, { 248, 96, 57, 76 } },
            { color::DangerA50, { 248, 96, 57, 128 } },
            { color::DangerA80, { 248, 96, 57, 204 } },
            { color::Success, { 69, 208, 67, 255 } },
            { color::SuccessL, { 170, 234, 169, 255 } },
            { color::SuccessD, { 44, 167, 42, 255 } },
            { color::SuccessA5, { 69, 208, 67, 13 } },
            { color::SuccessA20, { 69, 208, 67, 51 } },
            { color::SuccessA30, { 69, 208, 67, 76 } },
            { color::SuccessA50, { 69, 208, 67, 128 } },
            { color::SuccessA80, { 69, 208, 67, 204 } },
            { color::Border, { 255, 255, 255, 51 } },
            { color::BorderSoft, { 255, 255, 255, 26 } },
            { color::BorderHard, { 255, 255, 255, 76 } },
            { color::BorderSolid, { 255, 255, 255, 153 } },
            // RecompFrontend's raised surfaces (a hovered option, a card), in the palette's overlay colour.
            { color::Elevated, { 190, 184, 219, 26 } },
            { color::ElevatedSoft, { 190, 184, 219, 13 } },
            { color::ElevatedBorder, { 190, 184, 219, 204 } },
            { color::ElevatedBorderHard, { 190, 184, 219, 255 } },
        };
        for (const ThemeColor& entry : palette) {
            set_theme_color(entry.id, entry.value);
        }
    }

    void init_launcher() {
        recompui::register_launcher_init_callback(on_launcher_init);
        recompui::register_launcher_update_callback(on_launcher_update);
    }
}
