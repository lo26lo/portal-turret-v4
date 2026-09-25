#include "board/Log.h"
#include "Radar.h"
#include "pins.h"

void Radar::Initialize() {
  Serial1.begin(256000, SERIAL_8N1, PIN_RADAR_RX, PIN_RADAR_TX);
}

void Radar::Update(ulong deltaTime) {
  UpdateSerialData();
}

const RadarTarget &Radar::GetTarget(uint8_t index) const {
  return radarTargets[index % TRACK_COUNT];
}

void Radar::UpdateSerialData() {
  while (Serial1.available()) {
    int byte = Serial1.read();

    if (!readingAck) {
      if (byte == ackHeader[ackHeaderIndex]) {
        ackHeaderIndex++;
        if (ackHeaderIndex == 4) {
          ackHeaderIndex = 0;
          readingAck = true;
          writeIndex = 0;
        }
      } else {
        ackHeaderIndex = 0;
      }
    } else {
      messageBuffer[writeIndex++] = byte;
      if (byte == ackFooter[ackFooterIndex]) {
        ackFooterIndex++;
        if (ackFooterIndex == 4) {
          readingAck = false;
          ackFooterIndex = 0;
          ackHeaderIndex = 0;
          writeIndex = 0;
        }
      }
    }

    if (!readingData) {
      if (byte == messageHeader[headerIndex]) {
        headerIndex++;
        if (headerIndex == 4) {
          headerIndex = 0;
          readingData = true;
          writeIndex = 0;
        }
      } else {
        headerIndex = 0;
      }
    } else {
      messageBuffer[writeIndex++] = byte;
      if (byte == messageFooter[footerIndex]) {
        footerIndex++;
        if (footerIndex == 2) {
          previousRadarTargetCount = radarTargetCount;
          frameSeen = true;
          lastSensorUpdateTime = millis();
          radarTargetCount = 0;
          readingData = false;
          writeIndex -= 2;

          for (uint8_t i = 0; i < TRACK_COUNT; i++) {
            uint8_t index = i * 8;

            uint8_t combinedBytes = 0;

            for (uint8_t b = 0; b < 8; b++) {
              combinedBytes = combinedBytes | messageBuffer[index + b];
            }

            RadarTarget radarTarget = radarTargets[i];
            if (combinedBytes != 0x00) {
              // Valid target;

              int16_t x = (int16_t)(messageBuffer[index] |
                                    (messageBuffer[index + 1] << 8));
              int16_t y = (int16_t)(messageBuffer[index + 2] |
                                    (messageBuffer[index + 3] << 8));
              int16_t speed = (int16_t)(messageBuffer[index + 4] |
                                        (messageBuffer[index + 5] << 8));
              uint16_t resolution = (uint16_t)(messageBuffer[index + 6] |
                                               (messageBuffer[index + 7] << 8));

              if (messageBuffer[index + 1] & 0x80)
                x -= 0x8000;
              else
                x = -x;
              if (messageBuffer[index + 3] & 0x80)
                y -= 0x8000;
              else
                y = -y;
              if (messageBuffer[index + 5] & 0x80)
                speed -= 0x8000;
              else
                speed = -speed;

              radarTarget.previousX = radarTarget.x;
              radarTarget.previousY = radarTarget.y;
              radarTarget.x = x;
              radarTarget.y = y;
              radarTarget.speed = speed;
              radarTarget.resolution = resolution;
              radarTarget.available = true;

              radarTargetCount++;
            } else {
              radarTarget.available = false;
            }

            radarTargets[i] = radarTarget;

            lastSensorUpdateTime = millis();
          }

          if (previousRadarTargetCount != radarTargetCount) {
            Log.print("Radar Target Count Changed: ");
            Log.println(radarTargetCount);
          }

          footerIndex = 0;
          headerIndex = 0;
          writeIndex = 0;
        }
      } else {
        footerIndex = 0;
      }
    }
  }
}

bool Radar::IsAlive(ulong maxAgeMs) const {
  return frameSeen && millis() - lastSensorUpdateTime < maxAgeMs;
}

uint8_t Radar::GetTargetCount() {
  return radarTargetCount;
}