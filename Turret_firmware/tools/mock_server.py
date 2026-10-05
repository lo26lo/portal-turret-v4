"""Turret2 web page test bench, without the board.

Serves src/web/page/index.html and simulates the firmware API (web/TurretWebServer.cpp,
control/Actions.cpp) with plausible, moving values. The settings are read from
src/settings/Settings.cpp so that the page shows the real list.

    python tools/mock_server.py [port]      (default 8080, run from Turret_firmware/)
    open http://localhost:8080  -  user "turret", password "stillalive"

Simulation commands that do not exist on the turret (type them in Tests > console):
    sim fault on|off     PWR_FLT low / high
    sim radar on|off     radar frames or silence
    sim imu on|off       IMU present or missing
    sim crash on|off     fake core dump and previous-run log
    sim debug on|off     debug mode (SW1): menu on the screen, or status page (default on)
    sim oled on|off      OLED plugged in or not

The debug menu (Tests > Debug screen) walks through the real tree of
src/ui/MenuTree.h (tools/mock_menu.py).
"""
import base64
import json
import math
import os
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

import mock_menu

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAGE = os.path.join(ROOT, "src", "web", "page", "index.html")
SETTINGS_CPP = os.path.join(ROOT, "src", "settings", "Settings.cpp")
USER = "turret"
SERVO_NAMES = ["rotz", "rotx", "gunl", "gunr", "wingl", "wingr"]
FAULT_NAMES = {1: "brownout / reboot loop", 2: "eFuse", 3: "IMU missing", 4: "radar silent", 5: "Hall", 6: "LittleFS"}

lock = threading.RLock()  # re-entrant: status() may log
start = time.time()
log_lines = []


def log(text):
    with lock:
        for line in str(text).splitlines() or [""]:
            log_lines.append(line)
        del log_lines[:-200]
    print(text)


def parse_settings():
    """Rows of the Settings constructor: {"Key", "Label", "Group", SettingType::X, ...}."""
    source = open(SETTINGS_CPP, encoding="utf-8").read()
    rows = []
    pattern = re.compile(r'\{"(\w+)",\s*"([^"]*)",\s*"(\w+)",\s*SettingType::(\w+),\s*([^{}]*?)\}')
    for key, label, group, kind, rest in pattern.findall(source):
        values = [v.strip() for v in rest.split(",")]
        entry = {"key": key, "label": label, "group": group, "type": kind.lower().replace("str", "string")}

        def num(text):
            text = re.sub(r"\(\w+\)", "", text).rstrip("f")
            return float(text) if kind == "Float" else int(text)

        if kind == "Str":
            entry["default"] = values[0].strip('"')
        elif kind == "Bool":
            entry["default"] = values[0] == "true"
        else:
            entry["default"] = num(values[0])
            entry["min"] = num(values[1])
            entry["max"] = num(values[2])
        entry["value"] = entry["default"]
        entry["reboot"] = group == "WiFi"
        if key in ("ApPassword", "StaPassword"):
            entry["secret"] = True
        rows.append(entry)
    return rows


settings = parse_settings()
state = {
    "state": "Idle", "pwrFlt": False, "radar": True, "imu": True, "muted": False, "gain": 9,
    "wingPos": 0.0, "wingTarget": 0.0, "servos": {n: None for n in SERVO_NAMES}, "powerFaults": 0,
    "wifiOn": True, "joinAt": None, "timeSet": False, "scanUntil": 0, "crash": False,
    "debug": True, "fullPower": False, "oled": True,
}


def setting(key):
    return next(s for s in settings if s["key"] == key)


def password():
    return setting("ApPassword")["value"] or "stillalive"


FAKE_NETWORKS = [
    {"ssid": "Livebox-3F2A", "rssi": -52, "secure": True},
    {"ssid": "Freebox-Maison", "rssi": -67, "secure": True},
    {"ssid": "Voisins", "rssi": -81, "secure": True},
    {"ssid": "Cafe gratuit", "rssi": -85, "secure": False},
]


def menu_value(key):
    if not key or key.startswith("@"):
        return 90
    return int(setting(key)["value"])


