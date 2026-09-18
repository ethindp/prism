/**
 ******************************************************************************
 * SpeechCore.
 * @file SpeechCore.h
 * @brief Cross-platform C++ library for screen reader output.
 * @Copyright (C) 2024 Oussama Ben Gatrane
 * @license
 * This product is licensed under the MIT License. See the license file for the
 * full license text.
 *
 * @see https://github.com/still-standing88/SpeechCore
 * Version: 1.1.0
 *  Author: [Oussama AKA Still standing]
 * Description: Cross-platform C++ library for screen reader output.
 *
 ******************************************************************************
 **/

#ifndef SPEECH_CORE_H
#define SPEECH_CORE_H
#pragma once
#define _SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING
#ifdef _WIN32
#if defined(SPEECH_CORE_BUILDING) || defined(__SPEECH_C_EXPORT)
#define SPEECH_C_API __declspec(dllexport)
#elif defined(__SPEECH_C_STATIC)
#define SPEECH_C_API
#else
#define SPEECH_C_API __declspec(dllimport)
#endif // _WIN32
#else
#if defined(SPEECH_CORE_BUILDING) || defined(__SPEECH_C_EXPORT)
#define SPEECH_C_API                                                           \
  __attribute__((visibility("default"))) // For compilors such as Clang
#else
#define SPEECH_C_API
#endif // SPEECH_C_EXPORT
#endif
#ifdef __cplusplus
#define SPEECH_C_DEFAULT(value) = value
#else
#define SPEECH_C_DEFAULT(value)
#endif
/*
 * @breif Defined macroes for speech flags
 */
#define SC_SPEECH_FLOW_CONTROL (1 << 0)
#define SC_SPEECH_PARAMETER_CONTROL (1 << 1)
#define SC_VOICE_CONFIG (1 << 2)
#define SC_FILE_OUTPUT (1 << 3)
#define SC_HAS_SPEECH (1 << 4)
#define SC_HAS_BRAILLE (1 << 5)
#define SC_HAS_SPEECH_STATE (1 << 6)
#define SC_SSML_SUPPORT (1 << 7)

#ifdef __cplusplus
#include <cstdint>
#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

/**
 * @brief Initializes the SpeechCore library. Must be called before using other
 * functions.
 */
SPEECH_C_API void Speech_Init();

/**
 * @brief Finalizes SpeechCore and frees up resources.
 */
SPEECH_C_API void Speech_Free();

/**
 * @brief Detects and selects an available screen reader.
 *
 * Loops through available screen readers detected on the platform and selects
 * the first one that's running. If prefer_sapi is set to true, it sets SAPI as
 * the current screen reader. This function is called automatically by
 * Speech_Init and Speech_Output.
 */
SPEECH_C_API void Speech_Detect_Driver();

/**
 * @brief Retrieves the name of the currently detected/used screen reader.
 * @return A const wchar_t string representing the current driver name.
 */
SPEECH_C_API const wchar_t *Speech_Current_Driver();

/**
 * @brief Retrieves the name of a screen reader driver by index.
 * @param index The index of the driver to retrieve.
 * @return A const wchar_t string representing the driver name.
 */
SPEECH_C_API const wchar_t *Speech_Get_Driver(int index);

/**
 * @brief Sets the current screen reader driver by index.
 * @param index The index of the driver to set as current.
 */
SPEECH_C_API void Speech_Set_Driver(int index);

/**
 * @brief Retrieves the number of available screen reader drivers.
 * @return An integer representing the number of drivers.
 */
SPEECH_C_API int Speech_Get_Drivers();

/**
 * @brief Retrieves the flags associated with the currently detected/used screen
 * reader.
 *
 * The speech flags represent the features the screen reader supports.
 * @return An uint32_t representing the speech flags.
 */
SPEECH_C_API uint32_t Speech_Get_Flags();

/**
 * @brief Checks if the library is loaded.
 * @return A bool representing if the library is loaded.
 */
SPEECH_C_API bool Speech_Is_Loaded();

/**
 * @brief Checks if the current screen reader is actively speaking.
 * @return A bool representing the state of the screen reader's speaking state.
 */
