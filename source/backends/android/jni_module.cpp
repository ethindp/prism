// SPDX-License-Identifier: MPL-2.0

// Builds Djinni's generated TtsBackendModule.cpp, which is left unchanged so it
// can be regenerated, and keeps it and Djinni's JNI_OnLoad in a static link.
//
// TtsBackendModule.cpp registers its natives through a file-scope static, and
// the Java VM finds JNI_OnLoad by name at run time, so nothing a linker can see
// refers to either. source/prism.cpp references the anchor below (it is listed
// with the backend anchors in cmake/PrismBackends.cmake), which pulls in this
// object; this object references JNI_OnLoad, which pulls in Djinni's.

#include "annotations.h"
// Building the generated file inside this one is the point (see above).
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "jni/TtsBackendModule.cpp"
#include <jni.h>

PRISM_KEEP static const auto prism_jni_on_load = &JNI_OnLoad;

extern "C" void prism_anchor_jni_module() {}
