// HomeTidy screens and navigation.
#pragma once

#include <SDL.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gfx.h"
#include "platform.h"
#include "tasks.h"
#include "titles.h"

class App {
public:
    enum class Screen { Loading, Home, SortPreview, SortConfirm, SortDone, Mark, Message };

    /// Args:
    ///     canvas: Drawing surface; must outlive the app.
    ///     env: Storage locations from platform::init().
    App(gfx::Canvas& canvas, platform::Env env);
    ~App();

    /// Handles one frame of input.
    void update(const platform::Input& in);
    /// Draws the current screen.
    void draw();
    bool wantsQuit() const { return quit_; }
    Screen screen() const { return screen_; }

private:
    struct Icon {
        SDL_Texture* tex = nullptr;
        tasks::IconState state = tasks::IconState::Unreadable;
    };

    // lifecycle
    void load();
    void loadConfig();
    void saveConfig();
    // screens: update + draw
    void updateHome(const platform::Input& in);
    void drawHome();
    void enterSort();
    void updateSort(const platform::Input& in);
    void drawSort();
    void updateConfirm(const platform::Input& in);
    void drawConfirm();
    void applySort();
    void drawDone();
    void enterMark();
    void updateMark(const platform::Input& in);
    void drawMark();
    void toggleMark();
    void drawMessage();
    // shared pieces
    void header(const std::string& subtitle);
    void hints(const std::vector<std::pair<std::string, std::string>>& items);
    void toastDraw();
    void toast(const std::string& text, gfx::Color color);
    void showMessage(const std::string& title, const std::string& body, gfx::Color color, Screen back);
    Icon& icon(const titles::Title& t);
    void dropIcon(std::uint64_t id);
    std::string folderTitle(const tasks::FolderPlan& f) const;
    std::string layoutLabel(const tasks::LayoutFile& f) const;
    std::string statusText(tasks::Status s) const;
    int movesIn(const tasks::FolderPlan& f) const;

    gfx::Canvas& c_;
    platform::Env env_;
    Screen screen_ = Screen::Loading;
    bool quit_ = false;
    int frame_ = 0;
    Screen lastLogged_ = Screen::Message;  ///< Last screen written to the log.
    int busy_ = 0;  ///< Frames to wait so the "Working..." overlay shows before a slow job.

    // data
    std::vector<sorter::Rule> rules_;
    std::vector<tasks::LayoutFile> layouts_;
    std::map<std::uint64_t, titles::Title> catalog_;   ///< Installed games (the Mark screen list).
    std::map<std::uint64_t, titles::Title> allNames_;  ///< Games, system apps and homebrew, for sorting.
    std::vector<const titles::Title*> markList_;
    std::map<std::uint64_t, Icon> icons_;
    int homeGames_ = 0;
    int homeFolders_ = 0;

    // home
    int homeFocus_ = 0;
    // sort
    int layoutIdx_ = 0;
    std::optional<tasks::SortPlan> plan_;
    int folderSel_ = 0;
    int rowSel_ = 0;
    tasks::Result sortResult_;
    bool recheckOk_ = false;
    // mark
    int markSel_ = 0;
    int markScroll_ = 0;
    // message / toast
    std::string msgTitle_, msgBody_;
    gfx::Color msgColor_ = gfx::theme::red;
    Screen msgBack_ = Screen::Home;
    std::string toastText_;
    gfx::Color toastColor_ = gfx::theme::green;
    int toastFrames_ = 0;
};
