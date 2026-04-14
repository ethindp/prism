#ifndef UNIVERSAL_SPEECH_H
#define UNIVERSAL_SPEECH_H

#include <stdbool.h>
#include <wchar.h>

#if defined(_WIN32)
#if defined(US_BUILDING)
#define US_API __declspec(dllexport)
#else
#define US_API __declspec(dllimport)
#endif
#define US_CALL __cdecl
#elif defined(__GNUC__) || defined(__clang__)
#define US_API __attribute__((visibility("default")))
#define US_CALL
#else
#define US_API
#define US_CALL
#endif

#if defined(__cplusplus)
extern "C" {
#endif

enum speechparam { SP_VOLUME, SP_VOLUME_MAX, SP_VOLUME_MIN,  SP_VOLUME_SUPPORTED, SP_RATE, SP_RATE_MAX, SP_RATE_MIN, SP_RATE_SUPPORTED, SP_PITCH, SP_PITCH_MAX, SP_PITCH_MIN, SP_PITCH_SUPPORTED, SP_INFLEXION, SP_INFLEXION_MAX, SP_INFLEXION_MIN, SP_INFLEXION_SUPPORTED, SP_PAUSED, SP_PAUSE_SUPPORTED, SP_BUSY, SP_BUSY_SUPPORTED, SP_WAIT, SP_WAIT_SUPPORTED, SP_ENABLE_NATIVE_SPEECH = 0xFFFF, SP_VOICE = 0x10000, SP_LANGUAGE = 0x20000, SP_SUBENGINE = 0x30000, SP_ENGINE = 0x40000, SP_ENGINE_AVAILABLE = 0x50000, SP_AUTO_ENGINE = 0xFFFE, SP_USER_PARAM = 0x1000000 };

US_API US_CALL int speechSay (const wchar_t* str, int interrupt);
US_API US_CALL int speechSayA (const char* str, int interrupt);
US_API US_CALL int brailleDisplay (const wchar_t* str);
US_API US_CALL int brailleDisplayA (const char* str);
US_API US_CALL int speechStop (void);
US_API US_CALL int speechGetValue (int what);
US_API US_CALL int speechSetValue (int what, int value);
US_API US_CALL const wchar_t* speechGetString (int what);
US_API US_CALL int speechSetString (int what, const wchar_t* str);
US_API US_CALL const char* speechGetStringA (int what);
US_API US_CALL int speechSetStringA (int what, const char* value);

#ifdef __cplusplus
} // extern "C"
#endif
#endif
