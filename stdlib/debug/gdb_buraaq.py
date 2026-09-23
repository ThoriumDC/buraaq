"""GDB pretty-printers for Buraaq text, Option, Result, and vec handles."""

import gdb


class TextPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        if int(self.val) == 0:
            return "null"
        try:
            return self.val.string()
        except gdb.error:
            return "<unreadable>"


class OptionPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        p = int(self.val)
        if p == 0:
            return "None"
        try:
            tag = gdb.Value(p).cast(gdb.lookup_type("int").pointer()).dereference()
            return "Some(tag=%s)" % int(tag)
        except gdb.error:
            return "Option(?)"


def lookup(val):
    t = str(val.type)
    if t.find("char") >= 0 and "*" in t:
        return TextPrinter(val)
    if t.find("i8") >= 0 and "*" in t:
        return TextPrinter(val)
    if "Option" in t or "Result" in t:
        return OptionPrinter(val)
    return None


gdb.pretty_printers.append(lookup)
print("buraaq GDB pretty-printers loaded (text / Option)")
