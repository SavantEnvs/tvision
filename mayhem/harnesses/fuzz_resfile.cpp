// Fuzz the resource-file reader: TResourceFile parses the file header (skipping an MZ/EXE stub and
// foreign "FB" info blocks), loads the TResourceCollection index through the ipstream object reader,
// and get(key) deserializes the stored objects through each registered class's read(). Driven like examples/tvforms/listdlg.cpp: the input is written to a temp file and
// opened as an fpstream.
#define Uses_TResourceFile
#define Uses_TStringList
#define Uses_fpstream
#define Uses_TStreamableClass
#define Uses_TGroup
#define Uses_TView
#define Uses_TRect
#define Uses_TEvent
#define Uses_THardwareInfo
#define Uses_TCommandSet
#include <tvision/tv.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

// TNSCollection::error() (tcollect.cpp) reports a bad collection index/limit with exit(212 - code).
// Lift an exit() raised while an input is being processed to abort(), so it is reported as a crash
// with a backtrace instead of an uncategorized "fuzz target exited" (docs: lift exit to abort).
// Registered on the first input, i.e. after libFuzzer's own atexit hook, so it runs first.
static bool inInput = false;
static void abortOnExitDuringInput() { if (inInput) abort(); }

// Process-global command state: inserting a loaded view whose stored state has sfActive set calls
// setState(sfActive) (tgroup.cpp insertBefore), and TWindow/TInputLine/TEditor then enable/disable
// commands in TView::curCommandSet. TButton::read and TMenuItem consult that set, so it is saved
// once at start-up and restored before every input: each input's result depends only on it.
static TCommandSet initialCommandSet;
static Boolean initialCommandSetChanged;

// Library classes registered for reading — the string lists and the views/dialogs/controls/menus/
// validators applications store as resources (examples/tvforms stores whole dialogs). All have
// TObject as their first base, so an object read as void* is a TObject*. The file/directory
// dialogs are left out: they need a running TProgram.
__link( RView )
__link( RGroup )
__link( RFrame )
__link( RWindow )
__link( RDialog )
__link( RBackground )
__link( RButton )
__link( RCluster )
__link( RCheckBoxes )
__link( RRadioButtons )
__link( RMultiCheckBoxes )
__link( RStaticText )
__link( RParamText )
__link( RLabel )
__link( RInputLine )
__link( RHistory )
__link( RScrollBar )
__link( RScroller )
__link( RListViewer )
__link( RListBox )
__link( RSortedListBox )
__link( RMenuView )
__link( RMenuBar )
__link( RMenuBox )
__link( RMenuPopup )
__link( RStatusLine )
__link( RIndicator )
__link( REditor )
__link( RMemo )
__link( ROutline )
__link( RStringCollection )
__link( RStringList )
__link( RResourceCollection )
__link( RValidator )
__link( RFilterValidator )
__link( RRangeValidator )
__link( RPXPictureValidator )
__link( RLookupValidator )
__link( RStringLookupValidator )

namespace {
// Headless owner for loaded views, as fuzz_editor.cpp / upstream's teditor.test.cpp do: applications
// insert resource dialogs into the desktop before showing or closing them, so a loaded view is
// inserted here and torn down with the group. Drawing stops at this group's own buffer.
class FuzzGroup : public TGroup
{
public:
    FuzzGroup(const TRect &bounds) : TGroup(bounds) {}
    TColorAttr mapColor(uchar i) noexcept override { return TColorAttr(i); }
    void getEvent(TEvent &event) override { event.what = evNothing; }
    void putEvent(TEvent &) override {}
};
} // namespace

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    // Every TApplication constructs one THardwareInfo (tapplica.cpp: `static THardwareInfo
    // hwInfoManager`). Its constructor only binds the Platform singleton; no console is set up, so
    // caret updates from TView::resetCursor (which hiding a focused view always calls, owner or not)
    // land in the in-memory DisplayBuffer instead of a null platform pointer.
    static THardwareInfo hwInfo;
    (void) hwInfo;
    initialCommandSet = TView::curCommandSet;
    initialCommandSetChanged = TView::commandSetChanged;
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static bool hooked = (atexit(abortOnExitDuringInput) == 0);
    (void) hooked;
    TView::curCommandSet = initialCommandSet;
    TView::commandSetChanged = initialCommandSetChanged;
    const char *tmpdir = getenv("TMPDIR");
    std::string path = std::string(tmpdir && *tmpdir ? tmpdir : "/tmp") + "/fuzz_resfile.XXXXXX";
    int fd = mkstemp(&path[0]);
    if (fd < 0)
        return 0;
    for (size_t off = 0; off < size;)
    {
        ssize_t w = write(fd, data + off, size - off);
        if (w <= 0) break;
        off += (size_t) w;
    }
    close(fd);
    inInput = true;
    {
        fpstream *s = new fpstream(path.c_str(), ios::in | ios::binary);
        TResourceFile *rf = new TResourceFile(s); // owns and deletes s
        short n = rf->count();
        for (short i = 0; i < n && i < 32; ++i)
        {
            const char *key = rf->keyAt(i);
            if (!key)
                continue;
            void *p = rf->get(key);
            if (!p)
                continue;
            TObject *obj = (TObject *) p;
            if (TView *view = dynamic_cast<TView *>(obj))
            {
                FuzzGroup *group = new FuzzGroup(TRect(0, 0, 80, 25));
                group->setState(sfExposed, True);
                group->getBuffer();
                group->insert(view);
                TObject::destroy(group); // shutDown hides and destroys the view too
            }
            else
                TObject::destroy(obj);
        }
        delete rf;
    }
    inInput = false;
    unlink(path.c_str());
    return 0;
}