def menu_info(page, line, french):
    """Fake but plausible lines for the information pages of the debug menu."""
    pages = {
        "PageState": [("État: " if french else "State: ") + state["state"],
                      ("Défauts: aucun" if french else "Faults: none"),
                      ("Durée: " if french else "Up: ") + "0 h %02d min %02d" % divmod(int(time.time() - start), 60),
                      "mock"],
        "PagePower": ["PWR_FLT: " + ("BAS" if state["pwrFlt"] else "ok"), "eFuse:%d brownout:0" % state["powerFaults"],
                      ("Boucles:1 alim " if french else "Loops:1 supply ") + ("3 A" if state["fullPower"] else "PC"),
                      "Reset: POWERON"],
        "PageRadar": ["Radar: " + (("vivant" if french else "alive") if state["radar"] else ("muet" if french else "silent")) + ", 1",
                      "1: x-120 y1500 ZONE", "2: -", "3: -"],
        "PageImu": ["x0.1 y-9.8 z0.4", ("Axe: -Y  régl: " if french else "Axis: -Y  set: ") + str(setting("ImuUpAxis")["value"]),
                    ("Debout: oui" if french else "Upright: yes"), "Temp: 31.5 C"],
        "PageHall": [("G: " if french else "L: ") + str(hall("L")), "[" + "#" * (hall("L") * 15 // 4095) + "]",
                     ("D: " if french else "R: ") + str(hall("R")), "[" + "#" * (hall("R") * 15 // 4095) + "]"],
        "PageServos": ["rotz:---- rotx:----", "gunl:---- gunr:----", "wngl:---- wngr:----",
                       ("Alim 3 A: tous" if state["fullPower"] else "Alim PC: 1 servo") if french
                       else ("3 A supply: all" if state["fullPower"] else "PC supply: 1 servo")],
        "PageAudio": ["Gain: %d dB" % state["gain"], "Volume: %s %%" % setting("Volume")["value"],
                      ("Muet: " if french else "Muted: ") + str(state["muted"]), ("Lecture: non" if french else "Playing: no")],
        "PageWifiAp": [setting("ApSsid")["value"], "Mot de passe :" if french else "Password:", password(),
                       "192.168.4.1  1 cli."],
        "PageWifiHome": ([setting("StaSsid")["value"],
                          ("Connecté" if french else "Connected") if sta_connected() else ("Connexion..." if french else "Connecting..."),
                          "192.168.1.42 -58" if sta_connected() else "", "portal-turret.local" if sta_connected() else ""]
                         if setting("StaSsid")["value"] else
                         ["Non réglé" if french else "Not set", "(par la page web)" if french else "(from the web page)", "", ""]),
        "PageWifiScan": (["Scan en cours..." if french else "Scanning...", "", "", ""] if time.time() < state["scanUntil"]
                         else ["%.16s %d" % (n["ssid"], n["rssi"]) for n in FAKE_NETWORKS[:4]] if state["scanUntil"]
                         else ["Aucun (Scanner)" if french else "None (Scan networks)", "", "", ""]),
        "PageTime": ([time.strftime("%Y-%m-%d"), time.strftime("%H:%M:%S"), "NTP: " + ("oui" if french else "yes")]
                     if state["timeSet"] else
                     ["Heure non réglée" if french else "Time not set", "(réseau maison)" if french else "(home network)",
                      "NTP: " + ("non" if french else "no")]) + [str(setting("Timezone")["value"])[:21]],
        "PageNetwork": ["AP: " + setting("ApSsid")["value"], "Clients: 1",
                        "192.168.1.42 -58" if sta_connected() else ("Maison: non réglé" if french else "Home: not set"),
                        time.strftime("%m-%d %H:%M:%S")],
    }
    return pages.get(page, [""] * 4)[line]


def sta_connected():
    """Joins 3 s after "wifi join" if the network exists and the password is
    "password123" (or the network is open)."""
    ssid = setting("StaSsid")["value"]
    if not ssid or state["joinAt"] is None or time.time() - state["joinAt"] < 3:
        return False
    network = next((n for n in FAKE_NETWORKS if n["ssid"] == ssid), None)
    ok = network is not None and (not network["secure"] or setting("StaPassword")["value"] == "password123")
    if ok and not state["timeSet"]:
        state["timeSet"] = True
        log(f'WiFi: connected to "{ssid}", IP 192.168.1.42\nWiFi: http://portal-turret.local\nTime: '
            + time.strftime("%Y-%m-%d %H:%M:%S") + " (NTP)")
    return ok


def hall(side):
    """Closed ~1200, open ~2900, with noise; the right sensor is a bit different."""
    base = 1200 if side == "L" else 1150
    span = 1700 if side == "L" else 1800
    return int(base + span * state["wingPos"] + 25 * math.sin(time.time() * 3 + (0 if side == "L" else 1)))


def simulate():
    while True:
        time.sleep(0.05)
        with lock:
            pos, target = state["wingPos"], state["wingTarget"]
            if abs(target - pos) > 0.01:
                state["wingPos"] = pos + max(-0.04, min(0.04, target - pos))
                state["servos"]["wingl"] = 1975 if target > pos else 1025
                state["servos"]["wingr"] = 1025 if target > pos else 1975
            else:
                state["servos"]["wingl"] = state["servos"]["wingr"] = None


def status():
    uptime = int((time.time() - start) * 1000)
    faults = []
    if state["pwrFlt"]:
        faults.append(2)
    if not state["imu"]:
        faults.append(3)
    if not state["radar"]:
        faults.append(4)
    axis = setting("ImuUpAxis")["value"]
    targets = int(time.time() / 7) % 3 if state["radar"] else 0
    return {
        "version": "mock " + time.strftime("%b %d %Y"),
        "uptimeMs": uptime,
        "state": state["state"],
        "resetReason": "POWERON (power-up or RESET button)",
        "bootLoopCount": 1, "brownouts": 0, "powerFaults": state["powerFaults"],
        "pwrFlt": state["pwrFlt"], "benchMode": False, "reducedMode": False,
        "faults": faults,
        "radar": {"alive": state["radar"], "targets": targets},
        "imu": {"available": state["imu"], "gravity": [0.12, -9.78, 0.35] if state["imu"] else [0, 0, 0],
                "dominantAxis": 4 if state["imu"] else 0, "upright": state["imu"] and axis == 4,
                "temperature": 31.5},
        "hall": {"left": hall("L"), "right": hall("R"), "fault": False},
        "wings": {"leftOpen": state["wingPos"] > 0.9, "rightOpen": state["wingPos"] > 0.9,
                  "moving": abs(state["wingTarget"] - state["wingPos"]) > 0.01},
        "servos": dict(state["servos"]),
        "amp": {"gainDb": state["gain"], "running": not state["muted"], "muted": state["muted"]},
        "wifi": {"on": state["wifiOn"], "ssid": setting("ApSsid")["value"], "clients": 1,
                 "defaultPassword": password() == "stillalive",
                 "sta": {"enabled": bool(setting("StaSsid")["value"]),
                         "connected": sta_connected(),
                         "ssid": setting("StaSsid")["value"],
                         "ip": "192.168.1.42" if sta_connected() else "",
                         "rssi": -58}},
        "time": time.strftime("%Y-%m-%d %H:%M:%S") if state["timeSet"] else "",
        "crash": {"previousLog": state["crash"], "coredump": 24576 if state["crash"] else 0,
                  "summary": "task loopTask, PC 0x42012345, backtrace 0x42012345 0x42009876 0x4200abcd"
                  if state["crash"] else ""},
        "freeHeap": 181234,
    }


def set_value(key, text):
    entry = next((s for s in settings if s["key"].lower() == key.lower()), None)
    if entry is None:
        return f'unknown key "{key}"'
    if entry["type"] == "string":
        if key == "ApPassword" and not 8 <= len(text) <= 63:
            return "refused: ApPassword"
        if key == "StaPassword" and text and not 8 <= len(text) <= 63:
            return "refused: StaPassword"
        entry["value"] = text[:63]
    elif entry["type"] == "bool":
        entry["value"] = text.lower() in ("true", "on", "1")
    else:
        try:
            value = float(text) if entry["type"] == "float" else int(float(text))
        except ValueError:
            value = 0
        entry["value"] = max(entry["min"], min(entry["max"], value))
    shown = "********" if entry.get("secret") else entry["value"]
    return f'{entry["key"]} = {shown}' + (" (applied at the next boot)" if entry["reboot"] else "")


def execute(line):
    words = line.split()
    if not words:
        return ""
    cmd, args = words[0].lower(), words[1:]
    arg = " ".join(args)
    if cmd == "sim" and len(args) == 2:
        on = args[1] == "on"
        if args[0] == "fault":
            state["pwrFlt"] = on
            if on:
                state["powerFaults"] += 1
                state["state"] = "Fault"
                state["servos"] = {n: None for n in SERVO_NAMES}
                return "Fault: load shed (servos detached, amp muted, LEDs off)"
            state["state"] = "Idle"
            return "Fault: PWR_FLT high for 2 s, resuming"
        if args[0] in ("debug", "oled"):
            state[args[0]] = on
            return f"sim {args[0]} {'on' if on else 'off'}"
        if args[0] == "crash":
            state["crash"] = on
            return "sim crash " + ("on: core dump and previous log available" if on else "off")
        if args[0] in ("radar", "imu"):
            state[args[0]] = on
            return f"sim {args[0]} {'on' if on else 'off'}"
    if cmd == "help":
        return "Commands: status | scan | imu | hall | flt | get [key] | set <key> <value> | servo ... | wings open|close | ..."
    if cmd == "status":
        return json.dumps(status())
    if cmd == "scan":
        return "I2C scan: 0x6A\n1 device(s)" if state["imu"] else "I2C scan: no device\n0 device(s)"
    if cmd == "hall":
        return f"left {hall('L')}, right {hall('R')}"
    if cmd == "imu":
        return "gravity 0.12 -9.78 0.35 m/s2, dominant axis 4" if state["imu"] else "IMU not available"
    if cmd == "get":
        return "\n".join(f'{s["key"]} = {"********" if s.get("secret") else s["value"]}'
                         for s in settings if not arg or s["key"].lower() == arg.lower()) or f'unknown key "{arg}"'
    if cmd == "set" and len(args) >= 2:
        return set_value(args[0], " ".join(args[1:]))
    if cmd == "reset-settings":
        for s in settings:
            if not arg or s["group"] == arg:
                s["value"] = s["default"]
        return "settings reset" + (f" ({arg})" if arg else "")
    if cmd == "mute":
        state["muted"] = (arg == "on") if arg in ("on", "off") else not state["muted"]
        return "muted" if state["muted"] else "unmuted"
    if cmd == "gain" and args:
        g = int(args[0])
        state["gain"] = 9 if g <= 10 else 12 if g <= 13 else 15
        return f"gain {state['gain']} dB (not saved)"
    if cmd == "tone":
        return f"tone {args[0] if args else '?'} Hz, {args[1] if len(args) > 1 else 500} ms"
    if cmd == "reboot":
        return "rebooting"
    if cmd == "wifi" and arg == "scan":
        state["scanUntil"] = time.time() + 3
        return "scanning WiFi networks"
    if cmd == "wifi" and arg == "forget":
        setting("StaSsid")["value"] = ""
        setting("StaPassword")["value"] = ""
        state["joinAt"], state["timeSet"] = None, False
        return "home network forgotten"
    if cmd == "wifi" and arg == "join":
        state["joinAt"] = time.time()
        state["timeSet"] = False
        ssid = setting("StaSsid")["value"]
        return f'joining "{ssid}"' if ssid else "home network disabled (StaSsid empty)"
    if cmd == "wifi":
        return "access point on, clients: 1; home network: " + (
            "not configured" if not setting("StaSsid")["value"] else setting("StaSsid")["value"])
    if cmd == "key" and arg in ("a", "al", "b", "bl"):
        if state["debug"]:
            menu.key(arg)
        return ""
    if cmd == "power":
        if arg in ("full", "limited"):
            state["fullPower"] = arg == "full"
        return "debug mode: " + ("all servos allowed (3 A supply)" if state["fullPower"] else "one servo at a time (weak supply)")
    if cmd in ("wing", "gun") and len(args) == 2:
        if cmd == "wing":
            state["wingTarget"] = 1.0 if args[1] == "open" else 0.0
        return f"{cmd} {args[0]}: {args[1]}"
    if cmd in ("wings", "guns", "loadtest", "demo") and state["debug"] and not state["fullPower"]:
        return "refused: one servo at a time (power full first)"
    if cmd == "servos" and arg == "off":
        state["servos"] = {n: None for n in SERVO_NAMES}
        return "all servos released"
    if cmd == "shot":
        return "gunshot, 1 s"
    if cmd == "cal":
        if arg == "imu":
            setting("ImuUpAxis")["value"] = 4
            return "ImuUpAxis = 4"
        if arg == "hall save":
            return "Hall saved: L R"
        if len(args) == 3 and args[0] == "hall":
            return f"{args[1]} {args[2]} = {hall('L' if args[1] == 'left' else 'R')}"
        return "usage: cal hall left|right open|closed | cal hall save | cal imu"
    if cmd == "coredump" and arg == "erase":
        state["crash"] = False
        return "core dump erased"
    if cmd == "time":
        return time.strftime("%Y-%m-%d %H:%M:%S") if state["timeSet"] else "time not set (no NTP yet)"
    if state["state"] == "Fault" and cmd in ("servo", "led", "wings", "guns", "trim", "demo"):
        return "refused: power fault"
    if cmd == "resume":
        state["state"] = "Idle"
        state["wingTarget"] = 0.0
        return "resuming (homing, then Idle)"
    if cmd == "demo":
        return "demo cycle"
    state["state"] = "Manual"
    if cmd == "wings" and arg in ("open", "close"):
        state["wingTarget"] = 1.0 if arg == "open" else 0.0
        return "opening wings" if arg == "open" else "closing wings"
    if cmd == "guns" and arg in ("extend", "retract"):
        return "extending guns" if arg == "extend" else "retracting guns"
    if cmd == "servo" and len(args) == 2 and args[0] in SERVO_NAMES + [str(i) for i in range(6)]:
        name = SERVO_NAMES[int(args[0])] if args[0].isdigit() else args[0]
        if args[1] == "off":
            state["servos"][name] = None
            return f"{name} released"
        v = int(args[1])
        us = round(500 + v * 1900 / 180) if v <= 180 else v
        state["servos"][name] = us
        return f"{name} -> {us} us"
    if cmd == "led":
        if arg == "pattern bit":
            return "LED pattern: ring LED 1 = red 0x80, all else 0 (9th bit of 24 set, GRB order)"
        return "LED test off" if "off" in args else f"LED {arg}"
    if cmd == "sweep":
        return "sweep stopped" if arg == "stop" else f"sweep {arg} (simulated)"
    if cmd == "loadtest" and args:
        return f"load test: 6 servos, {args[0]} ms apart (simulated)"
    if cmd == "trim" and len(args) == 2:
        return f"wing {args[0]} at 1500 {args[1]} us for 2 s (not saved: set WingTrimL/R)"
    return f'unknown command "{cmd}" (help)'


def menu_execute(command):
    log(f"menu> {command}")
    answer = execute(command)
    log(answer)
    return answer


menu = mock_menu.MenuNav(mock_menu.parse_tree(), menu_execute, menu_value, menu_info)


def screen():
    french = int(setting("Language")["value"]) != 0
    # Wing animation (logic::RenderWings): text version of what the OLED draws.
    moving = abs(state["wingTarget"] - state["wingPos"]) > 0.01
    if moving:
        state["animUntil"] = time.time() + 0.8
    if setting("OledAnim")["value"] and time.time() < state.get("animUntil", 0):
        percent = int(max(0.0, min(1.0, state["wingPos"])) * 100)
        gap = percent * 5 // 100
        art = " " * (7 - gap) + "[]" + "=" * gap + "(O)" + "=" * gap + "[]"
        side = ("G", "D") if french else ("L", "R")
        lines = ["Ailes" if french else "Wings", "", art, "", "%s %3d%%     %s %3d%%" % (side[0], percent, side[1], percent),
                 (("en mouvement..." if french else "moving...") if moving else ("arrêtées" if french else "stopped"))]
        return {"present": state["oled"], "menu": state["debug"], "highlight": -1, "lines": lines, "animation": True}
    if state["debug"]:
        lines, highlight = menu.render(french)
    else:
        lines, highlight = ["Portal Turret", menu_info("PageState", 0, french), menu_info("PageState", 1, french),
                            menu_info("PageNetwork", 2, french), menu_info("PageNetwork", 3, french), "mock"], -1
    return {"present": state["oled"], "menu": state["debug"], "highlight": highlight, "lines": lines}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def authorized(self):
        header = self.headers.get("Authorization", "")
        if header.startswith("Basic "):
            user, _, pw = base64.b64decode(header[6:]).decode("utf-8", "replace").partition(":")
            if user == USER and pw == password():
                return True
        self.send_response(401)
        self.send_header("WWW-Authenticate", 'Basic realm="Portal Turret"')
        self.send_header("Content-Length", "0")
        self.end_headers()
        return False

    def reply(self, code, body, kind="application/json"):
        data = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        self.wfile.write(data)

    def form(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode("utf-8", "replace")
        return {k: v[-1] for k, v in parse_qs(body, keep_blank_values=True).items()}

    def queue(self, line):
        # The firmware executes queued lines in loop() and logs the answer.
        log(f"web> {line}")
        with lock:
            answer = execute(line)
        log(answer)

    def do_GET(self):
        # Captive portal checks: redirected without credentials, like the firmware.
        if self.path in ("/generate_204", "/gen_204", "/hotspot-detect.html",
                         "/library/test/success.html", "/connecttest.txt", "/ncsi.txt", "/fwlink"):
            self.send_response(302)
            self.send_header("Location", "/")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if not self.authorized():
            return
        if self.path == "/api/screen":
            with lock:
                self.reply(200, json.dumps(screen()))
        elif self.path == "/api/crashlog":
            text = ("I (8123) Wing Movement Timeout\nWiFi: lost \"Livebox-3F2A\", retrying\n"
                    "Servo rotate X attached (1450 us)\n") if state["crash"] else ""
            self.reply(200, text, "text/plain; charset=utf-8")
        elif self.path == "/api/coredump":
            if state["crash"]:
                self.reply(200, b"\x7fELF" + bytes(24572), "application/octet-stream")
            else:
                self.reply(404, "no core dump", "text/plain")
        elif self.path == "/api/wifi/scan":
            with lock:
                scanning = time.time() < state["scanUntil"]
                self.reply(200, json.dumps({"scanning": scanning, "networks": [] if scanning else FAKE_NETWORKS}))
        elif self.path == "/":
            self.reply(200, open(PAGE, "rb").read(), "text/html; charset=utf-8")
        elif self.path == "/api/status":
            with lock:
                self.reply(200, json.dumps(status()))
        elif self.path == "/api/settings":
            with lock:
                rows = [dict(s, value="" if s.get("secret") else s["value"],
                             default="" if s.get("secret") else s["default"]) for s in settings]
            self.reply(200, json.dumps(rows))
        elif self.path == "/api/log":
            with lock:
                self.reply(200, "\n".join(log_lines) + "\n", "text/plain; charset=utf-8")
        else:
            self.reply(404, "not found", "text/plain")

    def do_POST(self):
        if self.path == "/update":
            length = int(self.headers.get("Content-Length", 0))
            self.rfile.read(length)
            if not self.authorized():
                return
            log(f"OTA start ({length} bytes)\nOTA done (simulated), rebooting")
            self.reply(200, "Update OK, rebooting (simulated)", "text/plain")
            return
        if not self.authorized():
            return
        data = self.form()
        if self.path == "/api/settings":
            for key, value in data.items():
                self.queue(f"set {key} {value}")
            self.reply(202, json.dumps({"queued": len(data), "refused": 0}))
        elif self.path == "/api/settings/reset":
            self.queue(("reset-settings " + data.get("group", "")).strip())
            self.reply(202, '{"queued":1}')
        elif self.path == "/api/action":
            if "cmd" not in data:
                self.reply(400, '{"error":"cmd missing"}')
                return
            self.queue(data["cmd"])
            self.reply(202, '{"queued":1}')
        elif self.path == "/api/coredump/erase":
            self.queue("coredump erase")
            self.reply(202, '{"queued":1}')
        elif self.path == "/api/reboot":
            self.queue("reboot")
            self.reply(202, '{"queued":1}')
        else:
            self.reply(404, "not found", "text/plain")


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    log(f"This is a triumph (mock). {len(settings)} settings read from Settings.cpp")
    threading.Thread(target=simulate, daemon=True).start()
    print(f"Mock turret on http://localhost:{port}  (user {USER}, password {password()})")
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
