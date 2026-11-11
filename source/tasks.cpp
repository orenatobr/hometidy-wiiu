#include "tasks.h"

#include <algorithm>
#include <cstdio>

#include "badge.h"
#include "fsx.h"

namespace tasks {
namespace {

const char* kLayoutName = "BaristaAccountSaveFile.dat";

std::string hex16(std::uint64_t v) {
    char b[17];
    std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
    return b;
}

std::string iconPath(const titles::Title& t) { return t.dir + "/meta/iconTex.tga"; }

std::string iconBackupPath(const titles::Title& t, const std::string& backupDir) {
    return backupDir + "/icons/" + hex16(t.id) + "_iconTex_original.tga";
}

}  // namespace

std::vector<LayoutFile> findLayoutFiles(const std::string& sdRoot, const std::string& mlcRoot) {
    std::vector<LayoutFile> out;
    const std::string plugin = sdRoot + "/wiiu/homebrew_on_menu_plugin";
    for (const std::string& serial : fsx::listDir(plugin)) {
        const std::string saveDir = plugin + "/" + serial + "/save";
        for (const std::string& account : fsx::listDir(saveDir)) {
            const std::string p = saveDir + "/" + account + "/" + kLayoutName;
            if (fsx::exists(p)) out.push_back({p, true});
        }
    }
    for (const char* region : {"10040000", "10040100", "10040200"}) {
        const std::string userDir = mlcRoot + "/usr/save/00050010/" + region + "/user";
        for (const std::string& account : fsx::listDir(userDir)) {
            const std::string p = userDir + "/" + account + "/" + kLayoutName;
            if (fsx::exists(p)) out.push_back({p, false});
        }
    }
    return out;
}

std::optional<SortPlan> planSort(const std::vector<std::uint8_t>& layoutBytes,
                                 const std::map<std::uint64_t, titles::Title>& catalog,
                                 const std::vector<sorter::Rule>& rules) {
    auto parsed = layout::Layout::parse(layoutBytes);
    if (!parsed) return std::nullopt;
    layout::Layout lay = *parsed;

    SortPlan plan;
    plan.original = layoutBytes;
    for (int n = 1; n <= lay.folderCount(); ++n) {
        const layout::Folder f = lay.folder(n);
        if (f.items.empty()) continue;

        FolderPlan fp;
        fp.number = n;
        fp.name = f.name;
        fp.skipped = f.ambiguous;

        struct Entry {
            PlanRow row;
            sorter::Key key;
        };
        std::vector<Entry> entries;
        for (std::size_t i = 0; i < f.items.size(); ++i) {
            const layout::TitleId id = f.items[i].id;
            Entry e;
            e.row.id = id;
            e.row.from = int(i);
            auto it = catalog.find(id.value());
            e.row.known = it != catalog.end();
            e.row.name = e.row.known ? it->second.name : hex16(id.value());
            e.key = sorter::makeKey(e.row.name, rules);
            if (!e.row.known) e.key = {"~" + e.row.name, 0, e.row.name};  // not installed: keep last
            entries.push_back(e);
        }
        if (!fp.skipped) {
            std::stable_sort(entries.begin(), entries.end(),
                             [](const Entry& a, const Entry& b) { return sorter::less(a.key, b.key); });
        }
        std::vector<layout::TitleId> order;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            entries[i].row.to = int(i);
            fp.changed = fp.changed || entries[i].row.from != entries[i].row.to;
            order.push_back(entries[i].row.id);
            fp.rows.push_back(entries[i].row);
        }
        if (fp.changed && !lay.setOrder(n, order)) {
            fp.changed = false;
            fp.skipped = true;
        }
        plan.changedFolders += fp.changed ? 1 : 0;
        plan.folders.push_back(std::move(fp));
    }
    plan.sorted = lay.bytes();
    return plan;
}

bool fileEquals(const std::string& path, const std::vector<std::uint8_t>& expected) {
    std::vector<std::uint8_t> now;
    return fsx::readBytes(path, now) && now == expected;
}

Result applySort(const std::string& path, const SortPlan& plan, const std::string& backupDir,
                 const std::string& stamp) {
    Result r;
    if (plan.changedFolders == 0) return {Status::NothingToDo, ""};
    std::vector<std::uint8_t> now;
    if (!fsx::readBytes(path, now)) return {Status::ReadFailed, ""};
    if (now != plan.original) return {Status::Changed, ""};

    fsx::makeDirs(backupDir);
    r.backupPath = backupDir + "/BaristaAccountSaveFile_" + stamp + ".dat";
    if (!fsx::writeBytes(r.backupPath, now) || !fileEquals(r.backupPath, now)) {
        return {Status::BackupFailed, ""};
    }
    if (!fsx::writeBytes(path, plan.sorted)) return {Status::WriteFailed, r.backupPath};
    if (!fileEquals(path, plan.sorted)) return {Status::VerifyFailed, r.backupPath};
    r.status = Status::Ok;
    return r;
}

IconState iconState(const titles::Title& title) {
    std::vector<std::uint8_t> f;
    std::vector<std::uint8_t> rgba;
    int w = 0, h = 0;
    if (!fsx::readBytes(iconPath(title), f) || !badge::decode(f, rgba, w, h)) return IconState::Unreadable;
    return badge::isMarked(f) ? IconState::Marked : IconState::Plain;
}

Result markIcon(const titles::Title& title, const std::string& backupDir) {
    std::vector<std::uint8_t> f;
    if (!fsx::readBytes(iconPath(title), f)) return {Status::ReadFailed, ""};
    if (badge::isMarked(f)) return {Status::NothingToDo, ""};
    std::vector<std::uint8_t> marked = f;
    if (!badge::apply(marked)) return {Status::Unsupported, ""};

    Result r;
    r.backupPath = iconBackupPath(title, backupDir);
    if (!fsx::exists(r.backupPath)) {  // keep the very first original
        fsx::makeDirs(backupDir + "/icons");
        if (!fsx::writeBytes(r.backupPath, f) || !fileEquals(r.backupPath, f)) return {Status::BackupFailed, ""};
    }
    if (!fsx::writeBytes(iconPath(title), marked)) return {Status::WriteFailed, r.backupPath};
    if (!fileEquals(iconPath(title), marked)) return {Status::VerifyFailed, r.backupPath};
    return r;
}

Result unmarkIcon(const titles::Title& title, const std::string& backupDir) {
    Result r;
    r.backupPath = iconBackupPath(title, backupDir);
    std::vector<std::uint8_t> original;
    if (!fsx::readBytes(r.backupPath, original)) return {Status::NoBackup, ""};
    if (!fsx::writeBytes(iconPath(title), original)) return {Status::WriteFailed, r.backupPath};
    if (!fileEquals(iconPath(title), original)) return {Status::VerifyFailed, r.backupPath};
    return r;
}

}  // namespace tasks
