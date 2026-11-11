#include "app.h"

#include <algorithm>
#include <cstdio>

#include "badge.h"
#include "default_rules.h"
#include "fsx.h"
#include "layout.h"
#include "sorter.h"
#include "texts.h"

using gfx::Align;
using gfx::Size;
namespace col = gfx::theme;
using strings::Id;

namespace {

constexpr int kMargin = 64;
constexpr int kGridCols = 7;
constexpr int kGridRows = 2;
constexpr int kTileW = 152;
constexpr int kTileH = 206;
constexpr int kTileGap = 16;
constexpr const char* kVersion = "v1.0.4";

std::string fmt(const char* f, int a) {
    char b[256];
    std::snprintf(b, sizeof b, f, a);
    return b;
}

std::string fmt(const char* f, int a, int b2) {
    char b[256];
    std::snprintf(b, sizeof b, f, a, b2);
    return b;
}

std::string tr(Id id) { return strings::get(id); }

}  // namespace

App::App(gfx::Canvas& canvas, platform::Env env) : c_(canvas), env_(std::move(env)) {}

App::~App() {
    for (auto& kv : icons_) {
        if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    }
}

// ------------------------------------------------------------- lifecycle --

void App::loadConfig() {
    std::string text;
    if (!fsx::readFile(env_.appDir + "/config.txt", text)) return;
    if (text.find("lang=pt") != std::string::npos) strings::set(strings::Lang::Pt);
    if (text.find("lang=en") != std::string::npos) strings::set(strings::Lang::En);
}

void App::saveConfig() {
    const std::string text = strings::current() == strings::Lang::Pt ? "lang=pt\n" : "lang=en\n";
    fsx::makeDirs(env_.appDir);
    fsx::writeBytes(env_.appDir + "/config.txt", std::vector<std::uint8_t>(text.begin(), text.end()));
}

void App::load() {
    loadConfig();

    std::string user;
    if (fsx::readFile(env_.appDir + "/series.txt", user)) rules_ = sorter::parseRules(user);
    for (const auto& r : sorter::parseRules(kDefaultRules)) rules_.push_back(r);  // user rules win

    layouts_ = tasks::findLayoutFiles(env_.sdRoot, env_.mlcRoot);
    catalog_ = titles::scan(env_.titleRoots);
    allNames_ = catalog_;  // games, then system apps and SD homebrew, for sorting the Home pages
    const auto sys = titles::scanSystemApps(env_.mlcRoot);
    const auto brew = titles::scanHomebrew(env_.sdRoot);
    allNames_.insert(sys.begin(), sys.end());
    allNames_.insert(brew.begin(), brew.end());
    platform::log("names: games=%zu system=%zu homebrew=%zu", catalog_.size(), sys.size(), brew.size());
    for (const auto& f : layouts_) platform::log("layout file: %s (%s)", f.path.c_str(), f.onSd ? "sd" : "mlc");
    for (const auto& r : env_.titleRoots) platform::log("title root: %s", r.c_str());
    platform::log("loaded: rules=%zu layouts=%zu titles=%zu", rules_.size(), layouts_.size(), catalog_.size());

    for (const auto& kv : catalog_) markList_.push_back(&kv.second);  // catalog_ holds games only
    std::stable_sort(markList_.begin(), markList_.end(), [&](const titles::Title* a, const titles::Title* b) {
        return sorter::less(sorter::makeKey(a->name, rules_), sorter::makeKey(b->name, rules_));
    });

    if (!layouts_.empty()) {
        std::vector<std::uint8_t> bytes;
        if (fsx::readBytes(layouts_[0].path, bytes)) {
            if (auto lay = layout::Layout::parse(bytes)) {
                for (int n = 1; n <= lay->folderCount(); ++n) {
                    const auto f = lay->folder(n);
                    if (f.items.empty()) continue;
                    ++homeFolders_;
                    homeGames_ += int(f.items.size());
                }
            }
        }
    }
    platform::log("home: %d games in %d folders", homeGames_, homeFolders_);
    screen_ = Screen::Home;
}

// ----------------------------------------------------------------- input --

