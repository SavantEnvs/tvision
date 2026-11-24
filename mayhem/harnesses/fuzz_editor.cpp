// Fuzz TEditor, the text-editing engine behind TMemo/TFileEditor (gap buffer, line-ending
// detection/conversion in insertText, UTF-8 aware cursor motion, word/line deletion, selection,
// search/replace, formatLine drawing). Driven headlessly inside a TGroup with its own draw buffer,
// as upstream's test/tvision/teditor.test.cpp does.
// Input: [w][h][textLen][findLen][flags] <initial text> <find string> then 3-byte events
// (kind, a, b): typed keys / editor commands.
#define Uses_TGroup
#define Uses_TEditor
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TRect
#define Uses_TCommandSet
#define Uses_TView
#include <tvision/tv.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace {
class FuzzGroup : public TGroup
{
public:
    FuzzGroup(const TRect &bounds) : TGroup(bounds) {}
    TColorAttr mapColor(uchar i) noexcept override { return TColorAttr(i); }
    // Events are fed directly to the editor; nothing else may block on an event loop.
    void getEvent(TEvent &event) override { event.what = evNothing; }
    void putEvent(TEvent &) override {}
};

const ushort commands[] = {
    cmCharLeft, cmCharRight, cmWordLeft, cmWordRight, cmLineStart, cmLineEnd, cmLineUp,
    cmLineDown, cmPageUp, cmPageDown, cmTextStart, cmTextEnd, cmNewLine, cmBackSpace, cmDelChar,
    cmDelWord, cmDelStart, cmDelEnd, cmDelLine, cmInsMode, cmStartSelect, cmHideSelect,
    cmIndentMode, cmSelectAll, cmDelWordLeft, cmSearchAgain, cmUndo, cmClear, cmCut, cmCopy, cmPaste,
};

// TEditor::updateCommands enables/disables commands in the process-global TView::curCommandSet;
// saved once at start-up and restored before every input.
TCommandSet initialCommandSet;
Boolean initialCommandSetChanged;
} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    initialCommandSet = TView::curCommandSet;
    initialCommandSetChanged = TView::commandSetChanged;
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 5)
        return -1;
    int w = 1 + data[0] % 80, h = 1 + data[1] % 25;
    size_t textLen = data[2] * 4, findLen = data[3] % 80;
    uint8_t flags = data[4];
    data += 5; size -= 5;
    if (textLen > size) textLen = size;
    TStringView text((const char *) data, textLen);
    data += textLen; size -= textLen;
    if (findLen > size) findLen = size;

    // Process-global editor state: reset per input so results depend only on this input.
    TView::curCommandSet = initialCommandSet;
    TView::commandSetChanged = initialCommandSetChanged;
    memset(TEditor::findStr, 0, sizeof(TEditor::findStr));
    memset(TEditor::replaceStr, 0, sizeof(TEditor::replaceStr));
    memcpy(TEditor::findStr, data, findLen < sizeof(TEditor::findStr) - 1 ? findLen : sizeof(TEditor::findStr) - 1);
    TEditor::editorFlags = flags & (efCaseSensitive | efWholeWordsOnly);
    data += findLen; size -= findLen;

    TRect bounds(0, 0, w, h);
    FuzzGroup *group = new FuzzGroup(bounds);
    group->setState(sfExposed, True);
    group->getBuffer();
    TEditor *editor = new TEditor(bounds, nullptr, nullptr, nullptr, 1024);
    group->insert(editor);
    // An application clipboard editor (as tvedit sets up), so cut/copy/paste stay in-process
    // instead of reaching the system clipboard, which needs a running TProgram/terminal.
    TEditor *clip = new TEditor(TRect(0, 0, 1, 1), nullptr, nullptr, nullptr, 1024);
    group->insert(clip);
    TEditor::clipboard = clip;
    editor->insertText(text.data(), text.size(), Boolean(flags & 0x80));
    editor->setCurPtr(0, 0);

    for (size_t i = 0; i + 3 <= size; i += 3)
    {
        TEvent ev {};
        uint8_t kind = data[i], a = data[i + 1], b = data[i + 2];
        if (kind & 1)
        {
            ev.what = evCommand;
            ev.message.command = commands[a % (sizeof(commands) / sizeof(commands[0]))];
        }
        else
        {
            ev.what = evKeyDown;
            ev.keyDown.keyCode = ushort(a | (b << 8));
            ev.keyDown.controlKeyState = ushort((kind >> 1) & 0x0F);
            if (kind & 0x20)
            {
                ev.keyDown.charScan.charCode = a;
                ev.keyDown.text[0] = (char) a;
                ev.keyDown.text[1] = (char) b;
                ev.keyDown.textLength = (b & 0x80) ? 2 : 1;
            }
        }
        editor->handleEvent(ev);
        if (kind & 0x40)
            editor->scrollTo(a % 64, b % 64);
    }
    editor->drawView();
    TEditor::clipboard = nullptr;
    TObject::destroy(group);
    return 0;
}
