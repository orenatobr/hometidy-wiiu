// Host-side tests for the pure logic (no console needed). Run with tests/run.sh.
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../source/badge.h"
#include "../source/default_rules.h"
#include "../source/fsx.h"
#include "../source/layout.h"
#include "../source/sorter.h"
#include "../source/tasks.h"
#include "../source/titles.h"

static int g_failed = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            ++g_failed;                                                      \
        }                                                                    \
    } while (0)

using Bytes = std::vector<std::uint8_t>;

// ---------------------------------------------------------------- layout --

static void put32(Bytes& b, std::size_t off, std::uint32_t v) {
    b[off] = v >> 24; b[off + 1] = v >> 16; b[off + 2] = v >> 8; b[off + 3] = v;
}

static Bytes makeLayout(int folders = 60) {
    Bytes b(layout::kFolderBase + std::size_t(folders) * layout::kFolderStride, 0xAA);
    for (int f = 1; f <= folders; ++f) {
        const std::size_t base = layout::kFolderBase + std::size_t(f - 1) * layout::kFolderStride;
        for (std::size_t i = 0; i < layout::kFolderSlots; ++i) {
            for (int side = 0; side < 2; ++side) {
                const std::size_t e = base + (side * layout::kFolderSlots + i) * layout::kEntrySize;
                put32(b, e, 0); put32(b, e + 4, 0); put32(b, e + 8, side ? 2 : 1); put32(b, e + 12, 0);
            }
        }
    }
    return b;
}

static void setEntry(Bytes& b, int folder, bool usb, int slot, std::uint32_t hi, std::uint32_t lo) {
    const std::size_t base = layout::kFolderBase + std::size_t(folder - 1) * layout::kFolderStride;
    const std::size_t e = base + ((usb ? layout::kFolderSlots : 0) + slot) * layout::kEntrySize;
    put32(b, e, hi); put32(b, e + 4, lo); put32(b, e + 8, usb ? 2 : 1);
}

static void testLayout() {
    CHECK(!layout::Layout::parse(Bytes(100)));
    CHECK(!layout::Layout::parse(Bytes(layout::kFolderBase + 5)));

    Bytes raw = makeLayout();
    setEntry(raw, 1, true, 0, 0x50000, 0xC);
    setEntry(raw, 1, false, 1, 0x50000, 0xB);   // installed on NAND
    setEntry(raw, 1, true, 2, 0x50000, 0xA);
    // folder name "Wii U" in UTF-16BE
    const std::size_t info = layout::kFolderBase + layout::kInfoOffset;
    const char16_t nm[] = u"Wii é";
    for (int i = 0; i < 5; ++i) { raw[info + i * 2] = nm[i] >> 8; raw[info + i * 2 + 1] = nm[i] & 0xFF; }
    raw[info + 10] = 0; raw[info + 11] = 0;

    auto l = layout::Layout::parse(raw);
    CHECK(l && l->folderCount() == 60);
    auto f = l->folder(1);
    CHECK(f.name == "Wii \xC3\xA9");
    CHECK(f.items.size() == 3 && !f.ambiguous);
    CHECK(f.items[1].id.lo == 0xB && !f.items[1].onUsb);
    CHECK(l->folder(2).items.empty());

    // reorder: A, B, C -> keeps each side
    const std::vector<layout::TitleId> order = {{0x50000, 0xA}, {0x50000, 0xB}, {0x50000, 0xC}};
    CHECK(l->setOrder(1, order));
    auto g = l->folder(1);
    CHECK(g.items[0].id.lo == 0xA && g.items[0].onUsb);
    CHECK(g.items[1].id.lo == 0xB && !g.items[1].onUsb);
    CHECK(g.items[2].id.lo == 0xC && g.items[2].onUsb);
    // only the 120 entries of folder 1 differ
    std::size_t outside = 0;
    const std::size_t lo = layout::kFolderBase, hi = lo + 2 * layout::kFolderSlots * layout::kEntrySize;
    for (std::size_t i = 0; i < raw.size(); ++i)
        if (raw[i] != l->bytes()[i] && (i < lo || i >= hi)) ++outside;
    CHECK(outside == 0);
    // identity order is a no-op
    auto before = l->bytes();
    CHECK(l->setOrder(1, order));
    CHECK(before == l->bytes());
    // rejected orders leave the file untouched
    CHECK(!l->setOrder(1, {{0x50000, 0xA}, {0x50000, 0xB}}));
    CHECK(!l->setOrder(1, {{0x50000, 0xA}, {0x50000, 0xB}, {0x50000, 0xD}}));
    CHECK(!l->setOrder(1, {{0x50000, 0xA}, {0x50000, 0xA}, {0x50000, 0xB}}));
    CHECK(!l->setOrder(99, {}));
    CHECK(before == l->bytes());
    // ambiguous folder
    setEntry(raw, 3, true, 0, 0x50000, 1);
    setEntry(raw, 3, false, 0, 0x50000, 2);
    auto amb = layout::Layout::parse(raw);
    CHECK(amb->folder(3).ambiguous);
    CHECK(!amb->setOrder(3, {{0x50000, 1}, {0x50000, 2}}));
}

