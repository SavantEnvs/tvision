// Fuzz the compiled help-file reader (.hlp, as produced by tvhc and opened by THelpViewer apps):
// THelpFile parses the header + THelpIndex, getTopic() deserializes THelpTopic objects (paragraphs,
// cross refs) through the ipstream object reader, then the topic is wrapped/laid out as THelpViewer
// would. Driven like examples/tvdemo/tvdemo1.cpp, over an in-memory stream instead of a file.
#define Uses_TStreamableClass
#define Uses_iopstream
#define Uses_TPoint
#include <tvision/tv.h>
#include <tvision/help.h>
#include <tvision/helpbase.h>
#include <stddef.h>
#include <stdint.h>
#include <sstream>
#include <stdlib.h>
#include <string>

// TNSCollection::error() (tcollect.cpp) reports a bad collection index/limit with exit(212 - code).
// Lift an exit() raised while an input is being processed to abort(), so it is reported as a crash
// with a backtrace instead of an uncategorized "fuzz target exited" (docs: lift exit to abort).
// Registered on the first input, i.e. after libFuzzer's own atexit hook, so it runs first.
static bool inInput = false;
static void abortOnExitDuringInput() { if (inInput) abort(); }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static bool hooked = (atexit(abortOnExitDuringInput) == 0);
    (void) hooked;
    inInput = true;
    std::stringbuf sb(std::string((const char *) data, size),
                      std::ios::in | std::ios::out | std::ios::binary);
    // THelpFile owns its stream and deletes it (as THelpViewer apps hand it a new fpstream).
    iopstream *s = new iopstream(&sb);
    THelpFile *hf = new THelpFile(*s);
    // Keep the in-memory buffer unchanged on destruction (THelpFile rewrites a "modified" file).
    hf->modified = False;
    // Look topics up by context number the way THelpViewer/THelpWindow apps do (help.cpp:
    // getTopic(context)); THelpFile resolves each through its THelpIndex. Contexts past the index
    // yield the library's "invalid topic" page, which is laid out like any other topic.
    for (int i = 0; i < 64; ++i)
    {
        THelpTopic *t = hf->getTopic(i);
        if (!t)
            continue;
        for (int w : {78, 20})
        {
            t->setWidth(w);
            int lines = t->numLines();
            for (int l = 1; l <= lines && l <= 512; ++l)
                (void) t->getLine(l);
            (void) t->longestLineWidth();
        }
        int refs = t->getNumCrossRefs();
        for (int r = 0; r < refs && r < 256; ++r)
        {
            TPoint loc; uchar len; int ref;
            t->getCrossRef(r, loc, len, ref);
        }
        delete t;
    }
    delete hf;
    inInput = false;
    return 0;
}
