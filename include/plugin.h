
#ifndef TEMPOR_PLUGIN_H_
#define TEMPOR_PLUGIN_H_



typedef enum TprHook {
    TPR_HOOK_INIT = 0,
    TPR_HOOK_SHUTDOWN = 1,
    TPR_HOOK_GET_NEEDED_HOOKS = 2,
    TPR_HOOK_UPDATE_PER_FRAME = 3
} TprHook;



typedef enum TprLogStyle {
    TPR_LOG_STYLE_IDENT = 0,
    TPR_LOG_STYLE_TIMESTAMP1 = 1,
    TPR_LOG_STYLE_ERROR1 = 2,
    TPR_LOG_STYLE_WARN1 = 3,
    TPR_LOG_STYLE_STANDART = 4
} TprLogStyle;


typedef enum TprLogLevel {
    TPR_LOG_LEVEL_QUIET = 0,
    TPR_LOG_LEVEL_INFO = 1,
    TPR_LOG_LEVEL_WARN = 2,
    TPR_LOG_LEVEL_ERROR = 3,
    TPR_LOG_LEVEL_DEBUG = 4,
    TPR_LOG_LEVEL_TRACE = 5
} TprLogLevel;



typedef struct TprEngineAPI {

    void(*log)(TprLogLevel logLevel, const char* message);
    void(*logInfo)(const char* message);
    void(*logWarn)(const char* message);
    void(*logError)(const char* message);
    void(*logDebug)(const char* message);
    void(*logTrace)(const char* message);

    void(*logStyled)(TprLogLevel logLevel, TprLogStyle logStyle, const char* message);
    void(*logInfoStyled)(TprLogStyle logStyle, const char* message);
    void(*logWarnStyled)(TprLogStyle logStyle, const char* message);
    void(*logErrorStyled)(TprLogStyle logStyle, const char* message);
    void(*logDebugStyled)(TprLogStyle logStyle, const char* message);
    void(*logTraceStyled)(TprLogStyle logStyle, const char* message);

} TprEngineAPI;



#ifdef __cplusplus
    #define __noexcept noexcept(true)
#else
    #define __noexcept
#endif



#ifdef __cplusplus
extern "C" {
#endif



/// @brief initializes the plugin. Required.
/// @param ctx a pointer to a pointer to plugin's own data.
/// @returns exit code.
/// @details is called by the engine the first.
/// *ctx should be set to a pointer to plugin's data (e. g. to a struct where all of the data is stored) or NULL if there is none.
/// In case of negative exit code tprHookShutdown will not be called
int tprHookInit(void** ctx, const TprEngineAPI* engineApi) __noexcept;

/// @brief shutdowns the plugin. Required.
/// @details is called by the engine at the end of it's lifecycle or if any previous plugin's hook before returned negative exit code
void tprHookShutdown(void* ctx) __noexcept;

/// @brief returns required by plugin hooks. Required.
/// @param hooks a pointer to an array given by engine.
/// @returns number of hooks.
/// @details is called by the engine twice:
/// 1st time with *hooks set to NULL to obtain the number of hooks;
/// 2nd time with *hooks pointing to an array of size, obtained with the first call.
/// Returned hooks must not have any required hook listed
unsigned int tprHookGetNeededHooks(void* ctx, TprHook* hooks) __noexcept;

/// @brief is being called every frame update.
/// @returns exit code
int tprHookUpdatePerFrame(void* ctx) __noexcept;



#ifdef __cplusplus
}
#endif



#endif  // TEMPOR_PLUGIN_H_

