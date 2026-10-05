#pragma once

// Debug menu tree (improvements M3 to M5), in English and French. Pure data,
// without Arduino: shared by ui/DebugUi and the native tests.
// Every test entry runs a command of control/Actions, so the screen, the
// serial console and the web page behave the same.
// Labels: 19 characters at most (a submenu adds " >").

#include "logic/MenuLogic.h"

namespace ui {

// Information pages (MenuItem::infoPage), filled by DebugUi::InfoLine.
enum InfoPage : int16_t {
  PageState,
  PagePower,
  PageRadar,
  PageImu,
  PageHall,
  PageServos,
  PageAudio,
  PageNetwork,
  PageWifiAp,   // turret access point: name, password, address, clients
  PageWifiHome, // home network: name, state, address, signal
  PageWifiScan, // strongest networks of the last scan
  PageTime,     // date, time, time zone
};

// Positions of the menus in MENU_TREE (parents of the entries below).
enum MenuIndex : int16_t {
  MenuRoot,
  MenuInfo,
  MenuTests,
  MenuCalibration,
  MenuSettings,
  MenuWifi,
  MenuSystem,
  MenuWings,
  MenuGuns,
  MenuServos,
  MenuLeds,
  MenuSound,
};

#define M_MENU(en, fr, parent) \
  { en, fr, logic::ItemKind::Menu, parent, nullptr, nullptr, nullptr, 0, 0, 0, 0 }
#define M_CMD(en, fr, parent, command) \
  { en, fr, logic::ItemKind::Command, parent, command, nullptr, nullptr, 0, 0, 0, 0 }
#define M_ASK(en, fr, parent, command) \
  { en, fr, logic::ItemKind::Confirm, parent, command, nullptr, nullptr, 0, 0, 0, 0 }
#define M_INFO(en, fr, parent, page) \
  { en, fr, logic::ItemKind::Info, parent, nullptr, nullptr, nullptr, page, 0, 0, 0 }
#define M_ADJ(en, fr, parent, format, save, key, min, max, step) \
  { en, fr, logic::ItemKind::Adjust, parent, format, save, key, 0, min, max, step }

static const logic::MenuItem MENU_TREE[] = {
    // Menus first, in the order of MenuIndex.
    M_MENU("Turret debug", "Debug tourelle", -1),
    M_MENU("Information", "Informations", MenuRoot),
    M_MENU("Tests", "Tests", MenuRoot),
    M_MENU("Calibration", "Calibration", MenuRoot),
    M_MENU("Settings", "Réglages", MenuRoot),
    M_MENU("WiFi", "WiFi", MenuRoot),
    M_MENU("System", "Système", MenuRoot),
    M_MENU("Wings", "Ailes", MenuTests),
    M_MENU("Guns", "Canons", MenuTests),
    M_MENU("Servos", "Servos", MenuTests),
    M_MENU("LEDs", "LEDs", MenuTests),
    M_MENU("Sound", "Son", MenuTests),

    // Information
    M_INFO("State", "État", MenuInfo, PageState),
    M_INFO("Power", "Alimentation", MenuInfo, PagePower),
    M_INFO("Radar", "Radar", MenuInfo, PageRadar),
    M_INFO("IMU", "IMU", MenuInfo, PageImu),
    M_INFO("Hall sensors", "Capteurs Hall", MenuInfo, PageHall),
    M_INFO("Servos", "Servos", MenuInfo, PageServos),
    M_INFO("Audio", "Audio", MenuInfo, PageAudio),
    M_INFO("Network", "Réseau", MenuInfo, PageNetwork),

    // Tests (after the submenus Wings, Guns, Servos, LEDs, Sound)
    M_CMD("Demo cycle", "Cycle de démo", MenuTests, "demo"),
    M_ASK("Load test 250 ms", "Test charge 250 ms", MenuTests, "loadtest 250"),
    M_CMD("I2C scan", "Scan I2C", MenuTests, "scan"),
    M_ASK("3 A supply: all", "Alim 3 A : tout", MenuTests, "power full"),
    M_CMD("PC supply: 1 servo", "Alim PC : 1 servo", MenuTests, "power limited"),
    M_CMD("Release all servos", "Relâcher servos", MenuTests, "servos off"),

    M_CMD("Left: open", "Gauche : ouvrir", MenuWings, "wing left open"),
    M_CMD("Left: close", "Gauche : fermer", MenuWings, "wing left close"),
    M_CMD("Right: open", "Droite : ouvrir", MenuWings, "wing right open"),
    M_CMD("Right: close", "Droite : fermer", MenuWings, "wing right close"),
    M_CMD("Both: open", "Les 2 : ouvrir", MenuWings, "wings open"),
    M_CMD("Both: close", "Les 2 : fermer", MenuWings, "wings close"),

    M_CMD("Left: out", "Gauche : sortir", MenuGuns, "gun left extend"),
    M_CMD("Left: in", "Gauche : rentrer", MenuGuns, "gun left retract"),
    M_CMD("Right: out", "Droit : sortir", MenuGuns, "gun right extend"),
    M_CMD("Right: in", "Droit : rentrer", MenuGuns, "gun right retract"),

    // Angle in degrees; the wings are continuous servos: 90 = stop, the rest = speed.
    M_ADJ("Rotation Z", "Rotation Z", MenuServos, "servo rotz %d", nullptr, "@servo", 0, 180, 5),
    M_ADJ("Rotation X", "Rotation X", MenuServos, "servo rotx %d", nullptr, "@servo", 0, 180, 5),
    M_ADJ("Gun left", "Canon gauche", MenuServos, "servo gunl %d", nullptr, "@servo", 0, 180, 10),
    M_ADJ("Gun right", "Canon droit", MenuServos, "servo gunr %d", nullptr, "@servo", 0, 180, 10),
    M_ADJ("Wing L (speed)", "Aile G (vitesse)", MenuServos, "servo wingl %d", nullptr, "@servo", 45, 135, 5),
    M_ADJ("Wing R (speed)", "Aile D (vitesse)", MenuServos, "servo wingr %d", nullptr, "@servo", 45, 135, 5),
    M_CMD("Release all", "Tout relâcher", MenuServos, "servos off"),

    M_CMD("Red", "Rouge", MenuLeds, "led all FF0000"),
    M_CMD("Green", "Vert", MenuLeds, "led all 00FF00"),
    M_CMD("Blue", "Bleu", MenuLeds, "led all 0000FF"),
    M_CMD("White", "Blanc", MenuLeds, "led all FFFFFF"),
    M_CMD("Ring only", "Anneau seul", MenuLeds, "led ring FFFFFF"),
    M_CMD("Left gun only", "Canon gauche seul", MenuLeds, "led left FFFFFF"),
    M_CMD("Right gun only", "Canon droit seul", MenuLeds, "led right FFFFFF"),
    M_CMD("One-bit pattern", "Motif un bit", MenuLeds, "led pattern bit"),
    M_CMD("Normal", "Normal", MenuLeds, "led off"),

    M_CMD("Tone 1 kHz", "Tonalité 1 kHz", MenuSound, "tone 1000 500"),
    M_CMD("Gunshot", "Bruit de tir", MenuSound, "shot"),
    M_ADJ("Volume %", "Volume %", MenuSound, "set Volume %d", nullptr, "Volume", 0, 100, 10),
    M_ADJ("Gain dB", "Gain dB", MenuSound, "set AmpGain %d", nullptr, "AmpGain", 9, 15, 3),
    M_CMD("Mute on / off", "Muet oui / non", MenuSound, "mute"),

    // Calibration
    M_CMD("Hall L: open", "Hall G : ouvert", MenuCalibration, "cal hall left open"),
    M_CMD("Hall L: closed", "Hall G : fermé", MenuCalibration, "cal hall left closed"),
    M_CMD("Hall R: open", "Hall D : ouvert", MenuCalibration, "cal hall right open"),
    M_CMD("Hall R: closed", "Hall D : fermé", MenuCalibration, "cal hall right closed"),
    M_ASK("Hall: save", "Hall : enregistrer", MenuCalibration, "cal hall save"),
    M_ASK("IMU: upright now", "IMU : debout", MenuCalibration, "cal imu"),
    M_ADJ("Trim wing L", "Trim aile G", MenuCalibration, "trim left %d", "set WingTrimL %d", "WingTrimL", -200, 200, 5),
    M_ADJ("Trim wing R", "Trim aile D", MenuCalibration, "trim right %d", "set WingTrimR %d", "WingTrimR", -200, 200, 5),

    // Settings
    M_ADJ("Volume %", "Volume %", MenuSettings, "set Volume %d", nullptr, "Volume", 0, 100, 10),
    M_ADJ("LED brightness", "Luminosité LEDs", MenuSettings, "set LedBright %d", nullptr, "LedBright", 0, 255, 15),
    M_ADJ("Rest ms", "Repos ms", MenuSettings, "set CooldownMs %d", nullptr, "CooldownMs", 0, 120000, 1000),
    M_ADJ("Detect mm", "Détection mm", MenuSettings, "set DetectMaxMm %d", nullptr, "DetectMaxMm", 300, 6000, 100),
    M_ADJ("Detect angle", "Angle détection", MenuSettings, "set DetectAngle %d", nullptr, "DetectAngle", 5, 60, 5),
    M_CMD("Lab markers on", "Repères labo oui", MenuSettings, "set LabMarkers true"),
    M_CMD("Lab markers off", "Repères labo non", MenuSettings, "set LabMarkers false"),
    M_CMD("Wing animation on", "Animation ailes oui", MenuSettings, "set OledAnim true"),
    M_CMD("Wing animation off", "Animation ailes non", MenuSettings, "set OledAnim false"),
    M_CMD("Language: English", "Langue : English", MenuSettings, "set Language 0"),
    M_CMD("Language: French", "Langue : français", MenuSettings, "set Language 1"),

    // WiFi: information first (A goes from one page to the next), then actions.
    // The home network password is typed on the web page, not with two buttons.
    M_INFO("Turret network", "Réseau tourelle", MenuWifi, PageWifiAp),
    M_INFO("Home network", "Réseau maison", MenuWifi, PageWifiHome),
    M_INFO("Networks found", "Réseaux trouvés", MenuWifi, PageWifiScan),
    M_INFO("Date and time", "Date et heure", MenuWifi, PageTime),
    M_CMD("Scan networks", "Scanner", MenuWifi, "wifi scan"),
    M_CMD("Reconnect home", "Reconnecter maison", MenuWifi, "wifi join"),
    M_ASK("Forget home", "Oublier maison", MenuWifi, "wifi forget"),
    M_CMD("WiFi on", "WiFi marche", MenuWifi, "wifi on"),
    M_ASK("WiFi off", "WiFi arrêt", MenuWifi, "wifi off"),
    M_ASK("Factory AP password", "MdP tourelle usine", MenuWifi, "set ApPassword stillalive"),

    // System
    M_CMD("Resume (homing)", "Reprendre (homing)", MenuSystem, "resume"),
    M_ASK("Reboot", "Redémarrer", MenuSystem, "reboot"),
    M_ASK("Reset settings", "RAZ réglages", MenuSystem, "reset-settings"),
    M_ASK("Erase core dump", "Effacer coredump", MenuSystem, "coredump erase"),
};

static const int16_t MENU_TREE_COUNT = sizeof(MENU_TREE) / sizeof(MENU_TREE[0]);

#undef M_MENU
#undef M_CMD
#undef M_ASK
#undef M_INFO
#undef M_ADJ

} // namespace ui
