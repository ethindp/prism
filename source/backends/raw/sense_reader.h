// Created by Microsoft (R) C/C++ Compiler Version 14.50.35729.0 (7ae222c6).
//
// C++ source equivalent of type library C:\\Program Files\\Xvision\\Sense Reader\\xvsrd.exe
// compiler-generated file - DO NOT EDIT!

#pragma once
#pragma pack(push, 8)

#include <comdef.h>

//
// Forward references and typedefs
//

struct __declspec(uuid("02618547-7600-4ff3-ba96-748375317805"))
/* LIBID */ __SenseReader;
enum __MIDL___MIDL_itf_SenseReader_0000_0016_0001 : int;
enum __MIDL___MIDL_itf_SenseReader_0000_0016_0002 : int;
enum __MIDL___MIDL_itf_SenseReader_0000_0016_0003 : int;
enum __MIDL___MIDL_itf_SenseReader_0000_0016_0004 : int;
enum __MIDL___MIDL_itf_SenseReader_0000_0016_0005 : int;
struct __declspec(uuid("0f5c8d0a-37bc-49f4-a3b0-e9b9e3e7791c"))
/* dual interface */ IXVSpeechControlers;
struct __declspec(uuid("6c28435b-6c09-4647-9348-6171deed86de"))
/* dual interface */ IXVSpeechControler;
struct __declspec(uuid("45387039-5cf4-42b5-bba8-7c722573cdc2"))
/* dual interface */ IXVSpeechEngineInfos;
struct __declspec(uuid("7ba0195d-7924-4b57-ae8d-5e1d9c3fef6f"))
/* dual interface */ IXVSpeechEngineInfo;
struct __declspec(uuid("b935c5cd-7c2b-4c60-89e6-264a67beb8c8"))
/* dual interface */ IXVSpeakInfo;
enum __MIDL___MIDL_itf_SenseReader_0000_0000_0003 : int;
struct __declspec(uuid("90c93400-39f0-44c4-b64b-8c48b13b9c8e"))
/* dual interface */ IXVSpeechControler2;
enum __MIDL___MIDL_itf_SenseReader_0000_0000_0002 : int;
struct __declspec(uuid("78b44faa-99fd-45b0-9b42-e1bc0693d117"))
/* dual interface */ IXVSecondSpeechEngineInfos;
struct __declspec(uuid("d84c9198-fa9f-44ac-a1ff-29b9c388d4d3"))
/* dual interface */ IXVSecondSpeechEngineInfo;
enum __MIDL___MIDL_itf_SenseReader_0000_0000_0004 : int;
struct __declspec(uuid("ead677f7-9741-4c11-973d-0ae9508191d1"))
/* dual interface */ IXVScript;
struct __declspec(uuid("7087a040-c069-4784-96fc-fc2e7f166737"))
/* dual interface */ IXVApplication;
struct __declspec(uuid("794022e5-15cb-46a1-b5d8-bd44ed7cdbe9"))
/* dual interface */ IXVApplication2;
struct __declspec(uuid("34ef45d7-ea44-4f11-a9dc-f173bc281073"))
/* dispinterface */ _IXVSpeechControlerEvents;
struct /* coclass */ SenseReaderApplication;
struct /* coclass */ XVSpeechControler;
struct /* coclass */ ShowOutputSettingsParam;
struct __declspec(uuid("fa1fcf5f-6c07-4076-81db-58739d2142e7"))
/* dual interface */ IShowOutputSettingsParam;
struct /* coclass */ ScheduleSettingsParam;
struct __declspec(uuid("9c0c96ba-a78a-4a92-adb8-4a1ce460745c"))
/* dual interface */ IScheduleSettingsParam;
typedef enum __MIDL___MIDL_itf_SenseReader_0000_0016_0001 AUTOREADTYPE;
typedef enum __MIDL___MIDL_itf_SenseReader_0000_0016_0002 CONSTANTS;
typedef enum __MIDL___MIDL_itf_SenseReader_0000_0016_0003 STRINGCONVERTFLAGS;
typedef enum __MIDL___MIDL_itf_SenseReader_0000_0016_0004 REPEATFLAGTYPE;
typedef enum __MIDL___MIDL_itf_SenseReader_0000_0016_0005 DETAILCASETYPE;
typedef enum __MIDL___MIDL_itf_SenseReader_0000_0000_0003 SPEECHSTRINGLEVELTYPE;
typedef enum __MIDL___MIDL_itf_SenseReader_0000_0000_0002 SPEECHENGINEABLETYPE;
typedef enum __MIDL___MIDL_itf_SenseReader_0000_0000_0004 SECONDSPEECHMODETYPE;

