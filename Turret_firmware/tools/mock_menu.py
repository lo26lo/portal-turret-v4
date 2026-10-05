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
COLUMNS = 21


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
    """Entries of MENU_TREE: M_MENU, M_CMD, M_ASK, M_INFO, M_ADJ, M_WIZ, M_STEP (may span lines)."""
    source = open(MENU_TREE_H, encoding="utf-8").read()
    names = re.search(r"enum MenuIndex[^{]*\{([^}]*)\}", source).group(1)
    names = re.sub(r"//[^\n]*", "", names)
    index = {name.strip(): i for i, name in enumerate(n for n in names.split(",") if n.strip())}
    array = source.split("MENU_TREE[] = {", 1)[1].split("};", 1)[0]
    array = re.sub(r"^\s*//[^\n]*\n", "", array, flags=re.M)
    items = []
    for kind, body in re.findall(r"M_(MENU|CMD|ASK|INFO|ADJ|WIZ|STEP)\((.*?)\),\s*\n", array, re.S):
        a = _split_args(" ".join(body.split()))
        text = lambda s: None if s == "nullptr" else s.strip('"')
        item = {"en": text(a[0]), "fr": text(a[1]), "kind": kind,
                "parent": -1 if a[2] == "-1" else index[a[2]]}
        if kind in ("CMD", "ASK", "STEP"):
            item["command"] = text(a[3])
        elif kind == "INFO":
            item["page"] = a[3]
        elif kind == "ADJ":
            item.update(command=text(a[3]), save=text(a[4]), key=text(a[5]),
                        min=int(a[6]), max=int(a[7]), step=int(a[8]))
        items.append(item)
    return items


def wrap(text, columns=COLUMNS, max_lines=3):
    """Word wrap, like logic::WrapText."""
    lines, current = [], ""
    for word in text.split():
        if current and len(current) + 1 + len(word) > columns:
            lines.append(current)
            current = word
        else:
            current = (current + " " + word).strip()
    if current:
        lines.append(current)
    return (lines + [""] * max_lines)[:max_lines]


class MenuNav:
    def __init__(self, items, execute, get_value, info_line):
        self.items, self.execute, self.get_value, self.info_line = items, execute, get_value, info_line
        self.mode, self.menu, self.cursor, self.active, self.value, self.step = "browse", 0, 0, 0, 0, 0
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
        elif self.mode == "wizard":
            steps = self.children(self.active)
            if key == "bl" or self.step >= len(steps):
                self.mode = "browse"
            elif key == "al":
                self.step = max(0, self.step - 1)
            else:
                if key == "b":
                    self.run(self.items[steps[self.step]]["command"])
                else:
                    self.answer = ""
                self.step += 1
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
                elif item["kind"] in ("CMD", "STEP"):
                    self.run(item["command"])
                elif item["kind"] == "ASK":
                    self.active, self.mode = index, "confirm"
                elif item["kind"] == "INFO":
                    self.active, self.mode = index, "info"
                elif item["kind"] == "WIZ":
                    self.active, self.mode, self.step, self.answer = index, "wizard", 0, ""
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
        if self.mode == "wizard":
            steps = self.children(self.active)
            if self.step >= len(steps):
                lines[2] = "Terminé" if french else "Done"
                lines[4] = self.answer
                lines[5] = "B:retour" if french else "B:back"
            else:
                lines[0] = "%s %d/%d" % (item[lang], self.step + 1, len(steps))
                lines[1:4] = wrap(self.items[steps[self.step]][lang])
                lines[4] = self.answer
                lines[5] = "B:ok  A:passer" if french else "B:ok  A:skip"
        elif self.mode == "info":
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
