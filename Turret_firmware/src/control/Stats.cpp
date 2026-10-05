#include "Stats.h"

namespace {
const char *NVS_NAMESPACE = "stats";
const char *KEY_BOOTS = "boots";
const char *KEY_CYCLES = "cycles";
} // namespace

void Stats::Begin() {
  ready = prefs.begin(NVS_NAMESPACE, false);
  if (!ready) {
    return;
  }
  boots = prefs.getUInt(KEY_BOOTS, 0) + 1;
  totalCycles = prefs.getUInt(KEY_CYCLES, 0);
  prefs.putUInt(KEY_BOOTS, boots);
}

void Stats::CountCycle() {
  sessionCycles++;
  totalCycles++;
  lastCycleAt = millis();
  // One write per cycle: a cycle lasts several seconds, well within what
  // the wear levelling of NVS can take.
  if (ready) {
    prefs.putUInt(KEY_CYCLES, totalCycles);
  }
}

void Stats::CountTarget() { sessionTargets++; }