//
// Smart pointer typedef declarations
//

_COM_SMARTPTR_TYPEDEF(IXVSpeechEngineInfo, __uuidof(IXVSpeechEngineInfo));
_COM_SMARTPTR_TYPEDEF(IXVSpeechEngineInfos, __uuidof(IXVSpeechEngineInfos));
_COM_SMARTPTR_TYPEDEF(IXVSpeakInfo, __uuidof(IXVSpeakInfo));
_COM_SMARTPTR_TYPEDEF(IXVSpeechControler, __uuidof(IXVSpeechControler));
_COM_SMARTPTR_TYPEDEF(IXVSpeechControlers, __uuidof(IXVSpeechControlers));
_COM_SMARTPTR_TYPEDEF(IXVSecondSpeechEngineInfo, __uuidof(IXVSecondSpeechEngineInfo));
_COM_SMARTPTR_TYPEDEF(IXVSecondSpeechEngineInfos, __uuidof(IXVSecondSpeechEngineInfos));
_COM_SMARTPTR_TYPEDEF(IXVSpeechControler2, __uuidof(IXVSpeechControler2));
_COM_SMARTPTR_TYPEDEF(IXVScript, __uuidof(IXVScript));
_COM_SMARTPTR_TYPEDEF(IXVApplication, __uuidof(IXVApplication));
_COM_SMARTPTR_TYPEDEF(IXVApplication2, __uuidof(IXVApplication2));
_COM_SMARTPTR_TYPEDEF(_IXVSpeechControlerEvents, __uuidof(_IXVSpeechControlerEvents));
_COM_SMARTPTR_TYPEDEF(IShowOutputSettingsParam, __uuidof(IShowOutputSettingsParam));
_COM_SMARTPTR_TYPEDEF(IScheduleSettingsParam, __uuidof(IScheduleSettingsParam));

//
// Type library items
//

enum __MIDL___MIDL_itf_SenseReader_0000_0016_0001 : int
{
    AUTOREAD_INPUT = 1,
    AUTOREAD_CURSORING = 2,
    AUTOREAD_EVENT = 4,
    AUTOREAD_FOREGROUND = 8,
    AUTOREAD_ALL = -1
};

enum __MIDL___MIDL_itf_SenseReader_0000_0016_0002 : int
{
    VOICE_MIN_VALUE = 0,
    VOICE_MAX_VALUE = 10,
    REPEAT_MIN = 1,
    REPEAT_MAX = 9
};

enum __MIDL___MIDL_itf_SenseReader_0000_0016_0003 : int
{
    FLAG_ENGLISH = 1,
    FLAG_DIGIT = 2,
    FLAG_HANGUL = 8,
    FLAG_HANJA = 16,
    FLAG_SPACE = 32,
    FLAG_TAB = 64,
    FLAG_RETURN = 128,
    FLAG_BACKSPACE = 256,
    FLAG_ESCAPE = 512,
    FLAG_CAPITAL = 1024,
    FLAG_UNKNOWNCHAR = 2048,
    FLAG_USER = 4096,
    FLAG_CHARSET = 8192,
    FLAG_SPECIAL = 16384,
    FLAG_NUMBERTOGETHER = 32768,
    FLAG_ENGLISHCONTRACTIONTOGETHER = 65536,
    FLAG_ALLCONVERT = static_cast<int>(0x80000000)
};