SPEECH_C_API bool Speech_Is_Speaking();

/**
 * @brief Outputs a given string to be spoken by the currently used/detected
 * screen reader.
 * @param text A const wchar_t string representing the text to be spoken.
 * @param _interrupt Whether to interrupt the current speech segment. Default is
 * false.
 * @return A bool indicating if the operation was successful.
 */
SPEECH_C_API bool Speech_Output(const wchar_t *text,
                                bool _interrupt SPEECH_C_DEFAULT(false));

/**
 * @brief Outputs a given string to be spoken by the currently used/detected
 * screen reader.
 * @param text A const wchar_t string representing the text to be spoken.
 * @param interrupt Whether to interrupt the current speech segment. Default is
 * false.
 * @param with_ssml Whether to enable ssml support if the current driver
 * supports it. Disabled by default.
 * @return A bool indicating if the operation was successful.
 */
SPEECH_C_API bool Speech_Output_Text(const wchar_t *text,
                                     bool interrupt SPEECH_C_DEFAULT(false),
                                     bool with_ssml SPEECH_C_DEFAULT(false));

/**
 * @brief Outputs a given string to the braille display if supported.
 * @param text A const wchar_t string representing the text to be displayed in
 * braille.
 * @return A bool indicating if the operation was successful.
 */
SPEECH_C_API bool Speech_Braille(const wchar_t *text);

/**
 * @brief Stops speaking if the screen reader is currently speaking.
 * @return A bool indicating if the operation was successful.
 */
SPEECH_C_API bool Speech_Stop();

/**
 * @brief Gets the volume of the current screen reader if supported.
 * @return A float representing the volume.
 */
SPEECH_C_API float Speech_Get_Volume();

/**
 * @brief Sets the volume for the current screen reader if supported.
 * @param offset A float representing the volume to be set.
 */
SPEECH_C_API void Speech_Set_Volume(float offset);

/**
 * @brief Gets the rate/speed parameter for the current screen reader if
 * supported.
 * @return A float representing the rate value.
 */
SPEECH_C_API float Speech_Get_Rate();

/**
 * @brief Sets the rate/speed for the current screen reader if supported.
 * @param offset A float representing the rate to be set.
 */
SPEECH_C_API void Speech_Set_Rate(float offset);

/**
 * @brief Gets the pitch speed parameter for the current screen reader if
 * supported.
 * @return A float representing the pitch  value.
 */
SPEECH_C_API float Speech_Get_Pitch();

/**
 * @brief Sets the pitch/speed for the current screen reader if supported.
 * @param offset A float representing the pitch to be set.
 */
SPEECH_C_API void Speech_Set_Pitch(float offset);

/**
 * @brief Retrieves the current voice of the active screen reader if supported.
 * @return A const wchar_t string representing the voice name.
 */
SPEECH_C_API const wchar_t *Speech_Get_Current_Voice();

/**
 * @brief Retrieves the voice name of the given voice by its index for the
 * currently active screen reader if supported.
 * @param index The index of the voice.
 * @return A const wchar_t string representing the name of the voice.
 */
SPEECH_C_API const wchar_t *Speech_Get_Voice(int index);

/**
 * @brief Sets the voice by its index for the currently active screen reader if
 * supported.
 * @param index The index of the voice to set.
 */
SPEECH_C_API void Speech_Set_Voice(int index);

/**
 * @brief Retrieves the number of available voices in the currently active
 * screen reader if supported.
 * @return An integer representing the number of voices.
 */
SPEECH_C_API int Speech_Get_Voices();

/**
 * @brief Outputs the given text into a file for the currently active screen
 * reader if supported.
 * @param filePath A const char* representing the name of the file.
 * @param text A const wchar_t string representing the text to be outputted.
 */
SPEECH_C_API void Speech_Output_File(const char *filePath, const wchar_t *text);

/**
 * @brief Resumes speech for the currently active screen reader if supported.
 */
SPEECH_C_API void Speech_Resume();

/**
 * @brief Pauses speech for the currently active screen reader if supported.
 */
SPEECH_C_API void Speech_Pause();

