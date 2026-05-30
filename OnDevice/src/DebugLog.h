#pragma once

#ifndef ENABLE_DEBUG_LOG
#define ENABLE_DEBUG_LOG 0
#endif

#if ENABLE_DEBUG_LOG
#define DEBUG_LOG(...) Serial.printf(__VA_ARGS__)
#define DEBUG_LOGLN(msg) Serial.println(msg)
#else
#define DEBUG_LOG(...)
#define DEBUG_LOGLN(msg)
#endif