enum __MIDL___MIDL_itf_SenseReader_0000_0016_0004 : int
{
    REPEATFLAG_USE = 1,
    REPEATFLAG_SAY = 2
};

enum __MIDL___MIDL_itf_SenseReader_0000_0016_0005 : int
{
    MAXDETAILCASE = 3,
    PUNCDETAILCASE = 1,
    DIGITDETAILCASE = 2,
    SPACEDETAILCASE = 1,
    ENGDETAILCASE = 2,
    HANDETAILCASE = 3,
    HANPHONEMEDETAILCASE = 2,
    HANJADETAILCASE = 3,
    SPECIALDETAILCASE = 2
};

struct __declspec(uuid("7ba0195d-7924-4b57-ae8d-5e1d9c3fef6f"))
IXVSpeechEngineInfo : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_LanguageId (
        /*[out,retval]*/ long * pLangId ) = 0;
      virtual HRESULT __stdcall put_LanguageId (
        /*[in]*/ long pLangId ) = 0;
      virtual HRESULT __stdcall get_EngineKind (
        /*[out,retval]*/ BSTR * pKind ) = 0;
      virtual HRESULT __stdcall put_EngineKind (
        /*[in]*/ BSTR pKind ) = 0;
      virtual HRESULT __stdcall get_Name (
        /*[out,retval]*/ BSTR * pName ) = 0;
      virtual HRESULT __stdcall put_Name (
        /*[in]*/ BSTR pName ) = 0;
};

struct __declspec(uuid("45387039-5cf4-42b5-bba8-7c722573cdc2"))
IXVSpeechEngineInfos : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get__NewEnum (
        /*[out,retval]*/ IUnknown * * ppEnumerator ) = 0;
      virtual HRESULT __stdcall get_Count (
        /*[out,retval]*/ long * pCount ) = 0;
      virtual HRESULT __stdcall Item (
        /*[in]*/ long nIndex,
        /*[out,retval]*/ struct IXVSpeechEngineInfo * * ppEngineInfo ) = 0;
      virtual HRESULT __stdcall Rebuild ( ) = 0;
};

struct __declspec(uuid("b935c5cd-7c2b-4c60-89e6-264a67beb8c8"))
IXVSpeakInfo : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_LanguageId (
        /*[out,retval]*/ long * pLangId ) = 0;
      virtual HRESULT __stdcall put_LanguageId (
        /*[in]*/ long pLangId ) = 0;
      virtual HRESULT __stdcall get_Speed (
        /*[out,retval]*/ long * pSpeed ) = 0;
      virtual HRESULT __stdcall put_Speed (
        /*[in]*/ long pSpeed ) = 0;
      virtual HRESULT __stdcall get_Pitch (
        /*[out,retval]*/ long * pPitch ) = 0;
      virtual HRESULT __stdcall put_Pitch (
        /*[in]*/ long pPitch ) = 0;
      virtual HRESULT __stdcall get_Volume (
        /*[out,retval]*/ long * pVolume ) = 0;
      virtual HRESULT __stdcall put_Volume (
        /*[in]*/ long pVolume ) = 0;
      virtual HRESULT __stdcall get_Detail (
        /*[out,retval]*/ long * pDetail ) = 0;
      virtual HRESULT __stdcall put_Detail (
        /*[in]*/ long pDetail ) = 0;
      virtual HRESULT __stdcall get_HanjaDetail (
        /*[out,retval]*/ long * pRetVal ) = 0;
      virtual HRESULT __stdcall put_HanjaDetail (
        /*[in]*/ long pRetVal ) = 0;
      virtual HRESULT __stdcall get_SpecialDetail (
        /*[out,retval]*/ long * pRetVal ) = 0;
      virtual HRESULT __stdcall put_SpecialDetail (
        /*[in]*/ long pRetVal ) = 0;
      virtual HRESULT __stdcall get_Flags (
        /*[out,retval]*/ long * pFlags ) = 0;
      virtual HRESULT __stdcall put_Flags (
        /*[in]*/ long pFlags ) = 0;
      virtual HRESULT __stdcall get_RepeatFlags (
        /*[out,retval]*/ long * pFlags ) = 0;
      virtual HRESULT __stdcall put_RepeatFlags (
        /*[in]*/ long pFlags ) = 0;
      virtual HRESULT __stdcall get_RepeatCount (
        /*[out,retval]*/ long * pCount ) = 0;
      virtual HRESULT __stdcall put_RepeatCount (
        /*[in]*/ long pCount ) = 0;
      virtual HRESULT __stdcall get_PuncReadIndex (
        /*[out,retval]*/ long * pIndex ) = 0;
      virtual HRESULT __stdcall put_PuncReadIndex (
        /*[in]*/ long pIndex ) = 0;
      virtual HRESULT __stdcall get_SecondSpeech (
        /*[out,retval]*/ VARIANT_BOOL * pSecondVoice ) = 0;
      virtual HRESULT __stdcall put_SecondSpeech (
        /*[in]*/ VARIANT_BOOL pSecondVoice ) = 0;
      virtual HRESULT __stdcall get_SkipSpeakDone (
        /*[out,retval]*/ VARIANT_BOOL * pRetVal ) = 0;
      virtual HRESULT __stdcall put_SkipSpeakDone (
        /*[in]*/ VARIANT_BOOL pRetVal ) = 0;
      virtual HRESULT __stdcall Clone (
        /*[out,retval]*/ struct IXVSpeakInfo * * ppInfo ) = 0;
};

