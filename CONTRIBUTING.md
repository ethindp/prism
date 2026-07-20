# Contributing

Thank you for your interest in contributing to Prism. This document aims to help you get started with that process and lays down some ground rules that all contributions must obey.

## Ground rules

Let's get the boring stuff out of the way first. A contribution, for the purposes of this section and the following rules, is an issue, PR, or discussion.

Most of what follows is common sense, and much of it is process meant to keep review smooth rather than hard law. A handful, though, will get a contribution rejected no matter how good the code is: skipping the DCO (rule 4), routing a security report through a public tracker (rule 5), shipping a new first-party source file without an SPDX identifier (rule 10), failing a required lint or CI job (rule 11), or changing a documented contract without updating the manual in the same PR (rule 14). If you're short on time, make sure of those five and skim the rest.

1. Please use issues for actual bugs. Feature requests or other general discussions belong as a discussion. If you accidentally open an issue instead of a discussion, you should convert it to a discussion or, failing that, a maintainer will do it for you. If it is determined (at the discretion of the maintainers) that you are deliberately opening an issue instead of a discussion to frustrate the contribution process, you may be suspended from opening issues at all. If you are uncertain of which you should open, open a discussion.
2. Please open one contribution per topic. Multiple topics covered in one contribution makes the overall process more difficult and makes it harder to figure out what was solved and when. If you do open a contribution and it does multiple things, a maintainer will either close it and ask you to open one contribution per topic or split it into sub-contributions where supported.
3. Please ensure that you actually understand the contribution. Prism does not explicitly prohibit AI-assisted development, but contributions authored entirely by an AI will be refused. The Developer Certificate of Origin below asks you to certify, among other things, that you created the contribution, in whole or in part, and have the right to submit it under the project's license. A contribution you wrote none of is not one you can certify under those terms, and there is no clause under which an AI can certify it on your behalf. Independently of that, we require every contributor to be able to explain and maintain their work, which a wholesale AI contribution defeats. As such, you must be capable of proving that you authored the contribution by whatever means you find necessary. If you cannot adequately explain a contribution or how it affects the code, or if you need an LLM to hold your hand or reply on your behalf, do not open the contribution to begin with. This is because part of Prism's development is making all of us better software engineers, and we'd like your contributing to Prism to do the same; an LLM authoring an entire contribution for you does not. This rule is especially important if your contribution is complex (e.g., a new backend), is nebulous or controversial, is difficult to implement due to a rough landscape (e.g., SSML), etc., and in those instances your contribution may spark a discussion to which you are expected to take part.
4. Your contribution must comply with the [Developer Certificate of Origin](https://developercertificate.org) (DCO). If it does not for any reason, it will be refused.
5. Do NOT submit security reports via any tracker, be that issues, pull requests or anything else. Submit them through the security reporting system specified by this repository. Submitting a vulnerability report (potentially along with its fix) in a public venue such as the issue tracker only makes it possible for adversaries to exploit the vulnerability before it can be verified and fixed.
6. In your contribution, please include as much detail as possible. For example, if you wish a new backend to be added, provide links to its SDKs, documentation or other materials. Similarly, if you open a PR, document what the PR does and be ready to answer the why of your decision-making process. This rule is particularly critical for issues: for those, please include the following at minimum:
    - The version of Prism you are running;
    - The operating system(s) and backend(s) which suffer the problem, along with their versions and architectures;
    - The compiler and its version, together with any non-default build options, if this version of Prism was one you built from source; and
    - a minimal self-contained program reproducing the problem.
7. Where the nature of a bug renders furnishing of a reproduction impractical (e.g., a data race or timing-dependent failure), you may omit the reproduction, provided that the report states so, describes the conditions under which the problem has been observed, and includes any diagnostic material available to you, such as sanitizer output and logs, or a stack trace. In these instances, the rule of thumb is: the more information you can provide, the better.
8. If your contribution is a pull request, please avoid fluff. Fluff is generally defined as referring to language that adds length or formality without adding actual meaning. For example, if your PR fixes a bug, don't include a fix section describing how to fix the problem.
9. If your contribution is a feature request, please state the request in full together with the problem it solves. A problem arising in actual use constitutes the strongest case. An anticipatory feature is admissible provided that the reasoning is supplied. For any change exceeding a small and obvious fix, you SHOULD discuss the intended approach before writing the code. Prism is subject to design constraints that are not apparent from outside, and early consultation frequently prevents wasted effort.
10. Each new source file shall bear an SPDX identifier, where that source file is first-party code. Third-party code shall retain its own identifier, shall deposit its license text under the LICENSES directory, and shall be recorded in NOTICE.
11. A pull request shall pass every lint and every continuous-integration job that its changes trigger. A failing linter forecloses a merge. A diagnostic shall not be suppressed, whether by inline annotation or by edit to the configuration, except where the suppression is necessary and its justification is stated in the pull request. The governing presumption is that a diagnostic the contributor cannot satisfy has identified code wanting restructuring rather than silencing.
12. A commit message shall carry an imperative, capitalized subject, ideally within 72 characters, an area prefix being welcome. The body shall explain the reason for the change rather than restate its content. Issue-closing keywords belong in the pull request description rather than the commit message. A branch shall be updated by rebase and not by merge. Pull requests target master, unless a development branch already exists for the next version; in that instance, the pull request should target that branch.
13. A bug fix SHOULD be accompanied by a regression test that fails before the fix and passes after it. The C++ tests run under GoogleTest; the Python tests run under pytest with timeouts. Both shall be deterministic and bounded. Pull requests consisting solely of tests are welcome.
14. A change altering the documented contract of a function shall update the corresponding Manual chapter within the same pull request. New public API shall be documented before the Contribution is considered complete.
15. When altering the public API, please use as many of the compiler annotation macros as you are able to. When doing so, do not include checks to which the annotation would eliminate; for example, if you use PRISM_NONNULL, do not then add a check for NULL in the function.

## Development setup; general processes

Prism is designed to be easy to get started with. In general, you needn't any third-party dependencies, besides a C++23-capable compiler, unless building with a package manager solution such as vcpkg: in that instance, this document is probably not for you.

Prism has a few options, noted in the README, which are developer-only. You are free to take advantage of these, or to add more, as you see fit:

* When writing code you wish to be autovectorized, enable the PRISM_DIAGNOSE_VECTORIZATION option. This instructs the compiler to make available to you reports of why it did or did not vectorize a certain part of code. Some code is just not friendly to autovectorization, and there is little you can do about it. If you can't figure out how to get the compiler to emit optimal vectorized code, please don't force the issue via compiler intrinsics without opening a discussion first. It is usually the case that even non-vectorized code performs adequately enough that it poses no actual problem.
* If you for whatever reason wish to update the Java bindings (e.g., to add a new method), the procedure is a bit more complicated. Assuming that you have already updated idl/tts_backend.djinni:
    1. Build with the PRISM_REGENERATE_DJINNI option, setting DJINNI_JAR to the path to djinni or djinni.bat (or if you built it from source, the JAR that javac produced). You will need a JRE for this to actually work.
    2. Run your chosen generator specifying the regen-djinni target, which will instruct Djinni to regenerate all of the auto-generated JNI code.
    3. Commit the result.
    4. Update source/backends/java/TextToSpeechBackend.java to override the abstract method Djinni generated so that it returns not-implemented to callers.
    5. Verify that everything still builds with gradle. If spotless complains, run gradle SpotlessApply. Optionally also run gradle --init-script rewrite-init.gradle RewriteRun to apply openrewrite's additions. It will tell you that it formatted way more files than you modified; in general you can ignore this.
    6. Commit all of your modifications.
* If you update the Windows IDL files, you needn't do anything else other than commit them. The code will be auto-generated on the next CI build.
* The PRISM_ENABLE_LINTING option enables building with linters such as clang-tidy. You are very strongly encouraged to do this *before* you open a PR, but it is not required: the lint will be run when your PR is opened after the workflows have been approved. If your modification is instead to the Java code, you are strongly encouraged to run a full Java lint pass with gradle :lint, but again, this will be done after the workflows have been approved on your PR. As stated above, a PR will NOT be merged should a linter fail it.
* If you modify the shell scripts, please run shellcheck and shfmt, respectively, for linting and formatting, and, optionally, shellharden; if you modify the Python code, run ruff check and ruff check --fix. In the former case, fix as many errors as you are able to; we strongly encourage you to run shellcheck via shellcheck -o all. In the case of the latter, all errors MUST be fixed before your PR will get merged.

## Dependencies

Prism does not download dependencies excepting tests, whereupon we use [cpm.cmake](https://github.com/cpm-cmake/CPM.cmake) for googletest. This is because Prism is in package managers such as vcpkg, and these package managers require that all dependencies be available when the package is built, and internet access is usually disabled in these environments.

To add a dependency:

1. Drop it in third_party as a directory of its own.
2. Add it to cmake/PrismDeps.cmake, using the prism_declare_dependency function. At least the NAME and LICENSE options are required under all circumstances. This function is documented below. Please try to always use this function unless your dependency requires special handling; in that instance it is probably a better idea to open a discussion to figure out a path forward instead of trying to hack together custom CMake logic.
3. Ensure that Prism fully builds when PRISM_DEPENDENCY_PROVIDER is BUNDLED. You SHOULD also test with SYSTEM if your dependency is not header-only or bundled-only.

The synopsis of prism_declare_dependency is:

```cmake
function(
  prism_declare_dependency
  NAME name
  [HEADER_ONLY]
  [BUNDLED_ONLY]
  [PACKAGE pkg_name]
  [MIN_VERSION version]
  [BUNDLED_ROOT path]
  [LICENSE path]
  [LANGUAGE language]
  [SYSTEM_TARGETS targets]
  [BUNDLED_SOURCES sources]
  [BUNDLED_INCLUDES directories]
)
```

The arguments are described below.

| Argument | Description |
| --- | --- |
| NAME | Name of the dependency. This parameter is required. |
| HEADER_ONLY | Declares the dependency as header-only. Cannot be used together with BUNDLED_SOURCES. |
| BUNDLED_ONLY | Indicates that the dependency is only available from the bundled source tree. When specified, PACKAGE and SYSTEM_TARGETS must not be provided. |
| PACKAGE | Name passed to find_package() when using the system-provided dependency. Required unless BUNDLED_ONLY is specified. |
| MIN_VERSION | Minimum version accepted by find_package(). Optional. |
| BUNDLED_ROOT | Path, relative to PRISM_SOURCE_ROOT, containing the bundled dependency sources. Required. |
| LICENSE | Path, relative to PRISM_SOURCE_ROOT, to the bundled dependency's license file. Required. |
| LANGUAGE | Language used to compile bundled sources. Currently only C has special handling; all other values use the default CMake language settings. Optional. |
| SYSTEM_TARGETS | Imported CMake targets to link when using the system dependency. Required unless BUNDLED_ONLY is specified. Multiple targets may be provided. |
| BUNDLED_SOURCES | Source files, relative to BUNDLED_ROOT, used to build the bundled library. Required unless HEADER_ONLY is specified. Multiple files may be provided. |
| BUNDLED_INCLUDES | Include directories, relative to BUNDLED_ROOT, exported by the bundled dependency. One or more directories must be provided. |

The function creates the imported target prism::dep::<name>, which consumers should link against instead of referencing the underlying bundled or system dependency directly.

## Backends

Adding a new backend is a bit tricky and more involved than adding a dependency due to the number of moving parts. The general procedure is as follows:

1. Create a new source file under source/backends, with one or more classes inheriting from TextToSpeechBackend. In general, your backend should be gated behind the preprocessor macros the C++ implementation would define for the operating system (and, if needed, architectures) that the backend works on. If your backend works on all architectures Prism can compile for, then architecture macro guards are unnecessary and should be excluded. Be sure to implement get_name, get_features, and initialize, as well as a destructor, even if trivial; all other functions are optional. Lastly, register it with either of the backend registration macros. (This last part is critical: without it the backend will either not be compiled into the library or the linker will eliminate it as dead code.)
2. If your backend uses COM, use OLEView to extract the IDL from its type library and then use midl to generate source code to call into the type library. If OLEView generates invalid IDL, use MSVC's [#import](https://learn.microsoft.com/en-us/cpp/preprocessor/hash-import-directive-cpp?view=msvc-170) directive to generate a header file and rename the .tlh/.tli file to have h/cpp extensions, respectively, and add them to the CMake source files list.
3. Update the Backends namespace in the backend catalog header to define the backend ID that will be associated with your backend. Use the UDL when defining backends, and make the identifier inline and constexpr. Always use the auto type qualifier here instead of trying to derive the type yourself.
4. Update source/backend_check.cpp so the library can compile-time validate that the header and internal backend IDs for your new backend match.
5. Update the header to define a new PRISM_BACKEND_ macro defining the actual backend ID. Calculating this is currently rather annoying, since although we use FNV-1A internally, trying to compute the ID in a language like Python will break since Python uses arbitrary-precision integers. The simplest approach is to set the header ID to 0, let the build fail, and extract the computed ID from the compilers error messages. Make sure to convert it to hex!
6. Update the Python and GDExtension bindings with the backend ID you just extracted.
7. If your backend is an Android backend, the C++ source file should be a Djinni communication layer to the Java implementation and should do little to nothing else. You can use the existing Android backends as templates. You should always inherit from TextToSpeechBackend on the Java side, and not AbstractTextToSpeechBackend!
8. Lastly, declare the backend in cmake/PrismBackends.cmake using the prism_declare_backend function. Again, it is documented below.

If your backend depends on third-party libraries (e.g., proprietary DLLs), and the backend runs on Windows, additional steps are required:

1. Update defs/ with the module-definition file which corresponds to your backend. This should match the original DLL's symbols. Write both a 32-bit and 64-bit DEF file. Additionally, drop a header for the library under source/backends/raw, with the exact calling convention of the original library. If you are legally allowed to redistribute the header (which is usually the case for interfaces reproduced for interoperability), drop the official header there; otherwise, write your own.
2. Update source/delayimp.cpp with every function defined by the original DLL, as stubs, returning error codes indicating unavailability of the library, or the equivalent, in their own namespace, and the DLL stubs table with each symbol which needs to be stubbed. Ensure your stubs have the exact calling convention of the original library! For the stubs table, use stub_cast to launder the function pointer so that it is safe to call at run-time.
3. Update the DelayLoadFailureHook function with the appropriate logic to find and load the DLL, preferably using the registry or some other mechanism, so that an end-user needn't place the library next to Prism and it can be auto-loaded.
4. Update PrismPlatformWindows.cmake to include the delay-loaded DLL.
5. Update prism_shutdown to call __FUnloadDelayLoadedDLL2 to unload the delay-loaded DLL when the Prism context is destroyed.

If your backend can be directly implemented into Prism, all of the additional above steps can mostly be skipped, and the general process for adding any backend can be followed. We strongly prefer backends that can be directly implemented into Prism without delay loading, as delay loading is complex and easy to break, and implementing the backend directly into Prism means that Prism can stand in for the original DLL.

Your backends MUST always be self-contained translation units. No backend TU should ever depend on any other backend's TU. Ever.

Certain backend methods have special requirements. To wit:

* The get_name function should nearly always be a function returning a compile-time constant. Hence, it MUST return a std::string_view and be annotated with nodiscard. The sole exception to this is Android backends, which call into Java and get the name from that backend; however, even in that case, it should be something that identifies the backend to a consumer.
* The get_features function should do a run-time probe to determine if the backend is genuinely available. This differs from initialize which actually stands up the backend. For example, a COM backend should use [CoGetClassObject](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cogetclassobject) to determine if the COM CLSID is installed. If your get_features needs to do a full initialization sequence to check backend availability, be sure to tear it down before returning. This function should be entirely self-contained and should NOT set class-level fields.
* All feature bits other than IS_SUPPORTED_AT_RUNTIME MUST correspond to actual functionality that the backend implements. If your contribution doesn't do this, it will be rejected without exception.
* The initialize function should do all necessary setup to get the backend up and running. This can be more time-consuming and have much higher computational complexity than get_features. For example, while a COM backend might just check if the CLSID is available, initialize would actually create the COM object.

The synopsis of prism_declare_backend is:

```cmake
function(
  prism_declare_backend
  NAME name
  [LEGACY]
  SOURCE file
  DOC description
  [DEFAULT value]
  [LANGUAGE language]
  [FEATURE definition]
  [PLATFORM platforms]
  [ARCH architectures]
  [PKG_CONFIG modules]
  [DEFINES definitions]
)
```

The arguments are described below.

| Argument | Description |
| --- | --- |
| NAME | Name of the backend. This parameter is required. |
| LEGACY | Marks the backend as a legacy backend. This option should only be used for backends which are dead or have minimal user bases. Legacy backends are only built when PRISM_ENABLE_LEGACY_BACKENDS is enabled. |
| SOURCE | Source file, relative to source/backends, implementing the backend. Required. |
| DOC | Human-readable description of the backend. Used when generating the associated cache option. Required. |
| DEFAULT | Default value for the backend's cache option. If omitted, PRISM_BACKEND_DEFAULT is used. Valid values are AUTO, ON, and OFF. |
| LANGUAGE | Language used to compile the backend source. Currently only OBJCXX has special handling, enabling Objective-C++ compilation with ARC. If your backend is Objective-C/C++, always set this, otherwise your code will fail to compile! |
| FEATURE | Compile definition exported through prism_common when the backend is enabled. |
| PLATFORM | List of supported target platforms. The backend is ignored when configuring for any other platform. |
| ARCH | List of supported architecture classes (for example x86 or x64). The backend is skipped for other architectures. |
| PKG_CONFIG | One or more pkg-config modules required to build the backend. Each module may include a version constraint. |
| DEFINES | Additional compile definitions applied when compiling the backend. You can use these to disable the backend if a feature is not enabled as an example. |

Each backend defines a cache variable named PRISM_ENABLE_<NAME>_BACKEND, where <NAME> is the upper-case backend name. The option accepts the values AUTO, ON, or OFF. AUTO enables the backend only when all build requirements are satisfied; ON requires the backend to be built and reports a configuration error if its requirements are not met; and OFF disables the backend unconditionally.

If DEFAULT is not specified, the cache variable is initialized from PRISM_BACKEND_DEFAULT.

When enabled, prism_declare_backend() creates an object library containing the backend implementation and adds its object files to the prism target. Any libraries discovered through PKG_CONFIG are linked automatically, and any compile definitions specified by FEATURE are exported through prism_common.

A backend is considered buildable only when all of the following conditions are satisfied:

- the current platform matches PLATFORM, if specified;
- the current architecture class matches ARCH, if specified;
- all required pkg-config modules listed in PKG_CONFIG are available; and
- if LEGACY is specified, PRISM_ENABLE_LEGACY_BACKENDS is enabled.

Backends that are disabled by platform compatibility are silently ignored.

## New features/APIs

If your contribution adds a new API (which includes functions, structs, etc.), things can get rather messy depending on the scope of your contribution. This section is split into subsections to make it easier to delineate procedures for varying scopes. It is not a completely absolute list of every possible addition.

### New backend APIs

If all you're doing is extending the existing backend interface, things are quite simple, and the procedure is thusly:

1. Update the C++ TextToSpeechBackend interface, under source/backend.h, to define your new method(s). Under every circumstance your method should be a virtual base method, and under nearly every circumstance it should be implemented just returning std::unexpected(BackendError::NotImplemented). If the method MUST be overridden, open a discussion.
2. Update the Djinni IDL to define the new method on the Java side, unless you want this method not applicable to Android targets. If you choose to not update the Djinni IDL, be prepared to explain why this is not applicable to Android.
3. For each backend that actually implements the functionality you just added to the base class, add that functionality to the respective backends.
4. Update the BackendFeatures namespace with a new feature bit, or use a bit that is reserved. Ensure that all backends you updated in the prior step advertise this new feature bit. If modifications to the Djinni IDL are required, update the IDL to define the new flag.
5. Update the PrismBackendVTable struct by appending the new function to the end. Do NOT reorder fields in this structure!
6. Update the manual with the documentation associated with the new API and feature bit. Follow the existing conventions in the manual.

### Completely new features

If you're adding a totally new feature, things get a bit more complicated. The process, in general, is as follows:

1. Increment PRISM_CONFIG_VERSION and add a new field to PrismConfig, if you determine that this feature requires it. Not all features do require this change.
2. Update PrismPluginServices and PrismPluginHost to expose the new function, if required. If you do need to do this and such would create a new ABI generation, also update PRISM_PLUGIN_ABI_VERSION.
3. Thoroughly implement your new feature to the best of your abilities. Your feature does not need to be bulletproof or perfect. We are happy to assist in iterating on your features development and to wait until you are satisfied with your feature before we merge it.
4. Document the feature in the manual, and update the bindings as appropriate.

### New shims

A shim is a compatibility library to an existing screen reader or TTS API. This means that the top requirement of your shim is that it MUST be ABI-compatible and feature-compatible with the original library. It is allowed to diverge from the original implementation in every possible internal aspect (e.g., bug fixes, better audio library or locks, etc.), but the ABI which is seen by external consumers MUST be identical to the original.

When adding a shim, add a PRISM_ENABLE_XXX_SHIM option and gate it on whether PRISM_ENABLE_SHIMS is set. Your shim should be OS-agnostic (to the best you can achieve it) even if the original library is not; however, this rule may be relaxed depending on the circumstances.

## Code style and formatting

The clang-format tool is the authoritative code formatting source. Always format your code before opening a PR.

Additionally, the following style guidelines, alongside the conventions in the manual, apply:

* In classes, private class members come before public ones. All class fields use snake_case and do not have an m_ prefix or _ suffix.
* Headers are auto-sorted by clang-format. If modifying the delay load failure hook, windows.h MUST be included before delayimp.h, so you will need to modify the file after formatting.
* The Python bindings use Ruff's default formatting rules.
* Shell scripts should be formatted with shfmt, and nix files should be formatted with nixfmt.

## All of this seems so complicated!

Yes, it definitely can be a lot for a newcomer to handle. We apologise for the complexity! However, Prism is trying to do something that is, in effect, completely novel, and so complexity should be expected.

## I have questions!

Feel free to ask in the Discussions tracker! We are always happy to explain decisions or rationales or to help you solve a difficult problem. We are also happy to help you implement a new backend, particularly if you are new to the process. We understand that this document can be extremely overwhelming, so if you ever need help, do not hesitate to ask!