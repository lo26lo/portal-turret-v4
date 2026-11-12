#pragma once

// Texts of the web page in both languages (setting Language, the one of the
// OLED). Served by GET /api/meta as "text": {key: text}; the page puts them on
// the elements marked data-i18n="key" and uses them for the status labels.
// English is also the fallback written in the page itself, so that a page
// without /api/meta still reads. Keep the keys short and stable.
// No Arduino here: the native tests check the table.

#include <stddef.h>

namespace web {

struct Text {
  const char *key;
  const char *en;
  const char *fr;
};

static const Text TEXTS[] = {
    // Tabs
    {"tab.status", "Status", "État"},
    {"tab.settings", "Settings", "Réglages"},
    {"tab.calib", "Calibration", "Calibration"},
    {"tab.tests", "Tests", "Tests"},
    {"tab.log", "Log", "Journal"},
    {"tab.maint", "Maintenance", "Maintenance"},
    // Warnings
    {"warn.coredump", "The turret crashed: a core dump is stored. See Maintenance → Crash report.",
     "La tourelle a planté : un core dump est enregistré. Voir Maintenance → Rapport de plantage."},
    {"warn.password", "The WiFi password is still the factory one. Change ApPassword in Settings → WiFi.",
     "Le mot de passe WiFi est encore celui d'usine. Changer ApPassword dans Réglages → WiFi."},
    // Status tab
    {"wifi.title", "Home WiFi", "WiFi de la maison"},
    {"wifi.scan", "Scan", "Chercher"},
    {"wifi.choose", "– choose a network –", "– choisir un réseau –"},
    {"wifi.name", "network name", "nom du réseau"},
    {"wifi.password", "password", "mot de passe"},
    {"wifi.connect", "Connect", "Connecter"},
    {"wifi.help", "For the clock (NTP) and access from your home network. The turret's own network stays on.",
     "Pour l'heure (NTP) et l'accès depuis le réseau de la maison. Le réseau de la tourelle reste allumé."},
    {"status.title", "Status", "État"},
    {"sleep.title", "Sleep", "Veille"},
    {"sleep.radar", "Radar sleep now", "Veille radar maintenant"},
    {"sleep.deep", "Deep sleep now", "Veille profonde maintenant"},
    {"sleep.wake", "Wake up", "Réveiller"},
    {"st.uptime", "Uptime", "Allumée depuis"},
    {"st.reset", "Reset", "Reset"},
    {"st.mode", "Mode", "Mode"},
    {"st.loops", "Brownouts / loops", "Brownouts / boucles"},
    {"st.efuse", "eFuse events", "Déclenchements eFuse"},
    {"st.radar", "Radar", "Radar"},
    {"st.imu", "IMU", "IMU"},
    {"st.hall", "Hall L / R", "Hall G / D"},
    {"st.wings", "Wings", "Ailes"},
    {"st.amp", "Amp", "Ampli"},
    {"st.wifi", "WiFi", "WiFi"},
    {"st.home", "Home network", "Réseau maison"},
    {"st.time", "Time", "Heure"},
    {"st.heap", "Free heap", "Mémoire libre"},
    {"st.cycles", "Cycles", "Cycles"},
    {"st.firmware", "Firmware", "Firmware"},
    {"st.servos", "Servos (µs, – = released)", "Servos (µs, – = relâché)"},
    // Calibration tab
    {"hall.title", "Hall sensors", "Capteurs Hall"},
    {"hall.save", "Compute and save thresholds", "Calculer et enregistrer les seuils"},
    {"imu.title", "IMU", "IMU"},
    {"imu.capture", "The turret stands upright: capture", "La tourelle est debout : capturer"},
    {"imu.off", "Disable the check", "Désactiver le contrôle"},
    {"trim.title", "Wing trims", "Trims des ailes"},
    {"trim.save", "Save trims", "Enregistrer les trims"},
    // Tests tab
    {"screen.title", "Debug screen", "Écran de debug"},
    {"selftest.title", "Self-test", "Auto-test"},
    {"selftest.quick", "Quick (nothing moves)", "Rapide (rien ne bouge)"},
    {"selftest.full", "Full (LEDs, sound, servos, wings)", "Complet (LEDs, son, servos, ailes)"},
    {"selftest.report", "Report", "Rapport"},
    {"tests.title", "Tests", "Tests"},
    {"tests.help", "Tests stop the state machine (Manual). “Resume” homes and returns to Idle.",
     "Les tests arrêtent la machine à états (Manual). « Reprendre » refait le homing et revient en Idle."},
    {"tests.resume", "Resume", "Reprendre"},
    {"tests.demo", "Demo cycle", "Cycle de démo"},
    {"tests.command", "console command (help)", "commande console (help)"},
    {"tests.run", "Run", "Lancer"},
    {"tests.helpButton", "Help", "Aide"},
    {"tests.answers", "Answers appear in the Log tab.", "Les réponses s'affichent dans l'onglet Journal."},
    // Log and maintenance
    {"log.refresh", "Refresh", "Rafraîchir"},
    {"fw.title", "Firmware update", "Mise à jour du firmware"},
    {"fw.upload", "Upload", "Envoyer"},
    {"fw.help", "The turret stops before the upload and reboots afterwards.",
     "La tourelle s'arrête avant l'envoi et redémarre ensuite."},
    {"crash.title", "Crash report", "Rapport de plantage"},
    {"crash.log", "Log of the previous run", "Journal de la session précédente"},
    {"crash.erase", "Erase core dump", "Effacer le core dump"},
    {"sys.title", "System", "Système"},
    {"sys.reboot", "Reboot", "Redémarrer"},
    {"sys.factory", "Reset all settings", "Remettre tous les réglages à zéro"},
};

const size_t TEXT_COUNT = sizeof(TEXTS) / sizeof(TEXTS[0]);

} // namespace web
