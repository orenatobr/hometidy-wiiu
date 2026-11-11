// UI texts in English and Brazilian Portuguese.
#pragma once

namespace strings {

enum class Lang { En, Pt };

enum class Id {
    AppTagline,
    Loading,
    Busy,
    SortTitle,
    SortDesc,
    MarkTitle,
    MarkDesc,
    SortStatus,        // "%d games in %d folders"
    MarkStatus,        // "%d games found"
    HintSelect,
    HintBack,
    HintExit,
    HintLanguage,
    HintApply,
    HintLayout,
    HintFolder,
    HintScroll,
    HintPage,
    HintMark,
    HintUnmark,
    HintConfirm,
    HintCancel,
    HintClose,
    FolderGames,       // "%d games"
    FolderMoves,       // "%d to move"
    FolderSorted,
    FolderSkipped,
    WasPosition,       // "was %d"
    NotInstalled,
    AlreadySorted,
    NoLayout,
    LayoutSd,
    LayoutMlc,
    ConfirmTitle,
    ConfirmBody,
    DoneTitle,
    DoneBody,
    DoneBackup,
    DoneRecheckOk,
    DoneRecheckBad,
    DoneClose,
    FailTitle,
    StReadFailed,
    StChanged,
    StBackupFailed,
    StWriteFailed,
    StVerifyFailed,
    StNothing,
    StNoBackup,
    StUnsupported,
    Marked,
    NotMarked,
    ToastMarked,
    ToastUnmarked,
    ToastAlready,
    NoTitles,
    NoAccess,
    Count_
};

/// Current language.
Lang current();
void set(Lang lang);

/// Text for the current language.
const char* get(Id id);

}  // namespace strings
