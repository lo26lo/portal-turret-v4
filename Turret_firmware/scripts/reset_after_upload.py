# PlatformIO script: starts the firmware after an upload over the native USB.
#
# On the ESP32-S3 USB-Serial/JTAG, the usual esptool reset after writing
# ("Hard resetting via RTS pin") leaves the chip in download mode: the ROM
# prints "boot:0x20 (DOWNLOAD(USB/UART0))" and the firmware never starts.
# A watchdog reset does start it, but the USB port disappears during that
# reset, so esptool reports a serial error and PlatformIO says FAILED even
# though everything was written and verified (seen on the first board,
# 17.10.2026). Hence: the board definition asks esptool for no reset after
# writing (upload.after_reset = no_reset), and this script then restarts
# the chip with a watchdog reset, ignoring the expected loss of the port.
import os
import subprocess

Import("env")  # noqa: F821 (provided by PlatformIO)


def restart_board(source, target, env):
    if env.subst("$UPLOAD_PROTOCOL") != "esptool":
        return  # OTA uploads restart by themselves
    port = env.subst("$UPLOAD_PORT")
    esptool = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")
    print("Restarting the board (watchdog reset)...")
    result = subprocess.run(
        [env.subst("$PYTHONEXE"), esptool, "--chip", "esp32s3", "--port", port,
         "--before", "no_reset", "--after", "watchdog_reset", "read_mac"],
        capture_output=True, text=True)
    output = result.stdout + result.stderr
    if "Hard resetting with a watchdog" in output:
        print("Board restarted: the firmware is running.")
    else:
        print("Could not restart the board, press RESET (SW3). esptool said:\n" + output[-400:])


env.AddPostAction("upload", restart_board)  # noqa: F821
env.AddPostAction("uploadfs", restart_board)  # noqa: F821
