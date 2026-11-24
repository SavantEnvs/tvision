// Fuzz the Paradox picture-mask validator (TPXPictureValidator: syntaxCheck/process/iteration/group/
// scan) and the range/filter validators that check what a user types into a TInputLine.
// Input: <picture> NUL <text>; the first byte selects options.
#define Uses_TValidator
#define Uses_TPXPictureValidator
#define Uses_TFilterValidator
#define Uses_TRangeValidator
#include <tvision/tv.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1)
        return -1;
    uint8_t flags = data[0];
    const char *p = (const char *) data + 1;
    size_t n = size - 1;
    const char *sep = (const char *) memchr(p, '\0', n);
    if (!sep)
        return -1;
    std::string pic(p, sep - p);
    std::string text(sep + 1, n - (sep - p) - 1);
    text = text.substr(0, strnlen(text.c_str(), text.size()));
    // TInputLine hands validators a buffer of maxLen + 1 (255 at most); picture() may autofill
    // literal characters into it.
    if (pic.size() > 255 || text.size() > 255)
        return -1;

    TPXPictureValidator v(pic, Boolean(flags & 1));
    char buf[256 * 4];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, text.c_str(), text.size());
    (void) v.isValidInput(buf, Boolean(flags & 2));
    memcpy(buf, text.c_str(), text.size() + 1);
    (void) v.isValid(buf);
    (void) v.picture(buf, Boolean(flags & 4));

    TFilterValidator f(pic);
    memcpy(buf, text.c_str(), text.size() + 1);
    (void) f.isValidInput(buf, False);
    if (!(flags & 8))
    {
        // TRangeValidator::isValid pops an error messageBox (needs a TProgram) only via error();
        // isValidInput/isValid are pure.
        TRangeValidator r((int8_t) flags, 1000 + flags);
        memcpy(buf, text.c_str(), text.size() + 1);
        (void) r.isValidInput(buf, False);
        (void) r.isValid(buf);
    }
    return 0;
}
