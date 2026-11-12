// Lab aids (README section 11, "Lab"): pin probes, v2 rails, servo and tone
// sweeps, load test. Dispatch and the command table: Actions.cpp.
#include "Actions.h"

#include "board/Expander.h"
#include "board/Log.h"
#include "board/Power.h"
#include "pins.h"
#include "states/StateMachine.h"
#include <soc/gpio_periph.h> // GPIO_PIN_MUX_REG
#include <soc/gpio_reg.h>
#include <soc/gpio_sd_reg.h>
#include <soc/gpio_sig_map.h>
#include <soc/io_mux_reg.h>
#include <soc/usb_serial_jtag_reg.h>

// ------------------------------------------------------------------ pins

String Actions::CmdPins(const String &args) {
  return args.length() ? PinReport((uint8_t)args.toInt()) : PinsReport();
}

// "pins": raw registers of the green LED pin and of the USB, to compare before
// and after an incident (23.10.2026: green LED dark and USB lost while a servo
// was driven; pressing PROG, which re-routes the LED pin, brought the LED back).
String Actions::PinsReport() const {
  const uint8_t pin = PIN_LED_GREEN; // v0.1: IO33, v2: IO13
  uint32_t outSel = REG_READ(GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * pin) & 0x1FF;
  bool outEnabled = pin < 32 ? (REG_READ(GPIO_ENABLE_REG) >> pin) & 1 : (REG_READ(GPIO_ENABLE1_REG) >> (pin - 32)) & 1;
  bool outLevel = pin < 32 ? (REG_READ(GPIO_OUT_REG) >> pin) & 1 : (REG_READ(GPIO_OUT1_REG) >> (pin - 32)) & 1;
  uint32_t usbConf = REG_READ(USB_SERIAL_JTAG_CONF0_REG);
  uint32_t frame1 = REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG) & 0x7FF;
  delay(20);
  uint32_t frame2 = REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG) & 0x7FF;
  char text[420];
  snprintf(text, sizeof(text),
           "green LED IO%u: out_sel %u (%s), output %s, level %u, firmware: %s; SD0 0x%08lx, SD clock 0x%08lx; "
           "IO_MUX 19 0x%08lx, 20 0x%08lx, %u 0x%08lx\n"
           "USB: conf0 0x%08lx (pad %s, D+ pull-up %s), frames %u in 20 ms (%s); uptime %lu ms",
           (unsigned)pin, (unsigned)outSel, outSel == GPIO_SD0_OUT_IDX ? "sigma-delta 0" : outSel == SIG_GPIO_OUT_IDX ? "GPIO" : "other",
           outEnabled ? "enabled" : "DISABLED", (unsigned)outLevel,
           turret->board.IsGreenModulated() ? "modulated" : "GPIO", (unsigned long)REG_READ(GPIO_SIGMADELTA0_REG),
           (unsigned long)REG_READ(GPIO_SIGMADELTA_CG_REG), (unsigned long)REG_READ(IO_MUX_GPIO19_REG),
           (unsigned long)REG_READ(IO_MUX_GPIO20_REG), (unsigned)pin, (unsigned long)REG_READ(GPIO_PIN_MUX_REG[pin]),
           (unsigned long)usbConf, (usbConf & USB_SERIAL_JTAG_USB_PAD_ENABLE) ? "on" : "OFF",
           (usbConf & USB_SERIAL_JTAG_DP_PULLUP) ? "on" : "off", (unsigned)((frame2 - frame1) & 0x7FF),
           frame2 != frame1 ? "host talking" : "no host frames", (unsigned long)millis());
  return String(text);
}

