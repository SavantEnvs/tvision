// Fuzz tvision's UTF-8 text measuring and drawing: every string a view shows (labels, menu items,
// editor/terminal text, file names) goes through TText (width/next/prev/scroll/drawStr) and
// TDrawBuffer::moveStr/moveCStr. Input: 4 control bytes, then the text.
#define Uses_TText
#define Uses_TDrawBuffer
#define Uses_TScreenCell
#include <tvision/tv.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4)
        return -1;
    int count = (int8_t) data[0];
    size_t indent = data[1] % 40;
    size_t textIndent = data[2];
    unsigned cells = 1 + data[3] % 80;
    TStringView text((const char *) data + 4, size - 4);

    (void) TText::width(text);
    (void) TText::measure(text);
    size_t i = 0, w = 0;
    while (TText::next(text, i, w)) {}
    for (size_t j = text.size(); j > 0;)
    {
        size_t k = TText::prev(text, j);
        if (k == 0) break;
        j -= k;
    }
    size_t sw = 0, sb = 0;
    TText::scroll(text, count, Boolean(count & 1), sb, sw);
    (void) TText::scroll(text, count, False);
    (void) TText::toCodePage(text);

    std::vector<TScreenCell> buf(cells);
    TText::drawStr(TSpan<TScreenCell>(buf.data(), buf.size()), indent, text, textIndent);
    TText::drawStr(TSpan<TScreenCell>(buf.data(), buf.size()), indent, text, (int) textIndent, TColorAttr(0x1F));

    TDrawBuffer b;
    b.moveStr(indent, text, 0x1F, cells, textIndent);
    b.moveCStr(indent, text, TAttrPair(0x1F, 0x2E), cells, textIndent);
    return 0;
}