void App::update(const platform::Input& in) {
    ++frame_;
    if (screen_ != lastLogged_) {
        platform::log("screen -> %d", static_cast<int>(screen_));
        lastLogged_ = screen_;
    }
    if (toastFrames_ > 0) --toastFrames_;

    if (screen_ == Screen::Loading) {
        if (frame_ >= 3) load();
        return;
    }
    if (busy_ > 0) {
        if (--busy_ == 0) applySort();
        return;
    }
    if (in.y && screen_ == Screen::Home) {
        strings::set(strings::current() == strings::Lang::En ? strings::Lang::Pt : strings::Lang::En);
        saveConfig();
        return;
    }
    switch (screen_) {
        case Screen::Home: updateHome(in); break;
        case Screen::SortPreview: updateSort(in); break;
        case Screen::SortConfirm: updateConfirm(in); break;
        case Screen::SortDone:
            if (in.a) quit_ = true;
            if (in.b) screen_ = Screen::Home;
            break;
        case Screen::Mark: updateMark(in); break;
        case Screen::Message:
            if (in.a || in.b) screen_ = msgBack_;
            break;
        default: break;
    }
}

void App::updateHome(const platform::Input& in) {
    if (in.left || in.right || in.up || in.down) homeFocus_ = 1 - homeFocus_;
    if (in.b) quit_ = true;
    if (!in.a) return;
    if (!env_.storageOk) {
        toast(tr(Id::NoAccess), col::red);
        return;
    }
    if (homeFocus_ == 0) enterSort();
    else enterMark();
}

void App::enterSort() {
    if (layouts_.empty()) {
        showMessage(tr(Id::SortTitle), tr(Id::NoLayout), col::amber, Screen::Home);
        return;
    }
    layoutIdx_ = std::clamp(layoutIdx_, 0, int(layouts_.size()) - 1);
    std::vector<std::uint8_t> bytes;
    if (!fsx::readBytes(layouts_[layoutIdx_].path, bytes)) {
        showMessage(tr(Id::FailTitle), statusText(tasks::Status::ReadFailed), col::red, Screen::Home);
        return;
    }
    plan_ = tasks::planSort(bytes, allNames_, rules_);
    platform::log("sort plan for %s: %zu bytes read, valid=%d, folders=%zu, changed=%d", layouts_[layoutIdx_].path.c_str(),
                  bytes.size(), plan_.has_value(), plan_ ? plan_->folders.size() : 0, plan_ ? plan_->changedFolders : 0);
    if (!plan_ || plan_->folders.empty()) {
        showMessage(tr(Id::SortTitle), tr(Id::NoLayout), col::amber, Screen::Home);
        plan_.reset();
        return;
    }
    folderSel_ = 0;
    for (std::size_t i = 0; i < plan_->folders.size(); ++i) {
        if (plan_->folders[i].changed) {
            folderSel_ = int(i);
            break;
        }
    }
    rowSel_ = 0;
    screen_ = Screen::SortPreview;
}

void App::updateSort(const platform::Input& in) {
    if (!plan_) {
        screen_ = Screen::Home;
        return;
    }
    const int folders = int(plan_->folders.size());
    if (in.b) {
        screen_ = Screen::Home;
        return;
    }
    if (in.left || in.l) {
        folderSel_ = (folderSel_ + folders - 1) % folders;
        rowSel_ = 0;
    }
    if (in.right || in.r) {
        folderSel_ = (folderSel_ + 1) % folders;
        rowSel_ = 0;
    }
    const int rows = int(plan_->folders[folderSel_].rows.size());
    if (in.up) rowSel_ = std::max(0, rowSel_ - 1);
    if (in.down) rowSel_ = std::min(rows - 1, rowSel_ + 1);
    if (in.x && layouts_.size() > 1) {
        layoutIdx_ = (layoutIdx_ + 1) % int(layouts_.size());
        enterSort();
        return;
    }
    if (in.a) {
        if (plan_->changedFolders == 0) toast(tr(Id::AlreadySorted), col::green);
        else screen_ = Screen::SortConfirm;
    }
}