// ---------------------------------------------------------------- sorter --

static void testSorter() {
    CHECK(sorter::normalize("The Legend of Zelda: Breath of the Wild") == "legend of zelda breath of the wild");
    CHECK(sorter::normalize("Pok\xC3\xA9mon\xE2\x84\xA2 Rumble") == "pokemon rumble");
    CHECK(sorter::normalize("Luigi's  Mansion") == "luigis mansion");
    CHECK(sorter::normalize("A") == "a");

    auto rules = sorter::parseRules("# c\nzelda | zelda | 1990\n bad line\nmario kart 8 | mario kart | 2014 # x\n");
    CHECK(rules.size() == 2 && rules[1].year == 2014 && rules[1].series == "mario kart");

    const auto def = sorter::parseRules(kDefaultRules);
    CHECK(def.size() > 80);
    std::vector<std::string> names = {
        "The Legend of Zelda: Breath of the Wild", "Super Mario 3D World", "Xenoblade Chronicles",
        "Hyrule Warriors", "Mario Kart 8", "Mario Kart Wii", "Super Mario Galaxy 2", "Super Mario Galaxy",
        "Zelda: The Wind Waker HD", "Bayonetta 2", "Angry Birds", "Pok\xC3\xA9mon Rumble U"};
    std::sort(names.begin(), names.end(), [&](const std::string& a, const std::string& b) {
        return sorter::less(sorter::makeKey(a, def), sorter::makeKey(b, def));
    });
    const std::vector<std::string> want = {
        "Angry Birds", "Bayonetta 2", "Super Mario Galaxy", "Super Mario Galaxy 2", "Super Mario 3D World",
        "Mario Kart Wii", "Mario Kart 8", "Pok\xC3\xA9mon Rumble U", "Xenoblade Chronicles",
        "Zelda: The Wind Waker HD", "Hyrule Warriors", "The Legend of Zelda: Breath of the Wild"};
    CHECK(names == want);
    if (names != want) for (auto& n : names) std::printf("   got: %s\n", n.c_str());

    // natural numbers inside the same series/year
    CHECK(sorter::less(sorter::makeKey("Game 2", {}), sorter::makeKey("Game 10", {})));
}

// ---------------------------------------------------------------- titles --

static void testTitles() {
    const std::string xml =
        "<menu><longname_ja>ja</longname_ja><longname_en>Super\n  Mario &amp; Co &#233;</longname_en></menu>";
    CHECK(titles::nameFromMeta(xml) == "Super Mario & Co \xC3\xA9");
    CHECK(titles::nameFromMeta("<longname_ja>Nihon</longname_ja>") == "Nihon");
    CHECK(titles::nameFromMeta("<x/>").empty());

    const std::string root = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/hometidy_titles_test";
    fsx::makeDirs(root + "/00050000/10143600/meta");
    fsx::makeDirs(root + "/00050002/524d4350/meta");
    fsx::makeDirs(root + "/00050000/ffffffff/meta");  // no meta.xml: ignored
    const std::string a = "<longname_en>Wii Game</longname_en>", b = "<longname_en>Wii U Game</longname_en>";
    fsx::writeBytes(root + "/00050000/10143600/meta/meta.xml", Bytes(b.begin(), b.end()));
    fsx::writeBytes(root + "/00050002/524d4350/meta/meta.xml", Bytes(a.begin(), a.end()));
    auto cat = titles::scan({root});
    CHECK(cat.size() == 2);
    CHECK(cat[0x0005000010143600ULL].name == "Wii U Game");
    CHECK(cat[0x00050002524d4350ULL].name == "Wii Game");
    CHECK(cat[0x00050002524d4350ULL].dir == root + "/00050002/524d4350");
}

// ----------------------------------------------------------------- badge --

