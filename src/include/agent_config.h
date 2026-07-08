#pragma once

/* Shared build-configuration header for the agent-basic-library native API.
 *
 * Provides DLL export/import macros modeled after LuisaCompute's
 * luisa/core/dll_export.h. The project now builds a single agent_core
 * shared library that carries every public symbol; define
 * AGENT_CORE_EXPORT_DLL when compiling that library so all APIs are
 * exported. Consumers of the shared library should not define that macro,
 * causing symbols to be imported automatically.
 */

#ifdef __cplusplus
#define AGENT_EXTERN_C extern "C"
#else
#define AGENT_EXTERN_C
#endif

#ifdef _WIN32
#define AGENT_DECLSPEC_DLL_EXPORT __declspec(dllexport)
#define AGENT_DECLSPEC_DLL_IMPORT __declspec(dllimport)
#else
#define AGENT_DECLSPEC_DLL_EXPORT __attribute__((visibility("default")))
#define AGENT_DECLSPEC_DLL_IMPORT
#endif

/* C-only convenience macros that also include extern "C" linkage. */
#define AGENT_EXPORT_API AGENT_EXTERN_C AGENT_DECLSPEC_DLL_EXPORT
#define AGENT_IMPORT_API AGENT_EXTERN_C AGENT_DECLSPEC_DLL_IMPORT

/* Unified DLL export control for the single agent_core shared library.
 * All per-feature API macros are governed by AGENT_CORE_EXPORT_DLL so a
 * single build-time define exports every public symbol. */

#ifdef AGENT_CORE_EXPORT_DLL
#define AGENT_CORE_API AGENT_EXTERN_C AGENT_DECLSPEC_DLL_EXPORT
#define AGENT_MESSAGE_SANITIZATION_API AGENT_DECLSPEC_DLL_EXPORT
#define AGENT_PROMPT_BUILDER_API AGENT_DECLSPEC_DLL_EXPORT
#define AGENT_CONVERSATION_LOOP_API AGENT_DECLSPEC_DLL_EXPORT
#define AGENT_CONTEXT_COMPRESSOR_API AGENT_DECLSPEC_DLL_EXPORT
#define AGENT_TEXT_OPS_API AGENT_DECLSPEC_DLL_EXPORT
#else
#define AGENT_CORE_API AGENT_EXTERN_C AGENT_DECLSPEC_DLL_IMPORT
#define AGENT_MESSAGE_SANITIZATION_API AGENT_DECLSPEC_DLL_IMPORT
#define AGENT_PROMPT_BUILDER_API AGENT_DECLSPEC_DLL_IMPORT
#define AGENT_CONVERSATION_LOOP_API AGENT_DECLSPEC_DLL_IMPORT
#define AGENT_CONTEXT_COMPRESSOR_API AGENT_DECLSPEC_DLL_IMPORT
#define AGENT_TEXT_OPS_API AGENT_DECLSPEC_DLL_IMPORT
#endif