void App::updateConfirm(const platform::Input& in) {
    if (in.b) screen_ = Screen::SortPreview;
    if (in.a) busy_ = 3;
}

void App::applySort() {
    const std::string path = layouts_[layoutIdx_].path;
    sortResult_ = tasks::applySort(path, *plan_, env_.appDir + "/backup", platform::timestamp());
    platform::log("applySort %s status=%d backup=%s", path.c_str(), int(sortResult_.status),
                  sortResult_.backupPath.c_str());
    if (sortResult_.status != tasks::Status::Ok) {
        showMessage(tr(Id::FailTitle), statusText(sortResult_.status), col::red, Screen::Home);
        return;
    }
    platform::sleepMs(2000);  // the console may still rewrite the file; look again
    recheckOk_ = tasks::fileEquals(path, plan_->sorted);
    platform::log("recheck=%d", recheckOk_);
    screen_ = Screen::SortDone;
}

void App::enterMark() {
    if (markList_.empty()) {
        showMessage(tr(Id::MarkTitle), tr(Id::NoTitles), col::amber, Screen::Home);
        return;
    }
    markSel_ = std::clamp(markSel_, 0, int(markList_.size()) - 1);
    screen_ = Screen::Mark;
}

void App::updateMark(const platform::Input& in) {
    const int n = int(markList_.size());
    if (in.b) {
        screen_ = Screen::Home;
        return;
    }
    if (in.left) markSel_ = std::max(0, markSel_ - 1);
    if (in.right) markSel_ = std::min(n - 1, markSel_ + 1);
    if (in.up) markSel_ = std::max(0, markSel_ - kGridCols);
    if (in.down) markSel_ = std::min(n - 1, markSel_ + kGridCols);
    if (in.l) markSel_ = std::max(0, markSel_ - kGridCols * kGridRows);
    if (in.r) markSel_ = std::min(n - 1, markSel_ + kGridCols * kGridRows);
    const int row = markSel_ / kGridCols;
    if (row < markScroll_) markScroll_ = row;
    if (row >= markScroll_ + kGridRows) markScroll_ = row - kGridRows + 1;
    if (in.a) toggleMark();
}

void App::toggleMark() {
    const titles::Title& t = *markList_[markSel_];
    Icon& ic = icon(t);
    const std::string dir = env_.appDir + "/backup";
    tasks::Result r;
    if (ic.state == tasks::IconState::Marked) {
        r = tasks::unmarkIcon(t, dir);
        if (r.status == tasks::Status::Ok) toast(tr(r.approximate ? Id::ToastUnmarkedApprox : Id::ToastUnmarked), col::green);
    } else {
        r = tasks::markIcon(t, dir);
        if (r.status == tasks::Status::Ok) toast(tr(Id::ToastMarked), col::green);
    }
    platform::log("mark %016llx status=%d", static_cast<unsigned long long>(t.id), int(r.status));
    if (r.status == tasks::Status::NothingToDo) toast(tr(Id::ToastAlready), col::amber);
    else if (r.status != tasks::Status::Ok) toast(statusText(r.status), col::red);
    dropIcon(t.id);  // reload the new pixels
}

// ------------------------------------------------------------- utilities --

std::string App::statusText(tasks::Status s) const {
    switch (s) {
        case tasks::Status::ReadFailed: return tr(Id::StReadFailed);
        case tasks::Status::Changed: return tr(Id::StChanged);
        case tasks::Status::BackupFailed: return tr(Id::StBackupFailed);
        case tasks::Status::WriteFailed: return tr(Id::StWriteFailed);
        case tasks::Status::VerifyFailed: return tr(Id::StVerifyFailed);
        case tasks::Status::NothingToDo: return tr(Id::StNothing);
        case tasks::Status::NoBackup: return tr(Id::StNoBackup);
        case tasks::Status::Unsupported: return tr(Id::StUnsupported);
        default: return "";
    }
}

std::string App::folderTitle(const tasks::FolderPlan& f) const {
    if (f.number == 0) return tr(Id::HomeName);
    return f.name.empty() ? tr(Id::HintFolder) + " " + std::to_string(f.number) : f.name;
}

