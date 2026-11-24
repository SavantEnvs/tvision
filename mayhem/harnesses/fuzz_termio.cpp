// Fuzz the terminal input parser: the byte stream a terminal emulator (or anything writing to the
// tty) sends tvision is decoded by TermIO::parseEvent into key/mouse/paste events (CSI/SS3, kitty,
// win32-input-mode, X10/SGR mouse, far2l, OSC 52 + base64). Driven exactly as upstream's
// test/platform/termio.test.cpp does, with an in-memory InputGetter.
#define Uses_TKeys
#define Uses_TEvent
#include <tvision/tv.h>
#include <internal/termio.h>
#include <stddef.h>
#include <stdint.h>
#include <string>

using namespace tvision;

namespace {
class BufInputGetter : public InputGetter
{
    const uint8_t *data; size_t size; size_t i {0};
public:
    BufInputGetter(const uint8_t *d, size_t s) noexcept : data(d), size(s) {}
    int get() noexcept override { return i < size ? data[i++] : -1; }
    void unget(int) noexcept override { if (i > 0) --i; }
    size_t pos() const noexcept { return i; }
    size_t left() const noexcept { return size - i; }
};

size_t pasted = 0;
void onPaste(TStringView s) { pasted += s.size(); }
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    BufInputGetter in(data, size);
    InputState state {};
    state.putPaste = &onPaste;
    while (in.left() > 0)
    {
        size_t before = in.pos();
        GetChBuf buf(in);
        TEvent ev {};
        ParseResult r = TermIO::parseEvent(buf, ev, state);
        if (in.pos() == before)
            in.get(); // always make progress, like the real input loop discarding a byte
        if (r == Accepted && ev.what == evKeyDown)
            (void) ev.keyDown.getText();
    }
    return 0;
}