static Bytes makeTga(int w, int bpp, bool topDown) {
    Bytes f(18, 0);
    f[2] = 2; f[12] = w & 255; f[13] = w >> 8; f[14] = w & 255; f[15] = w >> 8; f[16] = bpp;
    f[17] = (bpp == 32 ? 8 : 0) | (topDown ? 0x20 : 0);
    for (int i = 0; i < w * w; ++i) {
        f.push_back(10); f.push_back(20); f.push_back(30);
        if (bpp == 32) f.push_back(255);
    }
    const char footer[] = "TRUEVISION-XFILE.";
    f.insert(f.end(), footer, footer + sizeof footer);
    return f;
}

static void testBadge() {
    for (bool topDown : {false, true}) {
        for (int bpp : {32, 24}) {
            Bytes f = makeTga(128, bpp, topDown);
            const Bytes orig = f;
            CHECK(!badge::isMarked(f));
            CHECK(badge::apply(f));
            CHECK(badge::isMarked(f));
            CHECK(f.size() == orig.size());
            CHECK(std::equal(orig.begin(), orig.begin() + 18, f.begin()));            // header
            CHECK(std::equal(orig.end() - 18, orig.end(), f.end() - 18));             // footer
            // pixels changed only in the top-right corner
            Bytes rgba, rgba0;
            int w = 0, h = 0;
            CHECK(badge::decode(f, rgba, w, h) && w == 128 && h == 128);
            badge::decode(orig, rgba0, w, h);
            int changed = 0, outside = 0;
            for (int y = 0; y < 128; ++y)
                for (int x = 0; x < 128; ++x) {
                    const bool diff = std::memcmp(&rgba[(y * 128 + x) * 4], &rgba0[(y * 128 + x) * 4], 4) != 0;
                    changed += diff;
                    outside += diff && (x < 78 || y > 50);
                }
            CHECK(changed > 800 && outside == 0);
            // top-down vs bottom-up: the badge is at the visual top-right either way
            const auto g = badge::geometryFor(128);
            CHECK(rgba[(g.cy * 128 + g.cx - g.radius / 2) * 4 + 1] > 150);  // green channel high
        }
    }
    Bytes rle = makeTga(128, 32, false);
    rle[2] = 10;
    const Bytes keep = rle;
    CHECK(!badge::apply(rle) && rle == keep);
    CHECK(!badge::isMarked(Bytes(5)));
    // tiny or huge sizes are rejected before any pixel is read
    for (int w : {1, 8, 31, 1025}) {
        Bytes small = makeTga(w > 1024 ? 1 : w, 32, false);
        small[12] = w & 255; small[13] = w >> 8; small[14] = w & 255; small[15] = w >> 8;
        const Bytes keepSmall = small;
        CHECK(!badge::isMarked(small) && !badge::apply(small) && small == keepSmall);
    }
}


// ----------------------------------------------------------------- tasks --