std::string App::layoutLabel(const tasks::LayoutFile& f) const {
    return tr(f.onSd ? Id::LayoutSd : Id::LayoutMlc);
}

int App::movesIn(const tasks::FolderPlan& f) const {
    int n = 0;
    for (const auto& r : f.rows) n += r.from != r.to;
    return n;
}

void App::toast(const std::string& text, gfx::Color color) {
    toastText_ = text;
    toastColor_ = color;
    toastFrames_ = 150;
}

void App::showMessage(const std::string& title, const std::string& body, gfx::Color color, Screen back) {
    msgTitle_ = title;
    msgBody_ = body;
    msgColor_ = color;
    msgBack_ = back;
    screen_ = Screen::Message;
}

App::Icon& App::icon(const titles::Title& t) {
    auto it = icons_.find(t.id);
    if (it != icons_.end()) return it->second;
    Icon ic;
    std::vector<std::uint8_t> file, rgba;
    int w = 0, h = 0;
    if (fsx::readBytes(t.dir + "/meta/iconTex.tga", file) && badge::decode(file, rgba, w, h)) {
        ic.tex = c_.texture(rgba, w, h);
        ic.state = badge::isMarked(file) ? tasks::IconState::Marked : tasks::IconState::Plain;
    }
    return icons_.emplace(t.id, ic).first->second;
}

void App::dropIcon(std::uint64_t id) {
    auto it = icons_.find(id);
    if (it == icons_.end()) return;
    if (it->second.tex) SDL_DestroyTexture(it->second.tex);
    icons_.erase(it);
}

// -------------------------------------------------------------- drawing --

void App::draw() {
    c_.clear(col::bg);
    switch (screen_) {
        case Screen::Loading: {
            c_.checkBadge(640, 300, 56, col::greenDark, col::text);
            c_.text("HomeTidy", Size::Title, col::text, 640, 380, Align::Center);
            c_.text(tr(Id::Loading), Size::Body, col::muted, 640, 450, Align::Center);
            break;
        }
        case Screen::Home: drawHome(); break;
        case Screen::SortPreview: drawSort(); break;
        case Screen::SortConfirm:
            drawSort();
            drawConfirm();
            break;
        case Screen::SortDone: drawDone(); break;
        case Screen::Mark: drawMark(); break;
        case Screen::Message: drawMessage(); break;
    }
    if (busy_ > 0) {
        c_.fill(0, 0, 1280, 720, 0, {0, 0, 0, 190});
        c_.card(440, 300, 400, 120, 20, col::card, 2, col::line);
        c_.text(tr(Id::Busy), Size::Large, col::text, 640, 340, Align::Center);
    }
    toastDraw();
}

void App::header(const std::string& subtitle) {
    c_.checkBadge(kMargin + 28, 76, 28, col::greenDark, col::text);
    c_.text("HomeTidy", Size::Large, col::text, kMargin + 72, 50);
    c_.text(subtitle, Size::Body, col::muted, 1280 - kMargin, 56, Align::Right);
}

void App::hints(const std::vector<std::pair<std::string, std::string>>& items) {
    int x = kMargin;
    const int y = 668;
    for (const auto& it : items) {
        const int kw = std::max(32, c_.textWidth(it.first, Size::Small) + 20);
        c_.fill(x, y, kw, 32, 16, col::raised);
        c_.text(it.first, Size::Small, col::text, x + kw / 2, y + 3, Align::Center);
        x += kw + 10;
        x += c_.text(it.second, Size::Small, col::muted, x, y + 3) + 34;
    }
    c_.text(kVersion, Size::Small, col::muted, 1280 - kMargin, y + 3, Align::Right);
}

void App::toastDraw() {
    if (toastFrames_ <= 0) return;
    const int w = std::max(320, c_.textWidth(toastText_, Size::Body) + 72);
    const int x = (1280 - w) / 2, y = 84;
    c_.card(x, y, w, 56, 28, col::card, 3, toastColor_);
    c_.text(toastText_, Size::Body, col::text, 640, y + 12, Align::Center);
}

