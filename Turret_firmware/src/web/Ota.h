#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class Actions;

// Over-the-air firmware upload over the existing web server.
// POST the firmware.bin to /update as multipart form data, with the web
// credentials (user "turret", password ApPassword), e.g.
//   curl -u turret:stillalive -F "firmware=@.pio/build/turret2/firmware.bin" http://192.168.4.1/update
// The turret is shut down (plan §3.3) before the data is accepted, and always
// reboots afterwards, whether the update succeeded or not.
class Ota {
public:
  void Initialize(AsyncWebServer &server, Actions &actions, const char *username, const char *password);
  void Update(ulong deltaTime);

private:
  Actions *actions = nullptr;
  String username;
  String password;
  bool uploadAccepted = false;
  ulong rebootTimer = 0;
  bool shouldReboot = false;
};
