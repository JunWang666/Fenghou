#include "ConfigStore.h"

bool ConfigStore::begin(SemaphoreHandle_t mutex) {
  mutex_ = mutex;
  return prefs_.begin("cfg", false);
}

void ConfigStore::load() {
  String ssid = prefs_.getString("ssid", "");
  String pass = prefs_.getString("pass", "");
  String server = prefs_.getString("server", "");
  String dev = chipDeviceIdString();
  String token = prefs_.getString("token", "");
  uint32_t interval = prefs_.getUInt("interval", DEFAULT_UPLOAD_INTERVAL_MS);

  DeviceConfig cfg;
  strlcpy(cfg.ssid, ssid.c_str(), sizeof(cfg.ssid));
  strlcpy(cfg.password, pass.c_str(), sizeof(cfg.password));
  strlcpy(cfg.server, server.c_str(), sizeof(cfg.server));
  strlcpy(cfg.deviceId, dev.c_str(), sizeof(cfg.deviceId));
  strlcpy(cfg.token, token.c_str(), sizeof(cfg.token));
  cfg.uploadIntervalMs = constrain(interval, MIN_UPLOAD_INTERVAL_MS, MAX_UPLOAD_INTERVAL_MS);
  cfg.valid = ssid.length() > 0 && server.length() > 0;
  setConfig(cfg);
}

void ConfigStore::save(const DeviceConfig &cfg) {
  prefs_.putString("ssid", cfg.ssid);
  prefs_.putString("pass", cfg.password);
  prefs_.putString("server", cfg.server);
  prefs_.putString("token", cfg.token);
  prefs_.putUInt("interval", cfg.uploadIntervalMs);
}

void ConfigStore::getCopy(DeviceConfig *out) {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  *out = config_;
  xSemaphoreGive(mutex_);
}

bool ConfigStore::applyPayload(const char *raw) {
  uint32_t hash = fnv1a(raw);
  if (hash == lastPayloadHash_) {
    return false;
  }

  DeviceConfig cfg;
  getCopy(&cfg);
  if (!parseConfigPayload(raw, &cfg)) {
    return false;
  }

  setConfig(cfg);
  save(cfg);
  lastPayloadHash_ = hash;
  return true;
}

void ConfigStore::setConfig(const DeviceConfig &cfg) {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  config_ = cfg;
  xSemaphoreGive(mutex_);
}

uint32_t ConfigStore::fnv1a(const char *s) {
  uint32_t h = 2166136261UL;
  while (*s) {
    h ^= static_cast<uint8_t>(*s++);
    h *= 16777619UL;
  }
  return h;
}

bool ConfigStore::extractValue(const char *src, const char *key, char *out, size_t outLen) {
  if (outLen == 0) {
    return false;
  }
  out[0] = 0;
  char pattern[24];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *p = strstr(src, pattern);
  if (!p) {
    return false;
  }
  p = strchr(p + strlen(pattern), ':');
  if (!p) {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p != '"') {
    return false;
  }
  p++;
  size_t i = 0;
  while (*p && *p != '"' && i + 1 < outLen) {
    out[i++] = *p++;
  }
  out[i] = 0;
  return i > 0;
}

bool ConfigStore::extractUint32(const char *src, const char *key, uint32_t *out) {
  char pattern[24];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *p = strstr(src, pattern);
  if (!p) {
    return false;
  }
  p = strchr(p + strlen(pattern), ':');
  if (!p) {
    return false;
  }
  *out = strtoul(p + 1, nullptr, 10);
  return *out > 0;
}

bool ConfigStore::parseConfigPayload(const char *raw, DeviceConfig *cfg) {
  const char *begin = strstr(raw, CONFIG_BEGIN);
  if (!begin) {
    begin = raw;
  } else {
    begin += strlen(CONFIG_BEGIN);
  }
  const char *json = strchr(begin, '{');
  if (!json) {
    return false;
  }

  char ssid[sizeof(cfg->ssid)];
  char password[sizeof(cfg->password)];
  char server[sizeof(cfg->server)];
  char token[sizeof(cfg->token)];
  if (!extractValue(json, "ssid", ssid, sizeof(ssid))) {
    return false;
  }
  if (!extractValue(json, "password", password, sizeof(password))) {
    return false;
  }
  if (!extractValue(json, "server", server, sizeof(server))) {
    return false;
  }
  if (!extractValue(json, "token", token, sizeof(token))) {
    token[0] = 0;
  }
  uint32_t interval = cfg->uploadIntervalMs;
  extractUint32(json, "upload_interval_ms", &interval);

  strlcpy(cfg->ssid, ssid, sizeof(cfg->ssid));
  strlcpy(cfg->password, password, sizeof(cfg->password));
  strlcpy(cfg->server, server, sizeof(cfg->server));
  setChipDeviceId(cfg->deviceId, sizeof(cfg->deviceId));
  strlcpy(cfg->token, token, sizeof(cfg->token));
  cfg->uploadIntervalMs = constrain(interval, MIN_UPLOAD_INTERVAL_MS, MAX_UPLOAD_INTERVAL_MS);
  cfg->valid = true;
  return true;
}
