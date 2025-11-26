
#ifndef TEMPOR_PLUGIN_H_
#define TEMPOR_PLUGIN_H_



#include <stdint.h>



typedef enum TprResult {
    TPR_SUCCESS = 0,
    TPR_COUNT_OVERFLOW = -1,
    TPR_UNKNOWN_ERROR = -2,
    TPR_INVALID_VALUE = -3
} TprResult;



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



#ifdef __cplusplus
    #define __noexcept noexcept(true)
#else
    #define __noexcept
#endif



typedef struct TprEngineAPI {

    // log functions, allow to log the same way the engine logs (i. e. to the same log file or to stdout and with the same formatting)
    // plugin should use log functions to log
    void(*log)(TprLogLevel logLevel, const char* message) __noexcept;
    void(*logInfo)(const char* message) __noexcept;
    void(*logWarn)(const char* message) __noexcept;
    void(*logError)(const char* message) __noexcept;
    void(*logDebug)(const char* message) __noexcept;
    void(*logTrace)(const char* message) __noexcept;

    void(*logStyled)(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) __noexcept;
    void(*logInfoStyled)(TprLogStyle logStyle, const char* message) __noexcept;
    void(*logWarnStyled)(TprLogStyle logStyle, const char* message) __noexcept;
    void(*logErrorStyled)(TprLogStyle logStyle, const char* message) __noexcept;
    void(*logDebugStyled)(TprLogStyle logStyle, const char* message) __noexcept;
    void(*logTraceStyled)(TprLogStyle logStyle, const char* message) __noexcept;

    /// @brief declares a new component id.
    /// @details new id is a valid id and can be passed to other functions.
    TprResult(*declareComponent)(uint32_t componentSize, const char* componentName, uint32_t* pNewComponentId) __noexcept;

    TprResult(*acquireComponent)(const char* componentName, uint32_t* pComponentId) __noexcept;

    /// @brief creates an entity.
    /// @param pEntityId pointer to uint32_t where created entity id will be stored.
    /// @param componentIdsCount count of components that created entity must have.
    /// @param pComponentIds array with component ids that created entity must have. All ids must be valid ids created using declareComponentId.
    /// @returns TPR_SUCCESS on success
    TprResult(*createEntity)(uint32_t componentIdCount, const uint32_t* pComponentIds, uint32_t* pEntityId) __noexcept;

    TprResult(*destroyEntity)(uint32_t entityId) __noexcept;

    TprResult(*modifyEntityComponentSet)(uint32_t entityId, uint32_t newComponentIdCount, const uint32_t* pNewComponentIds) __noexcept;

    TprResult(*copyEntityComponentData)(uint32_t entityId, uint32_t componentId, uint32_t start, uint32_t end, char* componentData) __noexcept;
    TprResult(*readEntityComponent8bit)(uint32_t entityId, uint32_t componentId, uint32_t offset, uint8_t* data) __noexcept;
    TprResult(*readEntityComponent16bit)(uint32_t entityId, uint32_t componentId, uint32_t offset, uint16_t* data) __noexcept;
    TprResult(*readEntityComponent32bit)(uint32_t entityId, uint32_t componentId, uint32_t offset, uint32_t* data) __noexcept;
    TprResult(*readEntityComponent64bit)(uint32_t entityId, uint32_t componentId, uint32_t offset, uint64_t* data) __noexcept;

    TprResult(*writeEntityComponentData)(uint32_t entityId, uint32_t componentId, const char* componentData, uint32_t start, uint32_t end) __noexcept;
    TprResult(*writeEntityComponent8bit)(uint32_t entityId, uint32_t componentId, uint8_t data, uint32_t offset) __noexcept;
    TprResult(*writeEntityComponent16bit)(uint32_t entityId, uint32_t componentId, uint16_t data, uint32_t offset) __noexcept;
    TprResult(*writeEntityComponent32bit)(uint32_t entityId, uint32_t componentId, uint32_t data, uint32_t offset) __noexcept;
    TprResult(*writeEntityComponent64bit)(uint32_t entityId, uint32_t componentId, uint64_t data, uint32_t offset) __noexcept;

} TprEngineAPI;



#ifdef __cplusplus
extern "C" {
#endif



/// @brief initializes the plugin. Required.
/// @param ctx a pointer to a pointer to plugin's own data.
/// @returns exit code.
/// @details is called by the engine the first.
/// *ctx should be set to a pointer to plugin's data (e. g. to a struct where all of the data is stored) or NULL if there is none.
/// In case of negative exit code tprHookShutdown will not be called
int32_t tprHookInit(void** ctx, const TprEngineAPI* engineApi) __noexcept;

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
uint32_t tprHookGetNeededHooks(void* ctx, TprHook* hooks) __noexcept;

/// @brief is being called every frame update.
/// @returns exit code
int32_t tprHookUpdatePerFrame(void* ctx) __noexcept;



#ifdef __cplusplus
}
#endif



#endif  // TEMPOR_PLUGIN_H_

