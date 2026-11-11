#include "texts.h"

namespace strings {
namespace {

Lang g_lang = Lang::En;

struct Row {
    const char* en;
    const char* pt;
};

// Same order as enum Id.
const Row kRows[] = {
    {"Tidy up your Wii U Home menu", "Organize o menu Home do seu Wii U"},
    {"Reading your library...", "Lendo sua biblioteca..."},
    {"Working...", "Trabalhando..."},
    {"Sort Home", "Ordenar a Home"},
    {"Put the games of every Home folder in alphabetical order. Games of one series go by release year.",
     "Coloca os jogos de cada pasta da Home em ordem alfabética. Jogos da mesma série seguem o ano de lançamento."},
    {"Mark as completed", "Marcar como zerado"},
    {"Add a green check to the icon of a game you finished. You can remove it later.",
     "Põe um check verde no ícone de um jogo que você zerou. Dá para remover depois."},
    {"%d games in %d folders", "%d jogos em %d pastas"},
    {"%d games found", "%d jogos encontrados"},
    {"Select", "Escolher"},
    {"Back", "Voltar"},
    {"Exit", "Sair"},
    {"Language", "Idioma"},
    {"Apply", "Aplicar"},
    {"Layout file", "Arquivo de layout"},
    {"Folder", "Pasta"},
    {"Scroll", "Rolar"},
    {"Page", "Página"},
    {"Mark", "Marcar"},
    {"Unmark", "Desmarcar"},
    {"Confirm", "Confirmar"},
    {"Cancel", "Cancelar"},
    {"Close app", "Fechar o app"},
    {"%d games", "%d jogos"},
    {"%d to move", "%d a mover"},
    {"Already sorted", "Já está em ordem"},
    {"Skipped (unexpected layout)", "Ignorada (layout inesperado)"},
    {"was %d", "era %d"},
    {"not installed", "não instalado"},
    {"Everything is already in order. Nothing to write.", "Tudo já está em ordem. Nada a gravar."},
    {"No Home layout file was found on this console.", "Nenhum arquivo de layout da Home foi encontrado neste console."},
    {"SD card (Homebrew on Menu)", "Cartão SD (Homebrew on Menu)"},
    {"Console memory", "Memória do console"},
    {"Apply the new order?", "Aplicar a nova ordem?"},
    {"A backup of the current layout is saved to the SD card first.",
     "Antes disso, um backup do layout atual é salvo no cartão SD."},
    {"New order saved", "Nova ordem salva"},
    {"Close this app now. The Home menu reads the new order when it starts again.",
     "Feche este app agora. O menu Home lê a nova ordem quando abrir de novo."},
    {"Backup", "Backup"},
    {"Checked again after 2 seconds: still there.", "Conferido de novo após 2 segundos: continua lá."},
    {"Checked again after 2 seconds: the console changed the file back!",
     "Conferido de novo após 2 segundos: o console desfez a mudança!"},
    {"Close app", "Fechar o app"},
    {"Could not finish", "Não foi possível concluir"},
    {"The file could not be read.", "Não foi possível ler o arquivo."},
    {"The file changed while this screen was open. Go back and try again.",
     "O arquivo mudou com esta tela aberta. Volte e tente de novo."},
    {"Could not save the backup. Nothing was changed.", "Não foi possível salvar o backup. Nada foi alterado."},
    {"Could not write the file. The backup is on the SD card.",
     "Não foi possível gravar o arquivo. O backup está no cartão SD."},
    {"The file read back different from what was written.", "O arquivo lido de volta difere do que foi gravado."},
    {"Nothing to do.", "Nada a fazer."},
    {"No saved original icon exists for this game.", "Não existe ícone original salvo para este jogo."},
    {"This icon format is not supported.", "Este formato de ícone não é suportado."},
    {"Completed", "Zerado"},
    {"Not marked", "Sem marca"},
    {"Marked as completed", "Marcado como zerado"},
    {"Mark removed", "Marca removida"},
    {"Already marked", "Já está marcado"},
    {"No installed games were found.", "Nenhum jogo instalado foi encontrado."},
    {"Could not open the console storage. Is this running under Aroma?",
     "Não foi possível abrir o armazenamento do console. Isto está rodando no Aroma?"},
};

static_assert(sizeof(kRows) / sizeof(kRows[0]) == static_cast<int>(Id::Count_), "string table out of sync");

}  // namespace

Lang current() { return g_lang; }
void set(Lang lang) { g_lang = lang; }

const char* get(Id id) {
    const Row& r = kRows[static_cast<int>(id)];
    return g_lang == Lang::Pt ? r.pt : r.en;
}

}  // namespace strings
