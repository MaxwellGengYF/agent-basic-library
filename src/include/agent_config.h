#pragma once

/* Shared build-configuration header for the agent-basic-library native API.
 *
 * Provides per-module DLL export/import macros modeled after
 * LuisaCompute's luisa/core/dll_export.h. When building a feature as a
 * shared library, define the corresponding AGENT_<MODULE>_EXPORT_DLL macro
 * so that the public symbols are exported. Consumers of the shared library
 * should not define that macro, causing symbols to be imported automatically.
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

/* Per-feature API macros. These intentionally do NOT include extern "C"
 * so they can be used inside existing extern "C" blocks in pure-C headers.
 * Define AGENT_<MODULE>_EXPORT_DLL when compiling the corresponding DLL. */

#ifdef AGENT_MESSAGE_SANITIZATION_EXPORT_DLL
#define AGENT_MESSAGE_SANITIZATION_API AGENT_DECLSPEC_DLL_EXPORT
#else
#define AGENT_MESSAGE_SANITIZATION_API AGENT_DECLSPEC_DLL_IMPORT
#endif

#ifdef AGENT_PROMPT_BUILDER_EXPORT_DLL
#define AGENT_PROMPT_BUILDER_API AGENT_DECLSPEC_DLL_EXPORT
#else
#define AGENT_PROMPT_BUILDER_API AGENT_DECLSPEC_DLL_IMPORT
#endif

#ifdef AGENT_CONVERSATION_LOOP_EXPORT_DLL
#define AGENT_CONVERSATION_LOOP_API AGENT_DECLSPEC_DLL_EXPORT
#else
#define AGENT_CONVERSATION_LOOP_API AGENT_DECLSPEC_DLL_IMPORT
#endif

#ifdef AGENT_CONTEXT_COMPRESSOR_EXPORT_DLL
#define AGENT_CONTEXT_COMPRESSOR_API AGENT_DECLSPEC_DLL_EXPORT
#else
#define AGENT_CONTEXT_COMPRESSOR_API AGENT_DECLSPEC_DLL_IMPORT
#endif