struct __declspec(uuid("6c28435b-6c09-4647-9348-6171deed86de"))
IXVSpeechControler : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_EngineInfos (
        /*[out,retval]*/ struct IXVSpeechEngineInfos * * pInfos ) = 0;
      virtual HRESULT __stdcall ChangeEngine (
        /*[in]*/ struct IXVSpeechEngineInfo * engineInfo ) = 0;
      virtual HRESULT __stdcall get_Name (
        /*[out,retval]*/ BSTR * pN ) = 0;
      virtual HRESULT __stdcall put_Name (
        /*[in]*/ BSTR pN ) = 0;
      virtual HRESULT __stdcall get_Speaking (
        /*[out,retval]*/ VARIANT_BOOL * pVal ) = 0;
      virtual HRESULT __stdcall get_DefaultSpeakInfo (
        /*[out,retval]*/ struct IXVSpeakInfo * * ppInfo ) = 0;
      virtual HRESULT __stdcall put_DefaultSpeakInfo (
        /*[in]*/ struct IXVSpeakInfo * ppInfo ) = 0;
      virtual HRESULT __stdcall NewSpeakInfo (
        /*[out,retval]*/ struct IXVSpeakInfo * * ppInfo ) = 0;
      virtual HRESULT __stdcall Speak (
        /*[in]*/ BSTR text,
        /*[in]*/ VARIANT SpeakInfo,
        /*[in]*/ SPEECHSTRINGLEVELTYPE stringLevel,
        /*[in]*/ VARIANT_BOOL sync ) = 0;
      virtual HRESULT __stdcall Stop (
        /*[in]*/ VARIANT_BOOL sync ) = 0;
      virtual HRESULT __stdcall Init (
        /*[in]*/ BSTR iniPath,
        /*[in]*/ BSTR dictionaryFolder ) = 0;
      virtual HRESULT __stdcall get_PuncReadCount (
        /*[out,retval]*/ long * pCount ) = 0;
      virtual HRESULT __stdcall get_PuncReadName (
        /*[in]*/ long index,
        /*[out,retval]*/ BSTR * pName ) = 0;
      virtual HRESULT __stdcall get_CurrentEngineInfo (
        /*[out,retval]*/ struct IXVSpeechEngineInfo * * engineInfo ) = 0;
      virtual HRESULT __stdcall put_CurrentEngineInfo (
        /*[in]*/ struct IXVSpeechEngineInfo * engineInfo ) = 0;
};

