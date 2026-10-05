"""Debug menu for the simulator: the real tree of src/ui/MenuTree.h, with the
key rules of src/logic/MenuLogic.h (Python copy of MenuNav, kept simple).

The firmware logic itself is tested by the native tests (test/test_logic);
this copy only lets you walk through the real menus and labels in a browser.
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MENU_TREE_H = os.path.join(ROOT, "src", "ui", "MenuTree.h")
LINES = 6


def _split_args(text):
    """Splits macro arguments on commas, outside of double quotes."""
    args, current, quoted = [], "", False
    for ch in text:
        if ch == '"':
            quoted = not quoted
        elif ch == "," and not quoted:
            args.append(current.strip())
            current = ""
            continue
        current += ch
    args.append(current.strip())
    return args


def parse_tree():
    source = open(MENU_TREE_H, encoding="utf-8").read()
    names = re.search(r"enum MenuIndex[^{]*\{([^}]*)\}", source).group(1)
    index = {name.strip(): i for i, name in enumerate(n for n in names.split(",") if n.strip())}
    array = source.split("MENU_TREE[] = {", 1)[1].split("};", 1)[0]
    items = []
    for kind, body in re.findall(r"M_(MENU|CMD|ASK|INFO|ADJ)\((.*)\),\s*$", array, re.M):
        a = _split_args(body)
        text = lambda s: None if s == "nullptr" else s.strip('"')
        item = {"en": text(a[0]), "fr": text(a[1]), "kind": kind,
                "parent": -1 if a[2] == "-1" else index[a[2]]}
        if kind in ("CMD", "ASK"):
            item["command"] = text(a[3])
        elif kind == "INFO":
            item["page"] = a[3]
        elif kind == "ADJ":
            item.update(command=text(a[3]), save=text(a[4]), key=text(a[5]),
                        min=int(a[6]), max=int(a[7]), step=int(a[8]))
        items.append(item)
    return items


class MenuNav:
    def __init__(self, items, execute, get_value, info_line):
        self.items, self.execute, self.get_value, self.info_line = items, execute, get_value, info_line
        self.mode, self.menu, self.cursor, self.active, self.value = "browse", 0, 0, 0, 0
        self.answer = ""

    def children(self, parent):
        return [i for i, item in enumerate(self.items) if item["parent"] == parent]

    def run(self, command):
        self.answer = (self.execute(command) or "").split("\n")[0] if command else ""

    def key(self, key):
        """key: 'a', 'al' (A long), 'b', 'bl' (B long)."""
        if self.mode == "info":
            if key in ("a", "al"):
                kids = self.children(self.items[self.active]["parent"])
                pos = kids.index(self.active)
                for _ in kids:
                    pos = (pos + (1 if key == "a" else -1)) % len(kids)
                    if self.items[kids[pos]]["kind"] == "INFO":
                        self.active, self.cursor = kids[pos], pos
                        break
            else:
                self.mode = "browse"
        elif self.mode == "adjust":
            item = self.items[self.active]
            if key in ("a", "b"):
                self.value = max(item["min"], min(item["max"], self.value + (item["step"] if key == "b" else -item["step"])))
                self.run(item["command"] % self.value)
            else:
                if key == "bl" and item["save"]:
                    self.run(item["save"] % self.value)
                self.mode = "browse"
        elif self.mode == "confirm":
            if key == "b":
                self.run(self.items[self.active]["command"])
            self.mode = "browse"
        else:
            kids = self.children(self.menu)
            if key == "bl":
                parent = self.items[self.menu]["parent"]
                if parent >= 0:
                    self.cursor = self.children(parent).index(self.menu)
                    self.menu = parent
            elif not kids:
                return
            elif key == "a":
                self.cursor = (self.cursor + 1) % len(kids)
            elif key == "al":
                self.cursor = (self.cursor - 1) % len(kids)
            else:
                index = kids[self.cursor]
                item = self.items[index]
                if item["kind"] == "MENU":
                    self.menu, self.cursor = index, 0
                elif item["kind"] == "CMD":
                    self.run(item["command"])
                elif item["kind"] == "ASK":
                    self.active, self.mode = index, "confirm"
                elif item["kind"] == "INFO":
                    self.active, self.mode = index, "info"
                else:
                    self.active, self.mode = index, "adjust"
                    self.value = max(item["min"], min(item["max"], self.get_value(item["key"])))
                    self.answer = ""

    def render(self, french):
        lang = "fr" if french else "en"
        lines, highlight = [""] * LINES, -1
        if self.mode == "browse":
            lines[0] = self.items[self.menu][lang]
            kids = self.children(self.menu)
            first = 0 if self.cursor < 4 else self.cursor - 3
            for row, index in enumerate(kids[first:first + 4]):
                lines[1 + row] = self.items[index][lang] + (" >" if self.items[index]["kind"] == "MENU" else "")
                if first + row == self.cursor:
                    highlight = 1 + row
            lines[5] = self.answer or ("A:suiv  B:ok" if french else "A:next  B:ok")
            return lines, highlight
        item = self.items[self.active]
        lines[0] = item[lang]
        if self.mode == "info":
            for i in range(4):
                lines[1 + i] = self.info_line(item["page"], i, french)
            lines[5] = "A:page  B:retour" if french else "A:page  B:back"
        elif self.mode == "adjust":
            lines[2] = "      <  %d  >" % self.value
            lines[4] = self.answer
            if item["save"]:
                lines[5] = "A:- B:+ B long:sauve" if french else "A:- B:+ hold B:save"
            else:
                lines[5] = "A:- B:+ B long:fin" if french else "A:- B:+ hold B:done"
        else:
            lines[2] = "Confirmer ?" if french else "Are you sure?"
            lines[5] = "B:oui   A:non" if french else "B:yes   A:no"
        return lines, highlight
