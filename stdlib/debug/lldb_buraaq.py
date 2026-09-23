"""LLDB pretty-printers for Buraaq text, Option, Result, and vec handles."""

import lldb


def _cstr(val):
    try:
        if not val or val.GetValueAsUnsigned() == 0:
            return "null"
        err = lldb.SBError()
        s = val.GetProcess().ReadCStringFromMemory(val.GetValueAsUnsigned(), 4096, err)
        if err.Fail() or s is None:
            return "<unreadable>"
        return s
    except Exception:
        return "<unreadable>"


class TextSynth:
    def __init__(self, valobj, _id):
        self.val = valobj

    def update(self):
        return False

    def num_children(self):
        return 0

    def get_value(self):
        return _cstr(self.val)


class OptionSynth:
    def __init__(self, valobj, _id):
        self.val = valobj

    def update(self):
        return False

    def num_children(self):
        return 0

    def get_value(self):
        p = self.val.GetValueAsUnsigned()
        if p == 0:
            return "None"
        err = lldb.SBError()
        tag = self.val.GetProcess().ReadUnsignedFromMemory(p, 4, err)
        if err.Fail():
            return "Option(?)"
        payload = self.val.GetProcess().ReadUnsignedFromMemory(p + 8, 8, err)
        return "Some(tag=%s payload=%s)" % (tag, payload)


def _vec_summary(valobj, _id):
    p = valobj.GetValueAsUnsigned()
    if p == 0:
        return "vec(null)"
    return "vec@%x" % p


def __lldb_init_module(debugger, _internal):
    debugger.HandleCommand("type summary add -s \"${var%S}\" --inline-children i8*")
    debugger.HandleCommand(
        "type summary add -F lldb_buraaq._cstr i8*"
    )
    debugger.HandleCommand(
        "type synthetic add -l lldb_buraaq.OptionSynth -x \"Option\""
    )
    print("buraaq LLDB pretty-printers loaded (text / Option / vec)")