struct __declspec(uuid("0f5c8d0a-37bc-49f4-a3b0-e9b9e3e7791c"))
IXVSpeechControlers : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get__NewEnum (
        /*[out,retval]*/ IUnknown * * ppEnumerator ) = 0;
      virtual HRESULT __stdcall get_Count (
        /*[out,retval]*/ long * pCount ) = 0;
      virtual HRESULT __stdcall Item (
        /*[in]*/ VARIANT index,
        /*[out,retval]*/ struct IXVSpeechControler * * ppControler ) = 0;
      virtual HRESULT __stdcall Add (
        /*[in]*/ BSTR Name,
        /*[out,retval]*/ struct IXVSpeechControler * * ppControler ) = 0;
      virtual HRESULT __stdcall Remove (
        /*[in]*/ struct IXVSpeechControler * controler ) = 0;
};

enum __MIDL___MIDL_itf_SenseReader_0000_0000_0003 : int
{
    SPEECHSTRINGLEVEL0 = 0,
    SPEECHSTRINGLEVEL1 = 1,
    SPEECHSTRINGLEVEL2 = 2
};

enum __MIDL___MIDL_itf_SenseReader_0000_0000_0002 : int
{
    ENGINEABLE_NOTIFYWORDSIGN = 0,
    ENGINEABLE_CHANGETOWAVE = 1,
    ENGINEABLE_SETSOUNDCARDID = 2,
    ENGINEABLE_SPLITCONSONANT = 3
};

struct __declspec(uuid("d84c9198-fa9f-44ac-a1ff-29b9c388d4d3"))
IXVSecondSpeechEngineInfo : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_LanguageId (
        /*[out,retval]*/ long * pLangId ) = 0;
      virtual HRESULT __stdcall put_LanguageId (
        /*[in]*/ long pLangId ) = 0;
      virtual HRESULT __stdcall get_AutoMember (
        /*[out,retval]*/ VARIANT_BOOL * pValue ) = 0;
      virtual HRESULT __stdcall put_AutoMember (
        /*[in]*/ VARIANT_BOOL pValue ) = 0;
      virtual HRESULT __stdcall get_SpeechEngineInfo (
        /*[out,retval]*/ struct IXVSpeechEngineInfo * * ppInfo ) = 0;
      virtual HRESULT __stdcall put_SpeechEngineInfo (
        /*[in]*/ struct IXVSpeechEngineInfo * ppInfo ) = 0;
      virtual HRESULT __stdcall get_Volume (
        /*[out,retval]*/ long * pValue ) = 0;
      virtual HRESULT __stdcall put_Volume (
        /*[in]*/ long pValue ) = 0;
      virtual HRESULT __stdcall get_Speed (
        /*[out,retval]*/ long * pValue ) = 0;
      virtual HRESULT __stdcall put_Speed (
        /*[in]*/ long pValue ) = 0;
      virtual HRESULT __stdcall get_Pitch (
        /*[out,retval]*/ long * pValue ) = 0;
      virtual HRESULT __stdcall put_Pitch (
        /*[in]*/ long pValue ) = 0;
};

struct __declspec(uuid("78b44faa-99fd-45b0-9b42-e1bc0693d117"))
IXVSecondSpeechEngineInfos : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get__NewEnum (
        /*[out,retval]*/ IUnknown * * ppEnumerator ) = 0;
      virtual HRESULT __stdcall get_Count (
        /*[out,retval]*/ long * pCount ) = 0;
      virtual HRESULT __stdcall Item (
        /*[in]*/ long nIndex,
        /*[out,retval]*/ struct IXVSecondSpeechEngineInfo * * ppInfo ) = 0;
      virtual HRESULT __stdcall Add (
        /*[in]*/ struct IXVSecondSpeechEngineInfo * pInfo ) = 0;
};