void App::drawHome() {
    header(tr(Id::AppTagline));
    const int cardW = 560, cardH = 420, y = 150;
    for (int i = 0; i < 2; ++i) {
        const int x = kMargin + i * (cardW + 32);
        const bool focus = homeFocus_ == i;
        if (focus) c_.fill(x - 6, y - 6, cardW + 12, cardH + 12, 30, col::green);
        c_.card(x, y, cardW, cardH, 24, focus ? col::raised : col::card, 2, col::line);

        const int ix = x + 40, iy = y + 40;
        if (i == 0) {  // bars shrinking = sorted list
            const int widths[4] = {88, 70, 52, 34};
            for (int k = 0; k < 4; ++k) {
                c_.fill(ix, iy + k * 24, widths[k], 14, 7, k == 0 ? col::green : col::muted);
            }
        } else {  // icon tile with a check
            c_.fill(ix, iy, 92, 92, 16, col::line);
            c_.fill(ix + 12, iy + 12, 68, 68, 10, col::card);
            c_.checkBadge(ix + 74, iy + 18, 26, col::green, col::bg);
        }
        c_.text(tr(i == 0 ? Id::SortTitle : Id::MarkTitle), Size::Large, col::text, x + 40, y + 160);
        const auto lines = c_.wrap(tr(i == 0 ? Id::SortDesc : Id::MarkDesc), Size::Body, cardW - 80);
        for (std::size_t k = 0; k < lines.size() && k < 4; ++k) {
            c_.text(lines[k], Size::Body, col::muted, x + 40, y + 212 + int(k) * 32);
        }
        const std::string status =
            i == 0 ? fmt(tr(Id::SortStatus).c_str(), homeGames_, homeFolders_) : fmt(tr(Id::MarkStatus).c_str(), int(catalog_.size()));
        const int sw = c_.textWidth(status, Size::Small) + 36;
        c_.fill(x + 40, y + cardH - 72, sw, 40, 20, focus ? col::card : col::raised);
        c_.text(status, Size::Small, col::text, x + 40 + sw / 2, y + cardH - 64, Align::Center);
    }
    if (!env_.storageOk) {
        c_.text(tr(Id::NoAccess), Size::Small, col::red, kMargin, 592, Align::Left, 1150);
    } else if (!layouts_.empty()) {
        c_.text(layoutLabel(layouts_[0]), Size::Small, col::muted, kMargin, 592);
    } else {
        c_.text(tr(Id::NoLayout), Size::Small, col::amber, kMargin, 592, Align::Left, 1150);
    }
    hints({{"A", tr(Id::HintSelect)}, {"Y", tr(Id::HintLanguage)}, {"B", tr(Id::HintExit)}});
}