// "pins <n>": routing and activity of any GPIO (04.11.2026: ring all white on
// a v0.1, is IO14 really driven?). The pad is sampled while a LED frame is
// sent, its input enabled for the time of the reading.
String Actions::PinReport(uint8_t pin) const {
  if (pin > 48 || (pin >= 22 && pin <= 25) || (pin >= 27 && pin <= 32) || pin == 19 || pin == 20) {
    return "pins <n>: a GPIO of the module, not 19 / 20 (USB) nor 27..32 (flash)";
  }
  uint32_t outSel = REG_READ(GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * pin) & 0x1FF;
  bool outEnabled = pin < 32 ? (REG_READ(GPIO_ENABLE_REG) >> pin) & 1 : (REG_READ(GPIO_ENABLE1_REG) >> (pin - 32)) & 1;
  bool outLevel = pin < 32 ? (REG_READ(GPIO_OUT_REG) >> pin) & 1 : (REG_READ(GPIO_OUT1_REG) >> (pin - 32)) & 1;
  uint32_t mux = REG_READ(GPIO_PIN_MUX_REG[pin]);

  REG_SET_BIT(GPIO_PIN_MUX_REG[pin], FUN_IE);
  // The pad is sampled from core 0 while core 1 sends the frame. The LED
  // driver sets the pin as an output at each frame, which clears its input
  // enable: the sampler sets it again as soon as it drops.
  // Edge times in CPU cycles (240 MHz): the WS2812 reads a "0" from a high
  // pulse of ~0.4 us and a "1" from ~0.8 us, in a bit of ~1.25 us.
  static const uint16_t MAX_EDGES = 128;
  struct Probe {
    uint8_t pin;
    volatile bool started, done;
    uint32_t samples, highs, edges;
    uint32_t edgeAt[MAX_EDGES];
  };
  static Probe probe;
  probe.pin = pin;
  probe.started = probe.done = false;
  probe.samples = probe.highs = probe.edges = 0;
  auto sampler = [](void *arg) {
    Probe &p = *(Probe *)arg;
    const uint32_t mask = 1u << (p.pin & 31);
    const uint32_t reg = p.pin < 32 ? GPIO_IN_REG : GPIO_IN1_REG;
    const uint32_t muxReg = GPIO_PIN_MUX_REG[p.pin];
    bool last = REG_READ(reg) & mask;
    p.started = true;
    uint32_t start = ESP.getCycleCount();
    while (ESP.getCycleCount() - start < 6000u * 240u) {
      if (!(REG_READ(muxReg) & FUN_IE)) {
        REG_SET_BIT(muxReg, FUN_IE);
      }
      bool level = REG_READ(reg) & mask;
      if (level != last) {
        if (p.edges < MAX_EDGES) {
          p.edgeAt[p.edges] = ESP.getCycleCount();
        }
        p.edges++;
        last = level;
      }
      p.highs += level;
      p.samples++;
    }
    p.done = true;
    vTaskDelete(nullptr);
  };
  xTaskCreatePinnedToCore(sampler, "pinprobe", 2048, &probe, configMAX_PRIORITIES - 1, nullptr, 0);
  while (!probe.started) {
  }
  delayMicroseconds(200);
  uint32_t showStart = micros();
  turret->light.Update(0);
  uint32_t showUs = micros() - showStart;
  uint32_t outSelAfter = REG_READ(GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * pin) & 0x1FF;
  while (!probe.done) {
    delay(1);
  }
  uint32_t samples = probe.samples, highs = probe.highs, edges = probe.edges;
  if (!(mux & FUN_IE)) {
    REG_CLR_BIT(GPIO_PIN_MUX_REG[pin], FUN_IE);
  }

  const char *signal = outSel == SIG_GPIO_OUT_IDX ? "GPIO"
                       : (outSel >= RMT_SIG_OUT0_IDX && outSel <= RMT_SIG_OUT0_IDX + 3) ? "RMT"
                       : outSel == GPIO_SD0_OUT_IDX ? "sigma-delta 0"
                                                    : "other";
  // Pulses from the recorded edges: the line rests low and the sampler starts
  // 200 us before the frame, so even edges rise.
  uint16_t recorded = probe.edges < MAX_EDGES ? probe.edges : MAX_EDGES;
  uint32_t highMin = UINT32_MAX, highMax = 0, periodMin = UINT32_MAX, periodMax = 0, shortCount = 0, longCount = 0;
  String bits;
  for (uint16_t i = 0; i + 1 < recorded; i += 2) {
    uint32_t highNs = (probe.edgeAt[i + 1] - probe.edgeAt[i]) * 1000 / 240;
    highMin = min(highMin, highNs);
    highMax = max(highMax, highNs);
    (highNs < 600 ? shortCount : longCount)++;
    if (bits.length() < 48) {
      bits += highNs < 600 ? '0' : '1';
    }
    if (i + 2 < recorded) {
      uint32_t periodNs = (probe.edgeAt[i + 2] - probe.edgeAt[i]) * 1000 / 240;
      periodMin = min(periodMin, periodNs);
      periodMax = max(periodMax, periodNs);
    }
  }

  char text[640];
  snprintf(text, sizeof(text),
           "IO%u: out_sel %u (%s), output %s, out level %u; IO_MUX 0x%08lx (func %lu, pull-up %s, pull-down %s, "
           "drive %lu); LED update %lu us, out_sel after it %lu\n"
           "pad over 6 ms (core 0, during the update): %lu samples, %lu high, %lu edges (%s)\n"
           "first %u edges: high %lu..%lu ns (%lu short < 600 ns, %lu long), bit period %lu..%lu ns; bits %s",
           (unsigned)pin, (unsigned)outSel, signal, outEnabled ? "enabled" : "DISABLED", (unsigned)outLevel,
           (unsigned long)mux, (unsigned long)((mux >> MCU_SEL_S) & MCU_SEL_V), (mux & FUN_PU) ? "on" : "off",
           (mux & FUN_PD) ? "on" : "off", (unsigned long)((mux >> FUN_DRV_S) & FUN_DRV_V), (unsigned long)showUs,
           (unsigned long)outSelAfter, (unsigned long)samples, (unsigned long)highs, (unsigned long)edges,
           edges ? "toggling" : "steady", (unsigned)recorded, (unsigned long)(highMax ? highMin : 0),
           (unsigned long)highMax, (unsigned long)shortCount, (unsigned long)longCount,
           (unsigned long)(periodMax ? periodMin : 0), (unsigned long)periodMax, bits.c_str());
  return String(text);
}