static void testTasks() {
    const std::string root = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/hometidy_tasks_test";
    std::system(("rm -rf '" + root + "'").c_str());
    const std::string sd = root + "/sd", mlc = root + "/mlc";
    const std::string dir = sd + "/wiiu/homebrew_on_menu_plugin/FEH1/save/80000006";
    fsx::makeDirs(dir);
    fsx::makeDirs(mlc + "/usr/save/00050010/10040200/user/80000006");

    Bytes raw = makeLayout();
    setEntry(raw, 1, true, 0, 0x50000, 0x30);   // "Zelda Game"
    setEntry(raw, 1, true, 1, 0x50000, 0x10);   // "Banana"
    setEntry(raw, 1, false, 2, 0x50000, 0x20);  // "Apple" (NAND)
    setEntry(raw, 1, true, 3, 0x50000, 0x99);   // not installed
    setEntry(raw, 2, true, 0, 0x50000, 0x11);   // single title folder: unchanged
    fsx::writeBytes(dir + "/BaristaAccountSaveFile.dat", raw);
    fsx::writeBytes(mlc + "/usr/save/00050010/10040200/user/80000006/BaristaAccountSaveFile.dat", raw);

    auto found = tasks::findLayoutFiles(sd, mlc);
    CHECK(found.size() == 2 && found[0].onSd && !found[1].onSd);

    std::map<std::uint64_t, titles::Title> cat;
    cat[0x0005000000000030ULL] = {0x0005000000000030ULL, "Zelda Game", ""};
    cat[0x0005000000000010ULL] = {0x0005000000000010ULL, "Banana", ""};
    cat[0x0005000000000020ULL] = {0x0005000000000020ULL, "Apple", ""};
    cat[0x0005000000000011ULL] = {0x0005000000000011ULL, "Solo", ""};
    const auto rules = sorter::parseRules(kDefaultRules);

    auto plan = tasks::planSort(raw, cat, rules);
    CHECK(plan && plan->folders.size() == 2 && plan->changedFolders == 1);
    const auto& rows = plan->folders[0].rows;
    CHECK(rows.size() == 4 && rows[0].name == "Apple" && rows[1].name == "Banana" &&
          rows[2].name == "Zelda Game" && !rows[3].known);
    CHECK(!plan->folders[1].changed);
    CHECK(!tasks::planSort(Bytes(10), cat, rules));

    const std::string file = found[0].path, backups = sd + "/wiiu/apps/HomeTidy/backup";
    auto res = tasks::applySort(file, *plan, backups, "T1");
    CHECK(res.status == tasks::Status::Ok);
    CHECK(tasks::fileEquals(file, plan->sorted));
    CHECK(tasks::fileEquals(res.backupPath, raw));
    // second apply: the file no longer matches the plan (console-side change detection)
    CHECK(tasks::applySort(file, *plan, backups, "T2").status == tasks::Status::Changed);
    // sorting again is a no-op
    auto again = tasks::planSort(plan->sorted, cat, rules);
    CHECK(again && again->changedFolders == 0);
    CHECK(tasks::applySort(file, *again, backups, "T3").status == tasks::Status::NothingToDo);

    // icons
    titles::Title t{0x0005000010143600ULL, "Game", sd + "/title"};
    fsx::makeDirs(t.dir + "/meta");
    CHECK(tasks::iconState(t) == tasks::IconState::Unreadable);
    const Bytes icon = makeTga(128, 32, false);
    fsx::writeBytes(t.dir + "/meta/iconTex.tga", icon);
    CHECK(tasks::iconState(t) == tasks::IconState::Plain);
    CHECK(tasks::unmarkIcon(t, backups).status == tasks::Status::NoBackup);
    CHECK(tasks::markIcon(t, backups).status == tasks::Status::Ok);
    CHECK(tasks::iconState(t) == tasks::IconState::Marked);
    CHECK(tasks::markIcon(t, backups).status == tasks::Status::NothingToDo);
    CHECK(tasks::unmarkIcon(t, backups).status == tasks::Status::Ok);
    CHECK(tasks::fileEquals(t.dir + "/meta/iconTex.tga", icon));
    CHECK(tasks::iconState(t) == tasks::IconState::Plain);
    // marking again keeps the very first original
    CHECK(tasks::markIcon(t, backups).status == tasks::Status::Ok);
    CHECK(tasks::unmarkIcon(t, backups).status == tasks::Status::Ok);
    CHECK(tasks::fileEquals(t.dir + "/meta/iconTex.tga", icon));
}

// ------------------------------------------------- real files (optional) --

static void testRealFiles() {
    if (const char* p = std::getenv("HOMETIDY_REAL_LAYOUT")) {
        Bytes raw;
        CHECK(fsx::readBytes(p, raw));
        auto l = layout::Layout::parse(raw);
        CHECK(l && l->folderCount() == 60);
        for (int n = 1; n <= l->folderCount(); ++n) {
            auto f = l->folder(n);
            std::vector<layout::TitleId> ids;
            for (auto& it : f.items) ids.push_back(it.id);
            CHECK(!f.ambiguous && l->setOrder(n, ids));
            std::reverse(ids.begin(), ids.end());
            CHECK(l->setOrder(n, ids));
            std::reverse(ids.begin(), ids.end());
            CHECK(l->setOrder(n, ids));
        }
        CHECK(l->bytes() == raw);  // round trip is byte exact
        std::printf("real layout: folder1=%zu items, folder2=%zu items\n", l->folder(1).items.size(),
                    l->folder(2).items.size());
    }
    if (const char* p = std::getenv("HOMETIDY_REAL_ICON")) {
        Bytes raw;
        CHECK(fsx::readBytes(p, raw));
        const bool was = badge::isMarked(raw);
        Bytes marked = raw;
        CHECK(badge::apply(marked) && badge::isMarked(marked) && marked.size() == raw.size());
        std::printf("real icon: already marked=%d\n", was);
    }
}

int main() {
    testLayout();
    testSorter();
    testTitles();
    testBadge();
    testTasks();
    testRealFiles();
    std::printf(g_failed ? "%d FAILED\n" : "all tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