void App::drawSort() {
    header(tr(Id::SortTitle));
    if (!plan_) return;
    const int top = 140, h = 500;
    // folder list
    const int lw = 360;
    for (std::size_t i = 0; i < plan_->folders.size() && i < 7; ++i) {
        const auto& f = plan_->folders[i];
        const int y = top + int(i) * 70;
        const bool sel = int(i) == folderSel_;
        if (sel) c_.fill(kMargin - 4, y - 4, lw + 8, 70, 18, col::green);
        c_.card(kMargin, y, lw, 62, 14, sel ? col::raised : col::card, 2, col::line);
        c_.text(folderTitle(f), Size::Body, col::text, kMargin + 20, y + 4, Align::Left, lw - 40);
        std::string sub;
        gfx::Color subColor = col::muted;
        if (f.skipped) {
            sub = tr(Id::FolderSkipped);
            subColor = col::amber;
        } else if (f.changed) {
            sub = fmt(tr(f.number == 0 ? Id::HomeIcons : Id::FolderGames).c_str(), int(f.rows.size())) + "  -  " +
                  fmt(tr(Id::FolderMoves).c_str(), movesIn(f));
            subColor = col::amber;
        } else {
            sub = fmt(tr(f.number == 0 ? Id::HomeIcons : Id::FolderGames).c_str(), int(f.rows.size())) + "  -  " + tr(Id::FolderSorted);
        }
        c_.text(sub, Size::Small, subColor, kMargin + 20, y + 33, Align::Left, lw - 40);
    }
    // new order
    const int px = kMargin + lw + 24, pw = 1280 - kMargin - px;
    c_.card(px, top, pw, h, 20, col::card, 2, col::line);
    const auto& f = plan_->folders[folderSel_];
    c_.text(folderTitle(f), Size::Large, col::text, px + 28, top + 14);
    c_.line(px + 20, top + 64, px + pw - 20, top + 64, 2, col::line);

    const int rowH = 38, visible = (h - 84) / rowH;
    const int n = int(f.rows.size());
    const int first = std::clamp(rowSel_ - visible / 2, 0, std::max(0, n - visible));
    for (int i = first; i < std::min(n, first + visible); ++i) {
        const auto& r = f.rows[i];
        const int y = top + 76 + (i - first) * rowH;
        if (i == rowSel_) c_.fill(px + 12, y - 2, pw - 24, rowH, 10, col::raised);
        c_.text(std::to_string(i + 1), Size::Small, col::muted, px + 64, y + 3, Align::Right);
        int nameX = px + 84;
        if (r.folder) {  // small folder glyph
            c_.fill(nameX, y + 9, 26, 18, 4, col::green);
            c_.fill(nameX, y + 5, 12, 8, 3, col::green);
            nameX += 38;
        }
        c_.text(r.name, Size::Body, r.known || r.folder ? col::text : col::muted, nameX, y, Align::Left, pw - (nameX - px) - 190);
        if (!r.known && !r.folder) c_.text(tr(f.number == 0 ? Id::SystemIcon : Id::NotInstalled), Size::Small, col::muted, px + pw - 28, y + 3, Align::Right);
        else if (r.from != r.to) c_.text(fmt(tr(Id::WasPosition).c_str(), r.from + 1), Size::Small, col::amber, px + pw - 28, y + 3, Align::Right);
    }
    std::vector<std::pair<std::string, std::string>> hs = {
        {"A", tr(Id::HintApply)}, {"B", tr(Id::HintBack)}, {"L / R", tr(Id::HintFolder)}, {"D-pad", tr(Id::HintScroll)}};
    if (layouts_.size() > 1) hs.push_back({"X", tr(Id::HintLayout)});
    hints(hs);
    c_.text(layoutLabel(layouts_[layoutIdx_]), Size::Small, col::muted, 1280 - kMargin, 108, Align::Right);
}

void App::drawConfirm() {
    c_.fill(0, 0, 1280, 720, 0, {0, 0, 0, 190});
    const int w = 780, h = 300, x = (1280 - w) / 2, y = (720 - h) / 2;
    c_.card(x, y, w, h, 24, col::card, 2, col::line);
    c_.text(tr(Id::ConfirmTitle), Size::Large, col::text, x + 40, y + 36);
    const auto lines = c_.wrap(tr(Id::ConfirmBody), Size::Body, w - 80);
    for (std::size_t i = 0; i < lines.size(); ++i) c_.text(lines[i], Size::Body, col::muted, x + 40, y + 96 + int(i) * 32);
    c_.fill(x + 40, y + h - 84, 260, 52, 26, col::greenDark);
    c_.text("A  " + tr(Id::HintApply), Size::Body, col::text, x + 170, y + h - 74, Align::Center);
    c_.fill(x + 324, y + h - 84, 260, 52, 26, col::raised);
    c_.text("B  " + tr(Id::HintCancel), Size::Body, col::text, x + 454, y + h - 74, Align::Center);
}