// ------------------------------------------------------------------ rails (v2)

String Actions::CmdRails(const String &) { return RailsReport(); }

// "rails": port 0 of the v2 rail expander, as read back from the chip.
String Actions::RailsReport() const {
#if BOARD_HAS_EXPANDER
  uint8_t output = 0, config = 0, input = 0;
  if (!Expander::ReadPort0(output, config, input)) {
    return "rails: expander at 0x20 does not answer";
  }
  auto rail = [&](const char *name, uint8_t port, bool activeLow) {
    bool isOutput = (config & (1u << port)) == 0;
    bool high = output & (1u << port);
    bool on = isOutput && (activeLow ? !high : high);
    return String(name) + (on ? " ON" : " off") + (isOutput ? "" : " (input)");
  };
  char raw[64];
  snprintf(raw, sizeof(raw), "; port 0: output 0x%02X, config 0x%02X, input 0x%02X", output, config, input);
  return rail("servos", EXPANDER_SERVO_EN, false) + ", " + rail("LEDs", EXPANDER_LED_EN, false) + ", " +
         rail("radar", EXPANDER_RADAR_EN, false) + ", " + rail("Hall", EXPANDER_HALL_EN_N, true) + raw;
#else
  return "rails: none on " BOARD_NAME " (5 V always on)";
#endif
}

// "rail <servo|led|radar|hall> on|off" (v2, bring-up): what drives the rail
// is stopped before it goes off, and started again once it is on.
String Actions::CmdRail(const String &args) {
  if (!Power::IsSwitchable()) {
    return "rail: no switched rails on " BOARD_NAME;
  }
  String state;
  String name = FirstWord(args, state);
  state.trim();
  state.toLowerCase();
  Rail rail;
  if (!Power::Parse(name, rail) || (state != "on" && state != "off")) {
    return "usage: rail servo|led|radar|hall on|off";
  }
  bool on = state == "on";
  bool ok = true;
  switch (rail) {
  case Rail::Servo:
    if (on) {
      ok = Power::On(rail); // the waiting commands attach from the next loop
    } else {
      turret->gantry.DetachAll();
      ok = Power::Off(rail);
    }
    break;
  case Rail::Led:
    if (on) {
      ok = Power::On(rail);
      turret->light.SetEnabled(stateMachine->GetCurrentStateId() != StateId::Fault);
    } else {
      turret->light.SetEnabled(false);
      ok = Power::Off(rail);
    }
    break;
  case Rail::Radar:
    if (on) {
      ok = Power::On(rail);
      turret->radar.Initialize();
    } else {
      turret->radar.End();
      ok = Power::Off(rail);
    }
    break;
  case Rail::Hall:
    // Wings moving on the Hall threshold: they would run to their timeout.
    if (!on && (turret->gantry.GetWingLeft().IsMoving() || turret->gantry.GetWingRight().IsMoving())) {
      return "refused: a wing is moving";
    }
    ok = on ? Power::On(rail) : Power::Off(rail);
    break;
  }
  if (!ok) {
    return String("rail ") + Power::Name(rail) + ": the expander did not take it";
  }
  return RailsReport();
}

// ------------------------------------------------------------------ sweeps (L2)