struct __declspec(uuid("90c93400-39f0-44c4-b64b-8c48b13b9c8e"))
IXVSpeechControler2 : IXVSpeechControler
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_IsEngineAble (
        /*[in]*/ SPEECHENGINEABLETYPE able,
        /*[out,retval]*/ VARIANT_BOOL * pRetVal ) = 0;
      virtual HRESULT __stdcall TextToPcmBuffer (
        /*[in]*/ BSTR text,
        /*[in]*/ struct IXVSpeakInfo * SpeakInfo,
        /*[in,out]*/ SAFEARRAY * resultData,
        /*[out]*/ long * pLeng,
        /*[in]*/ long flag,
        /*[in]*/ long threadId,
        /*[out,retval]*/ long * pRetVal ) = 0;
      virtual HRESULT __stdcall get_SecondSpeechEngineInfos (
        /*[out,retval]*/ struct IXVSecondSpeechEngineInfos * * pInfos ) = 0;
      virtual HRESULT __stdcall put_SecondSpeechEngineInfos (
        /*[in]*/ struct IXVSecondSpeechEngineInfos * pInfos ) = 0;
      virtual HRESULT __stdcall ChangeSecondSpeechMode (
        /*[in]*/ SECONDSPEECHMODETYPE mode ) = 0;
      virtual HRESULT __stdcall get_SecondSpeechMode (
        /*[out,retval]*/ SECONDSPEECHMODETYPE * pMode ) = 0;
      virtual HRESULT __stdcall GetSoundCardList (
        /*[out,retval]*/ SAFEARRAY * * pList ) = 0;
      virtual HRESULT __stdcall get_SoundCard (
        /*[out,retval]*/ BSTR * card ) = 0;
      virtual HRESULT __stdcall put_SoundCard (
        /*[in]*/ BSTR card ) = 0;
};

enum __MIDL___MIDL_itf_SenseReader_0000_0000_0004 : int
{
    SECONDSPEECHMODE_OFF = 0,
    SECONDSPEECHMODE_AUTO = 1,
    SECONDSPEECHMODE_MANUAL = 2,
    SECONDSPEECHMODE_BEGIN = 0,
    SECONDSPEECHMODE_END = 2
};

struct __declspec(uuid("ead677f7-9741-4c11-973d-0ae9508191d1"))
IXVScript : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall ConnectObject (
        /*[in]*/ VARIANT obj,
        /*[in]*/ BSTR prefix ) = 0;
};

struct __declspec(uuid("7087a040-c069-4784-96fc-fc2e7f166737"))
IXVApplication : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_SpeechControlers (
        /*[out,retval]*/ struct IXVSpeechControlers * * pControlers ) = 0;
      virtual HRESULT __stdcall get_SpeechControler (
        /*[out,retval]*/ struct IXVSpeechControler * * pControler ) = 0;
      virtual HRESULT __stdcall Speak (
        /*[in]*/ BSTR text ) = 0;
      virtual HRESULT __stdcall StopSpeaking ( ) = 0;
      virtual HRESULT __stdcall RegisterAutoReadExceptionWindow (
        /*[in]*/ long wnd,
        /*[in]*/ long option,
        /*[in]*/ VARIANT_BOOL applyChildren ) = 0;
      virtual HRESULT __stdcall RemoveAutoReadExceptionWindow (
        /*[in]*/ long wnd ) = 0;
      virtual HRESULT __stdcall get_DemoMode (
        /*[out,retval]*/ VARIANT_BOOL * pVal ) = 0;
};

struct __declspec(uuid("794022e5-15cb-46a1-b5d8-bd44ed7cdbe9"))
IXVApplication2 : IXVApplication
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall SetFavoriteParam (
        /*[in]*/ long lIndex,
        /*[in]*/ BSTR bstrName,
        /*[in]*/ BSTR bstrParam ) = 0;
      virtual HRESULT __stdcall get_SenseReaderId (
        /*[out,retval]*/ BSTR * pVal ) = 0;
      virtual HRESULT __stdcall CallFunction (
        /*[in]*/ BSTR bstrName ) = 0;
};

struct __declspec(uuid("34ef45d7-ea44-4f11-a9dc-f173bc281073"))
_IXVSpeechControlerEvents : IDispatch
{};

struct __declspec(uuid("8c94f442-b088-412c-9dbb-042cd8c04a78"))
SenseReaderApplication;
    // [ default ] interface IXVApplication

