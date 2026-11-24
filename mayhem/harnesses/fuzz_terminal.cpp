// Fuzz TTerminal, the scrolling text view applications pipe program output into through otstream
// (do_sputn's circular buffer, bufInc/canInsert, nextLine/prevLines line walking), as upstream's
// test/tvision/textview.test.cpp TestTerminal does. Input: 1 byte buffer-size selector, then chunks
// "<len byte><bytes>" written to the terminal; after each chunk the line structure is walked.
#define Uses_TRect
#define Uses_TTerminal
#include <tvision/tv.h>
#include <stddef.h>
#include <stdint.h>

namespace {
struct FuzzTerminal : TTerminal
{
    FuzzTerminal(ushort bufSize) : TTerminal(TRect(0, 0, 0, 0), nullptr, nullptr, bufSize) {}
    void walk()
    {
        ushort pos = queFront;
        for (int i = 0; i < 64 && pos != queBack; ++i)
            pos = prevLines(pos, 1);
        pos = queBack;
        for (int i = 0; i < 64 && pos != queFront; ++i)
            pos = nextLine(pos);
        (void) prevLines(queFront, 3);
    }
};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1)
        return -1;
    FuzzTerminal t(1 + data[0] % 64);
    size_t i = 1;
    while (i < size)
    {
        size_t len = data[i++];
        if (len > size - i)
            len = size - i;
        t.do_sputn((const char *) data + i, (int) len);
        i += len;
        t.walk();
    }
    return 0;
}
