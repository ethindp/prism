// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "../simdutf.h"
#ifdef _WIN32
#include "raw/fsapi.h"
#include <windows.h>

class JawsBackend final: public TextToSpeechBackend {
private:
IJawsApi* controller;
public:
~JawsBackend() override {
if (controller) {
controller->Release();
controller = nullptr;
}
CoUninitialize();
}

std::string_view get_name() const override {
return "JAWS";
}

BackendResult<> initialize() override {
if (controller) return std::unexpected(BackendError::AlreadyInitialized);
switch (CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {
case S_OK:
case S_FALSE:
case RPC_E_CHANGED_MODE: {
switch (CoCreateInstance(CLSID_JawsApi, nullptr, CLSCTX_INPROC_SERVER, IID_IJawsApi, (void **)&controller)) {
case S_OK: {
if (!(!!FindWindow("JFWUI2", nullptr))) return std::unexpected(BackendError::NotInitialized);
return {};
} break;
case REGDB_E_CLASSNOTREG:
case E_NOINTERFACE:
return std::unexpected(BackendError::BackendNotAvailable);
default:
return std::unexpected(BackendError::Unknown);
}
}
break;
case E_INVALIDARG:
return std::unexpected(BackendError::InvalidParam);
case E_OUTOFMEMORY:
return std::unexpected(BackendError::MemoryFailure);
case E_UNEXPECTED:
return std::unexpected(BackendError::Unknown);
}
return {};
}

BackendResult<> speak(std::string_view text, bool interrupt) override {
// This is terrible. Find another way.
if (!controller) return std::unexpected(BackendError::NotInitialized);
if (!(!!FindWindow("JFWUI2", nullptr))) return std::unexpected(BackendError::NotInitialized);
const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
std::wstring wstr;
wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(text.data(), text.size(), reinterpret_cast<char16_t *>(wstr.data())); res == 0) return std::unexpected(BackendError::InvalidUtf8);
const BSTR bstr = SysAllocString(wstr.c_str());
if (!bstr) return std::unexpected(BackendError::MemoryFailure);
VARIANT_BOOL result = VARIANT_FALSE;
const VARIANT_BOOL flush = interrupt ? VARIANT_TRUE : VARIANT_FALSE;
const bool succeeded = SUCCEEDED(controller->SayString(bstr, flush, &result));
SysFreeString(bstr);
if (succeeded && result == VARIANT_TRUE) return {};
return std::unexpected(BackendError::InternalBackendError);
}

BackendResult<> braille(std::string_view text) override {
// This is terrible. Find another way.
if (!controller) return std::unexpected(BackendError::NotInitialized);
if (!(!!FindWindow("JFWUI2", nullptr))) return std::unexpected(BackendError::NotInitialized);
const auto len = simdutf::utf16_length_from_utf8(text.data(), text.size());
std::wstring wstr;
wstr.resize(len);
    if (const auto res = simdutf::convert_utf8_to_utf16le(text.data(), text.size(), reinterpret_cast<char16_t *>(wstr.data())); res == 0) return std::unexpected(BackendError::InvalidUtf8);
    auto i = wstr.find_first_of(L"\"");
    while (i != std::wstring::npos) {
    wstr[i] = L'\'';
    i = wstr.find_first_of(L"\"", i + 1);
    }
    wstr.insert(0, L"BrailleString(\"");
    wstr.append(L"\")");
    const BSTR bstr = SysAllocString(wstr.c_str());
if (!bstr) return std::unexpected(BackendError::MemoryFailure);
VARIANT_BOOL result = VARIANT_FALSE;
const bool succeeded = SUCCEEDED(controller->RunFunction(bstr, &result));
SysFreeString(bstr);
if (succeeded && result == VARIANT_TRUE) return {};
return std::unexpected(BackendError::InternalBackendError);
}

BackendResult<> output(std::string_view text, bool interrupt) override {
    if (const auto res = speak(text, interrupt); !res)
      return res;
    if (const auto res = braille(text); !res)
      return res;
    return {};
}

BackendResult<> stop() override {
if (!controller) return std::unexpected(BackendError::NotInitialized);
if (!(!!FindWindow("JFWUI2", nullptr))) return std::unexpected(BackendError::NotInitialized);
if (SUCCEEDED(controller->StopSpeech())) return {};
return std::unexpected(BackendError::InternalBackendError);
}
};

REGISTER_BACKEND_WITH_ID(JawsBackend, Backends::JAWS, "JAWS", 1);
#endif