void App::drawDone() {
    header(tr(Id::SortTitle));
    const int w = 960, h = 420, x = (1280 - w) / 2, y = 150;
    c_.card(x, y, w, h, 24, col::card, 2, col::line);
    c_.checkBadge(x + 80, y + 80, 36, col::green, col::bg);
    c_.text(tr(Id::DoneTitle), Size::Large, col::text, x + 140, y + 56);
    int ty = y + 130;
    std::string shown = sortResult_.backupPath;
    if (shown.compare(0, env_.sdRoot.size(), env_.sdRoot) == 0) shown.erase(0, env_.sdRoot.size());
    for (const auto& l : c_.wrap(tr(Id::DoneBackup) + ": " + shown, Size::Small, w - 80)) {
        c_.text(l, Size::Small, col::muted, x + 40, ty);
        ty += 26;
    }
    ty += 14;
    c_.text(tr(recheckOk_ ? Id::DoneRecheckOk : Id::DoneRecheckBad), Size::Body, recheckOk_ ? col::green : col::red, x + 40, ty,
            Align::Left, w - 80);
    ty += 56;
    for (const auto& l : c_.wrap(tr(Id::DoneBody), Size::Body, w - 80)) {
        c_.text(l, Size::Body, col::text, x + 40, ty);
        ty += 32;
    }
    hints({{"A", tr(Id::HintClose)}, {"B", tr(Id::HintBack)}});
}

void App::drawMessage() {
    header("");
    const int w = 860, h = 300, x = (1280 - w) / 2, y = 190;
    c_.card(x, y, w, h, 24, col::card, 3, msgColor_);
    c_.text(msgTitle_, Size::Large, col::text, x + 40, y + 36, Align::Left, w - 80);
    int ty = y + 100;
    for (const auto& l : c_.wrap(msgBody_, Size::Body, w - 80)) {
        c_.text(l, Size::Body, col::muted, x + 40, ty);
        ty += 32;
    }
    hints({{"A", tr(Id::HintBack)}});
}

void App::drawMark() {
    header(tr(Id::MarkTitle));
    const int n = int(markList_.size());
    const int x0 = (1280 - (kGridCols * kTileW + (kGridCols - 1) * kTileGap)) / 2, y0 = 150;
    for (int row = 0; row < kGridRows; ++row) {
        for (int cIdx = 0; cIdx < kGridCols; ++cIdx) {
            const int idx = (markScroll_ + row) * kGridCols + cIdx;
            if (idx >= n) break;
            const titles::Title& t = *markList_[idx];
            const int x = x0 + cIdx * (kTileW + kTileGap), y = y0 + row * (kTileH + kTileGap);
            const bool sel = idx == markSel_;
            if (sel) c_.fill(x - 5, y - 5, kTileW + 10, kTileH + 10, 20, col::green);
            c_.card(x, y, kTileW, kTileH, 16, sel ? col::raised : col::card, 2, col::line);
            Icon& ic = icon(t);
            if (ic.tex) c_.blit(ic.tex, x + 12, y + 12, 128, 128);
            else c_.fill(x + 12, y + 12, 128, 128, 12, col::line);
            auto lines = c_.wrap(t.name, Size::Small, kTileW - 16);
            if (lines.size() > 2) {
                lines.resize(2);
                lines[1] += " ...";
            }
            for (std::size_t k = 0; k < lines.size(); ++k) {
                c_.text(lines[k], Size::Small, col::text, x + kTileW / 2, y + 148 + int(k) * 25, Align::Center, kTileW - 12);
            }
        }
    }
    // detail strip
    const titles::Title& cur = *markList_[markSel_];
    const Icon& ic = icon(cur);
    const bool marked = ic.state == tasks::IconState::Marked;
    c_.card(kMargin, 598, 1280 - 2 * kMargin, 52, 26, col::card, 2, col::line);
    c_.text(cur.name, Size::Body, col::text, kMargin + 28, 610, Align::Left, 800);
    if (marked) {
        c_.checkBadge(1280 - kMargin - 170, 624, 14, col::green, col::bg);
        c_.text(tr(Id::Marked), Size::Body, col::green, 1280 - kMargin - 140, 610);
    } else {
        c_.text(tr(Id::NotMarked), Size::Body, col::muted, 1280 - kMargin - 28, 610, Align::Right);
    }
    hints({{"A", tr(marked ? Id::HintUnmark : Id::HintMark)}, {"B", tr(Id::HintBack)}, {"D-pad", tr(Id::HintSelect)},
           {"L / R", tr(Id::HintPage)}});
    c_.text(std::to_string(markSel_ + 1) + " / " + std::to_string(n), Size::Small, col::muted, 1280 - kMargin, 108, Align::Right);
}