#ifdef _WIN32
/**
 * @brief Sets the preference for using SAPI as the primary speech engine.
 * @param prefer_sapi A boolean indicating whether to prefer SAPI over other
 * screen readers.
 */
SPEECH_C_API void Speech_Prefer_Sapi(bool prefer_sapi);

/**
 * @brief Checks if SAPI is loaded and active.
 * @return A boolean indicating whether SAPI is loaded and active.
 */
SPEECH_C_API bool Speech_Sapi_Loaded();

/**
 * @brief Initializes the SAPI speech engine.
 */
SPEECH_C_API void Sapi_Init();

/**
 * @brief Releases resources associated with the SAPI speech engine.
 */
SPEECH_C_API void Sapi_Release();

/**
 * @brief Retrieves the name of the currently selected SAPI voice.
 * @return A const wchar_t string representing the current SAPI voice name.
 */
SPEECH_C_API const wchar_t *Sapi_Get_Current_Voice();

/**
 * @brief Retrieves the name of a SAPI voice by its index.
 * @param index The index of the SAPI voice to retrieve.
 * @return A const wchar_t string representing the name of the SAPI voice.
 */
SPEECH_C_API const wchar_t *Sapi_Get_Voice(int index);

/**
 * @brief Sets the current SAPI voice by name.
 * @param voice A const wchar_t string representing the name of the voice to
 * set.
 */
SPEECH_C_API void Sapi_Set_Voice(const wchar_t *voice);

/**
 * @brief Sets the current SAPI voice by its index.
 * @param index The index of the SAPI voice to set.
 */
SPEECH_C_API void Sapi_Set_Voice_By_Index(int index);

/**
 * @brief Retrieves the number of available SAPI voices.
 * @return An integer representing the number of available SAPI voices.
 */
SPEECH_C_API int Sapi_Get_Voices();

/**
 * @brief Gets the current volume of the SAPI voice.
 * @return A float representing the current SAPI voice volume.
 */
SPEECH_C_API float Sapi_Voice_Get_Volume();

/**
 * @brief Sets the volume for the SAPI voice.
 * @param volume A float representing the volume to be set for the SAPI voice.
 */
SPEECH_C_API void Sapi_Voice_Set_Volume(float volume);

/**
 * @brief Gets the current speaking rate of the SAPI voice.
 * @return A float representing the current SAPI voice speaking rate.
 */
SPEECH_C_API float Sapi_Voice_Get_Rate();

/**
 * @brief Sets the speaking rate for the SAPI voice.
 * @param rate A float representing the speaking rate to be set for the SAPI
 * voice.
 */
SPEECH_C_API void Sapi_Voice_Set_Rate(float rate);

/**
 * @brief Speaks the given text using the SAPI voice.
 * @param text A const wchar_t string representing the text to be spoken.
 * @param _interrupt A boolean indicating whether to interrupt any ongoing
 * speech. Default is false.
 * @param _xml A boolean indicating whether the input text contains SSML markup.
 * Default is false.
 */
SPEECH_C_API void Sapi_Speak(const wchar_t *text,
                             bool _interrupt SPEECH_C_DEFAULT(false),
                             bool _xml SPEECH_C_DEFAULT(false));

/**
 * @brief Outputs the given text to an audio file using the SAPI voice.
 * @param filename A const char string representing the name of the output audio
 * file.
 * @param text A const wchar_t string representing the text to be converted to
 * speech.
 * @param _xml A boolean indicating whether the input text contains SSML markup.
 * Default is false.
 */
SPEECH_C_API void Sapi_Output_File(const char *filename, const wchar_t *text,
                                   bool _xml SPEECH_C_DEFAULT(false));

/**
 * @brief Pauses the current SAPI speech output.
 */
SPEECH_C_API void Sapi_Pause();

/**
 * @brief Resumes the previously paused SAPI speech output.
 */
SPEECH_C_API void Sapi_Resume();

/**
 * @brief Stops the current SAPI speech output.
 */
SPEECH_C_API void Sapi_Stop();
#endif // _WIN32

#ifdef __cplusplus
} // extern "C"
#endif // cplusplus
#endif // SPEECH_CORE_H