String Actions::CmdSweep(const String &args) {
  if (args == "stop") {
    sweepChannel = -1;
    turret->audio.StopTone();
    return "sweep stopped";
  }
  String error;
  if (!EnterTestMode(error)) {
    return error;
  }
  String rest;
  String what = FirstWord(args, rest);
  if (what == "servo") {
    String msText;
    int index = Gantry::ChannelIndex(FirstWord(rest, msText));
    ulong ms = msText.toInt();
    if (index < 0 || ms < 500 || ms > 60000) {
      return "usage: sweep servo <0-5|rotz|rotx|gunl|gunr|wingl|wingr> <500..60000 ms>";
    }
    sweepChannel = index;
    sweepStart = millis();
    sweepDuration = ms;
    sweepLastWrite = 0;
    return String("sweeping ") + turret->gantry.GetChannel(index).GetName() + " 0 -> 180 -> 0 deg in " +
           String(ms) + " ms";
  }
  if (what == "tone") {
    String a, b;
    uint32_t start = FirstWord(rest, a).toInt();
    uint32_t end = FirstWord(a, b).toInt();
    uint32_t ms = b.toInt();
    if (start < 20 || end < 20 || start > 20000 || end > 20000 || ms == 0 || ms > 30000) {
      return "usage: sweep tone <20..20000 Hz> <20..20000 Hz> <ms <= 30000>";
    }
    turret->audio.PlayChirp(start, end, ms);
    return "tone sweep " + String(start) + " -> " + String(end) + " Hz in " + String(ms) + " ms";
  }
  return "usage: sweep servo <n> <ms> | sweep tone <Hz> <Hz> <ms> | sweep stop";
}

void Actions::UpdateSweep(ulong now) {
  if (sweepChannel < 0) {
    return;
  }
  // One command per servo period is enough.
  if (sweepLastWrite != 0 && now - sweepLastWrite < 20) {
    return;
  }
  sweepLastWrite = now;
  ulong elapsed = now - sweepStart;
  ServoChannel &channel = turret->gantry.GetChannel(sweepChannel);
  if (elapsed >= sweepDuration) {
    channel.SetAngle(0);
    sweepChannel = -1;
    Log.println("sweep done");
    return;
  }
  // Triangle: 0 -> 180 over the first half, back to 0 over the second.
  float phase = (float)elapsed / sweepDuration;
  int angle = (int)(phase < 0.5f ? phase * 2 * 180 : (1 - phase) * 2 * 180);
  channel.SetAngle(angle);
}

// ------------------------------------------------------------------ load test (L4)

String Actions::CmdLoadTest(const String &args) {
  long gap = args.toInt();
  if (gap < 0 || gap > 2000 || args.length() == 0) {
    return "usage: loadtest <gap 0..2000 ms> (0 = all together)";
  }
  if (loadPhase != LoadPhase::Off) {
    return "load test already running";
  }
  Gantry &gantry = turret->gantry;
  Settings &settings = turret->settings;
  // 0 would mean "back to the setting": 1 ms is "all together".
  loadGapMs = gap == 0 ? 1 : gap;
  gantry.SetStaggerOverride(loadGapMs);
  // Every servo gets a command; the attach schedule starts them loadGapMs apart.
  gantry.GetChannel(0).SetAngle(110); // rotate Z
  gantry.GetChannel(1).SetAngle(110); // rotate X
  gantry.GetWingLeft().GetGun().Extend();
  gantry.GetWingRight().GetGun().Extend();
  // Wings at their stop point (attached, not turning) for the whole test.
  gantry.GetWingLeft().TestStop(settings.GetInt(SettingId::WingTrimL), 20000);
  gantry.GetWingRight().TestStop(settings.GetInt(SettingId::WingTrimR), 20000);
  loadPhase = LoadPhase::Attaching;
  loadPhaseAt = millis();
  return "load test: 6 servos, " + String(gap) + " ms apart (see the log)";
}

void Actions::UpdateLoadTest(ulong now) {
  Gantry &gantry = turret->gantry;
  switch (loadPhase) {
  case LoadPhase::Off:
    return;
  case LoadPhase::Attaching:
    if (!gantry.HasPendingAttach()) {
      Log.printf("loadtest: all servos attached after %lu ms\n", now - loadPhaseAt);
      loadPhase = LoadPhase::Holding;
      loadPhaseAt = now;
    }
    return;
  case LoadPhase::Holding:
    if (now - loadPhaseAt >= 1000) {
      gantry.GetChannel(0).SetAngle(90);
      gantry.GetChannel(1).SetAngle(90);
      gantry.GetWingLeft().GetGun().Retract();
      gantry.GetWingRight().GetGun().Retract();
      loadPhase = LoadPhase::Returning;
      loadPhaseAt = now;
    }
    return;
  case LoadPhase::Returning:
    if (now - loadPhaseAt >= 1000) {
      gantry.DetachAll();
      gantry.SetStaggerOverride(0);
      loadPhase = LoadPhase::Off;
      Log.println("loadtest: done, all servos released");
    }
    return;
  }
}

void Actions::CancelLabTests() {
  if (sweepChannel >= 0 || loadPhase != LoadPhase::Off) {
    Log.println("lab test cancelled");
  }
  sweepChannel = -1;
  if (loadPhase != LoadPhase::Off) {
    turret->gantry.SetStaggerOverride(0);
    loadPhase = LoadPhase::Off;
  }
}
