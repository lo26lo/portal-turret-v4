"""Web page check without the board (improvement D2, also run by the CI).

Starts tools/mock_server.py on a free port and checks the API the page relies
on, then checks the syntax of the page JavaScript with Node.js if available.

    python tools/check_web.py        (from Turret_firmware/; exit code 1 on failure)
"""
import base64
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUTH = "Basic " + base64.b64encode(b"turret:stillalive").decode()
failures = []


def check(name, condition, detail=""):
    print(("ok    " if condition else "FAIL  ") + name + (f"  ({detail})" if detail and not condition else ""))
    if not condition:
        failures.append(name)


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


def request(base, path, data=None, auth=True):
    req = urllib.request.Request(base + path, data=urllib.parse.urlencode(data).encode() if data is not None else None)
    if auth:
        req.add_header("Authorization", AUTH)
    opener = urllib.request.build_opener(NoRedirect)
    try:
        with opener.open(req, timeout=5) as r:
            return r.status, r.read().decode("utf-8", "replace"), dict(r.headers)
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace"), dict(e.headers)


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def main():
    port = free_port()
    base = f"http://127.0.0.1:{port}"
    server = subprocess.Popen([sys.executable, os.path.join(ROOT, "tools", "mock_server.py"), str(port)],
                              stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        for _ in range(50):
            try:
                socket.create_connection(("127.0.0.1", port), timeout=0.2).close()
                break
            except OSError:
                time.sleep(0.1)

        code, _, _ = request(base, "/", auth=False)
        check("credentials required", code == 401, code)
        code, body, _ = request(base, "/")
        check("page served", code == 200 and "<script>" in body, code)

        settings_cpp = open(os.path.join(ROOT, "src", "settings", "Settings.cpp"), encoding="utf-8").read()
        expected = len(re.findall(r'\{"\w+",\s*"[^"]*",\s*"\w+",\s*SettingType::', settings_cpp))
        code, body, _ = request(base, "/api/settings")
        rows = json.loads(body)
        check("all settings listed", len(rows) == expected, f"{len(rows)} of {expected}")
        check("passwords never sent", all(r["value"] == "" for r in rows if r.get("secret")))
        keys = [r["key"] for r in rows]
        check("NVS keys at most 15 characters", all(len(k) <= 15 for k in keys), [k for k in keys if len(k) > 15])
        check("keys unique", len(set(keys)) == len(keys))

        request(base, "/api/settings", {"Volume": "150", "ApPassword": "short"})
        _, body, _ = request(base, "/api/settings")
        values = {r["key"]: r["value"] for r in json.loads(body)}
        check("values clamped", values.get("Volume") == 100, values.get("Volume"))
        _, log, _ = request(base, "/api/log")
        check("short password refused", "refused: ApPassword" in log)

        code, _, _ = request(base, "/api/action", {"cmd": "wings open"})
        check("action queued", code == 202, code)
        code, body, _ = request(base, "/api/status")
        status = json.loads(body)
        for field in ("version", "state", "faults", "radar", "imu", "hall", "servos", "amp", "wifi", "time", "crash"):
            check(f"status has {field}", field in status)

        code, _, headers = request(base, "/generate_204", auth=False)
        check("captive portal redirect", code == 302 and "Location" in headers, code)
        code, body, _ = request(base, "/api/wifi/scan")
        check("wifi scan answers", code == 200 and "networks" in json.loads(body), code)

        # Debug menu (lot IM): the tree of src/ui/MenuTree.h against the firmware.
        import mock_menu
        tree = mock_menu.parse_tree()
        check("menu tree parsed", len(tree) > 40 and tree[0]["parent"] == -1, len(tree))
        long_labels = [i[lang] for i in tree for lang in ("en", "fr") if len(i[lang]) > 19]
        check("menu labels fit the screen (19 characters)", not long_labels, long_labels)
        actions_cpp = open(os.path.join(ROOT, "src", "control", "Actions.cpp"), encoding="utf-8").read()
        known = set(re.findall(r'command == "([\w-]+)"', actions_cpp))
        unknown = sorted({i["command"].split()[0] for i in tree if i.get("command")} - known)
        check("every menu command exists in control/Actions.cpp", not unknown, unknown)
        setting_keys = set(keys)
        bad_keys = sorted({i["key"] for i in tree if i["kind"] == "ADJ" and not i["key"].startswith("@")} - setting_keys)
        check("every adjustable value is a setting", not bad_keys, bad_keys)
        orphans = [i["en"] for i in tree if i["parent"] >= 0 and tree[i["parent"]]["kind"] != "MENU"]
        check("every menu entry has a menu as parent", not orphans, orphans)

        code, body, _ = request(base, "/api/screen")
        scr = json.loads(body)
        check("debug screen has six lines", code == 200 and len(scr["lines"]) == 6, code)
        root_title = scr["lines"][0]
        request(base, "/api/action", {"cmd": "key b"})
        _, body, _ = request(base, "/api/screen")
        check("key B enters the first menu", json.loads(body)["lines"][0] != root_title)
        request(base, "/api/action", {"cmd": "key bl"})
        _, body, _ = request(base, "/api/screen")
        check("key B long goes back", json.loads(body)["lines"][0] == root_title)

        code, _, _ = request(base, "/api/coredump")
        check("no core dump -> 404", code == 404, code)
        request(base, "/api/action", {"cmd": "sim crash on"})
        code, body, _ = request(base, "/api/coredump")
        check("core dump download", code == 200 and body.startswith("\x7fELF"), code)
        code, body, _ = request(base, "/api/crashlog")
        check("previous run log", code == 200 and len(body) > 0, code)
        request(base, "/api/coredump/erase", {})
        code, _, _ = request(base, "/api/coredump")
        check("core dump erased", code == 404, code)
    finally:
        server.terminate()
        server.wait(timeout=5)

    node = shutil.which("node")
    if node:
        html = open(os.path.join(ROOT, "src", "web", "page", "index.html"), encoding="utf-8").read()
        script = re.search(r"<script>(.*)</script>", html, re.S).group(1)
        with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as f:
            f.write(script)
        result = subprocess.run([node, "--check", f.name], capture_output=True, text=True)
        os.unlink(f.name)
        check("page JavaScript syntax", result.returncode == 0, result.stderr.strip())
    else:
        print("skip  page JavaScript syntax (node not found)")

    print(f"\n{len(failures)} failure(s)" if failures else "\nall checks passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
