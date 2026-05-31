#include "DeviceIdentity.h"

void setChipDeviceId(char *out, size_t outLen) {
  if (outLen == 0) {
    return;
  }

  uint64_t mac = ESP.getEfuseMac();
  snprintf(out,
           outLen,
           "%02x:%02x:%02x:%02x:%02x:%02x",
           static_cast<uint8_t>(mac >> 40),
           static_cast<uint8_t>(mac >> 32),
           static_cast<uint8_t>(mac >> 24),
           static_cast<uint8_t>(mac >> 16),
           static_cast<uint8_t>(mac >> 8),
           static_cast<uint8_t>(mac));
}

String chipDeviceIdString() {
  char id[18] = "";
  setChipDeviceId(id, sizeof(id));
  return String(id);
}
