// Fuzz TCommandSet's public API (enableCmd/disableCmd/has/+=/-= and set algebra) with arbitrary
// command numbers, as an application feeding command ids through TView::enableCommand etc. would.
// This is the code upstream fixed in 6141d9e1a4 (out-of-bounds access for commands outside 0..255).
// Input: a stream of 5-byte records: op byte + little-endian int32 command.
#define Uses_TCommandSet
#include <tvision/tv.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    TCommandSet a, b;
    unsigned acc = 0;
    for (size_t i = 0; i + 5 <= size; i += 5)
    {
        int32_t cmd;
        memcpy(&cmd, data + i + 1, 4);
        TCommandSet &s = (data[i] & 0x80) ? b : a;
        switch (data[i] & 0x0F)
        {
            case 0: s.enableCmd(cmd); break;
            case 1: s.disableCmd(cmd); break;
            case 2: acc += s.has(cmd); break;
            case 3: s += cmd; break;
            case 4: s -= cmd; break;
            case 5: a.enableCmd(b); break;
            case 6: a.disableCmd(b); break;
            case 7: a |= b; break;
            case 8: a &= b; break;
            case 9: acc += (a == b) + a.isEmpty(); break;
            case 10: b = a & b; break;
            case 11: b = a | b; break;
            default: acc += s.has((ushort) cmd); break;
        }
    }
    (void) acc;
    return 0;
}