struct __declspec(uuid("b5aa829b-9fcd-45d4-92ec-81cdc684432d"))
XVSpeechControler;
    // [ default ] interface IXVSpeechControler2
    // [ default, source ] dispinterface _IXVSpeechControlerEvents

struct __declspec(uuid("1665383c-4964-4f18-863e-5c84c13a3141"))
ShowOutputSettingsParam;
    // [ default ] interface IShowOutputSettingsParam

struct __declspec(uuid("fa1fcf5f-6c07-4076-81db-58739d2142e7"))
IShowOutputSettingsParam : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall get_BKColor (
        /*[out,retval]*/ BSTR * pVal ) = 0;
      virtual HRESULT __stdcall put_BKColor (
        /*[in]*/ BSTR pVal ) = 0;
      virtual HRESULT __stdcall get_TextColor (
        /*[out,retval]*/ BSTR * pVal ) = 0;
      virtual HRESULT __stdcall put_TextColor (
        /*[in]*/ BSTR pVal ) = 0;
      virtual HRESULT __stdcall get_ShowOutput (
        /*[out,retval]*/ VARIANT_BOOL * pVal ) = 0;
      virtual HRESULT __stdcall put_ShowOutput (
        /*[in]*/ VARIANT_BOOL pVal ) = 0;
      virtual HRESULT __stdcall get_ShowAlign (
        /*[out,retval]*/ long * pVal ) = 0;
      virtual HRESULT __stdcall put_ShowAlign (
        /*[in]*/ long pVal ) = 0;
};

struct __declspec(uuid("ceddcdfd-8b8a-4ad3-848e-b499a505d61e"))
ScheduleSettingsParam;
    // [ default ] interface IScheduleSettingsParam

struct __declspec(uuid("9c0c96ba-a78a-4a92-adb8-4a1ce460745c"))
IScheduleSettingsParam : IDispatch
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall SetBit (
        /*[in]*/ long lBit,
        /*[in]*/ VARIANT_BOOL bSet ) = 0;
      virtual HRESULT __stdcall GetBit (
        /*[in]*/ long lBit,
        /*[out,retval]*/ VARIANT_BOOL * pSet ) = 0;
      virtual HRESULT __stdcall get_GetAllBits (
        /*[out,retval]*/ unsigned long * pBits ) = 0;
      virtual HRESULT __stdcall get_Year (
        /*[out,retval]*/ long * pVal ) = 0;
      virtual HRESULT __stdcall put_Year (
        /*[in]*/ long pVal ) = 0;
      virtual HRESULT __stdcall get_Month (
        /*[out,retval]*/ long * pVal ) = 0;
      virtual HRESULT __stdcall put_Month (
        /*[in]*/ long pVal ) = 0;
      virtual HRESULT __stdcall get_Day (
        /*[out,retval]*/ long * pVal ) = 0;
      virtual HRESULT __stdcall put_Day (
        /*[in]*/ long pVal ) = 0;
      virtual HRESULT __stdcall get_Hour (
        /*[out,retval]*/ long * pVal ) = 0;
      virtual HRESULT __stdcall put_Hour (
        /*[in]*/ long pVal ) = 0;
      virtual HRESULT __stdcall get_Minute (
        /*[out,retval]*/ long * pVal ) = 0;
      virtual HRESULT __stdcall put_Minute (
        /*[in]*/ long pVal ) = 0;
      virtual HRESULT __stdcall get_Name (
        /*[out,retval]*/ BSTR * pVal ) = 0;
      virtual HRESULT __stdcall put_Name (
        /*[in]*/ BSTR pVal ) = 0;
      virtual HRESULT __stdcall get_Sound (
        /*[out,retval]*/ long * pVal ) = 0;
      virtual HRESULT __stdcall put_Sound (
        /*[in]*/ long pVal ) = 0;
      virtual HRESULT __stdcall get_nowYear (
        /*[out,retval]*/ long * pVal ) = 0;
};

#pragma pack(pop)
