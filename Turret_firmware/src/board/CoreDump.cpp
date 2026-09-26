#include "CoreDump.h"

#include "board/Log.h"
#include <esp_core_dump.h>
#include <esp_partition.h>

namespace {
size_t imageSize = 0;
size_t imageAddress = 0; // absolute flash address given by esp_core_dump_image_get
String summary;

const esp_partition_t *Partition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
}
} // namespace

namespace CoreDump {

void Begin() {
  imageSize = 0;
  summary = "";
  size_t address = 0;
  size_t size = 0;
  if (esp_core_dump_image_check() != ESP_OK || esp_core_dump_image_get(&address, &size) != ESP_OK) {
    return;
  }
  imageSize = size;
  imageAddress = address;

  esp_core_dump_summary_t *info = (esp_core_dump_summary_t *)malloc(sizeof(esp_core_dump_summary_t));
  if (info != nullptr && esp_core_dump_get_summary(info) == ESP_OK) {
    char line[48];
    summary = "task ";
    summary += info->exc_task;
    snprintf(line, sizeof(line), ", PC 0x%08lx, backtrace", (unsigned long)info->exc_pc);
    summary += line;
    for (uint32_t i = 0; i < info->exc_bt_info.depth && i < 16; i++) {
      snprintf(line, sizeof(line), " 0x%08lx", (unsigned long)info->exc_bt_info.bt[i]);
      summary += line;
    }
    if (info->exc_bt_info.corrupted) {
      summary += " (corrupted)";
    }
  } else {
    summary = "no summary available";
  }
  free(info);
  Log.printf("Core dump found (%u bytes): %s\n", (unsigned)imageSize, summary.c_str());
}

size_t Size() { return imageSize; }

const String &Summary() { return summary; }

size_t Read(size_t offset, uint8_t *buffer, size_t length) {
  const esp_partition_t *partition = Partition();
  if (partition == nullptr || offset >= imageSize || imageAddress < partition->address) {
    return 0;
  }
  length = min(length, imageSize - offset);
  size_t start = imageAddress - partition->address;
  if (esp_partition_read(partition, start + offset, buffer, length) != ESP_OK) {
    return 0;
  }
  return length;
}

bool Erase() {
  bool ok = esp_core_dump_image_erase() == ESP_OK;
  if (ok) {
    imageSize = 0;
    summary = "";
  }
  return ok;
}

} // namespace CoreDump
