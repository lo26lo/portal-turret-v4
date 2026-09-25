#include "board/Log.h"
#include "Ota.h"

#include "control/Actions.h"
#include <Update.h>

namespace {
const ulong RebootDelay = 500;
const uint32_t ShutdownTimeoutMs = 1000;
}

void Ota::Initialize(AsyncWebServer &server, Actions &actionsIn, const char *usernameIn, const char *passwordIn) {
  actions = &actionsIn;
  username = usernameIn;
  password = passwordIn;

  server.on(
      "/update", HTTP_POST,
      // Runs after the whole body, and after the authentication middleware.
      [this](AsyncWebServerRequest *request) {
        if (!uploadAccepted) {
          request->send(400, "text/plain", "No firmware received");
          return;
        }
        bool failed = ::Update.hasError() || !::Update.isFinished();
        AsyncWebServerResponse *response = request->beginResponse(
            failed ? 500 : 200, "text/plain",
            failed ? String("Update failed, rebooting: ") + ::Update.errorString() : String("Update OK, rebooting"));
        response->addHeader("Connection", "close");
        request->send(response);

        // Everything was shut down before the upload: reboot in both cases.
        uploadAccepted = false;
        shouldReboot = true;
        rebootTimer = RebootDelay;
      },
      // Runs for each chunk as the body arrives, BEFORE the authentication
      // middleware (ESPAsyncWebServer 3.x): credentials are checked here too.
      [this](AsyncWebServerRequest *request, String filename, size_t index,
             uint8_t *data, size_t len, bool final) {
        if (index == 0) {
          uploadAccepted = request->authenticate(username.c_str(), password.c_str());
          if (!uploadAccepted) {
            Log.println("OTA: rejected, bad credentials");
            return;
          }
          Log.printf("OTA start: %s\n", filename.c_str());
          // Plan §10.5: stop the turret before accepting any data.
          if (!actions->RequestOtaShutdown(ShutdownTimeoutMs)) {
            Log.println("OTA: shutdown not confirmed in time, continuing");
          }
          if (!::Update.begin(UPDATE_SIZE_UNKNOWN)) {
            ::Update.printError(Log);
            return;
          }
        }

        if (!uploadAccepted || ::Update.hasError()) {
          return;
        }

        if (::Update.write(data, len) != len) {
          ::Update.printError(Log);
          return;
        }

        if (final) {
          if (::Update.end(true)) {
            Log.printf("OTA done: %u bytes\n", index + len);
          } else {
            ::Update.printError(Log);
          }
        }
      });
}

void Ota::Update(ulong deltaTime) {
  if (!shouldReboot) {
    return;
  }

  if (rebootTimer > deltaTime) {
    rebootTimer -= deltaTime;
    return;
  }

  Log.println("Rebooting into new firmware");
  actions->ShutdownForRestart(); // already done at upload start; harmless twice
  Serial.flush();
  ESP.restart();
}
