

#ifndef TEMPOR_PLUGIN_CORE_H_
#define TEMPOR_PLUGIN_CORE_H_


#include <cstdint>



// types

typedef uint8_t TprBool8;
#define TPR_TRUE 1
#define TPR_FALSE 0

typedef uint32_t TprFlag_T;



// enums

typedef enum TprLogStyle {
    TPR_LOG_STYLE_2IDENT = 0,
    TPR_LOG_STYLE_TIMESTAMP1 = 1,
    TPR_LOG_STYLE_ERROR1 = 2,
    TPR_LOG_STYLE_WARN1 = 3,
    TPR_LOG_STYLE_STANDART = 4,
    TPR_LOG_STYLE_SUCCESS1 = 5,
    TPR_LOG_STYLE_ENDSTAMP1 = 6,
    TPR_LOG_STYLE_STARTSTAMP1 = 7,
    TPR_LOG_STYLE_6IDENT = 8,
    TPR_LOG_STYLE_MAX_ENUM = INT32_MAX
} TprLogStyle;

typedef enum TprLogLevel {
    TPR_LOG_LEVEL_QUIET = 0,
    TPR_LOG_LEVEL_INFO = 1,
    TPR_LOG_LEVEL_ERROR = 2,
    TPR_LOG_LEVEL_WARN = 3,
    TPR_LOG_LEVEL_DEBUG = 4,
    TPR_LOG_LEVEL_TRACE = 5,
    TPR_LOG_LEVEL_MAX_ENUM = INT32_MAX
} TprLogLevel;

typedef enum TprResult {
    TPR_SUCCESS = 0,
    TPR_COUNT_OVERFLOW = -1,
    TPR_UNKNOWN_ERROR = -2,
    TPR_INVALID_VALUE = -3,
    TPR_INSUFFICIENT_INIT = -4,
    TPR_BAD_ALLOC = -5,
    TPR_PARSE_ERROR = -6,
    TPR_CONTRACT_VIOLATION = -7,
    TPR_NOT_PERMITTED = -8,
    TPR_RESULT_MAX_ENUM = INT32_MAX
} TprResult;

typedef enum TprDtype {
    TPR_DTYPE_NONE = 0,
    TPR_DTYPE_FLOAT32 = 1,
    TPR_DTYPE_FLOAT64 = 2,
    TPR_DTYPE_INT8 = 3,
    TPR_DTYPE_INT16 = 4,
    TPR_DTYPE_INT32 = 5,
    TPR_DTYPE_INT64 = 6,
    TPR_DTYPE_UINT8 = 7,
    TPR_DTYPE_UINT16 = 8,
    TPR_DTYPE_UINT32 = 9,
    TPR_DTYPE_UINT64 = 10,
    TPR_DTYPE_BOOL8 = 11,
    TPR_DTYPE_BOOL32 = 12,
    TPR_DTYPE_CHAR = 13,
    TPR_DTYPE_MAX_ENUM = INT32_MAX
} TprDtype;

typedef enum TprHook {
    TPR_HOOK_INIT = 0,
    TPR_HOOK_SHUTDOWN = 1,
    TPR_HOOK_GET_NEEDED_HOOKS = 2,
    TPR_HOOK_UPDATE_PER_FRAME = 3,
    TPR_HOOK_RENDER_BEGIN = 4,
    TPR_HOOK_GET_ENTITY_DRAW_ARRAY = 5,
    TPR_HOOL_MAX_ENUM = INT32_MAX
} TprHook;

typedef enum TprAssetType {
    // TPR_ASSET_TYPE_AUTO = 0,
    TPR_ASSET_TYPE_MODEL = 1,
    TPR_ASSET_TYPE_MAX_ENUM = INT32_MAX
} TprAssetType;



// flags

typedef enum TprEntityDrawDescFlagBits {
    TPR_ENTITY_DRAW_DESC_VISIBLE_FLAG_BIT = 0x1,
    TPR_ENTITY_DRAW_DESC_FLAG_BITS_MAX_ENUM = INT32_MAX
} TprEntityDrawFlagBits;
typedef TprFlag_T TprEntityDrawDescFlags;


typedef enum TprCreateWindowFlagBits {
    TPR_CREATE_WINDOW_HIDDEN_FLAG_BIT = 0x1,
    TPR_CREATE_WINDOW_UNRESIZEABLE_FLAG_BIT = 0x2,
    TPR_CREATE_WINDOW_FLAG_BITS_MAX_ENUM = INT32_MAX
} TprCreateWindowFlagBits;
typedef TprFlag_T TprCreateWindowFlags;


typedef enum TprOpenReferenceResourceFlagBits {
    TPR_OPEN_REFERENCE_RESOURCE_DONT_COPY_FLAG_BIT = 0x1,
    TPR_OPER_REFERENCE_RESOURCE_FLAG_BITS_MAX_ENUM = INT32_MAX
} TprOpenReferenceResourceFlagBits;
typedef TprFlag_T TprOpenReferenceResourceFlags;

typedef enum TprOpenEmptyResourceFlagBits {
    TPR_OPEN_EMPTY_RESOURCE_ZEROED_FLAG_BIT = 0x1,
    TPR_OPEN_EMPTY_RESOURCE_FLAG_BITS_MAX_ENUM = INT32_MAX
} TprOpenEmptyResourceFlagBits;
typedef TprFlag_T TprOpenEmptyResourceFlags;

typedef enum TprOpenPathResourceFlagBits {
    TPR_OPEN_PATH_RESOURCE_SYNC_FLAG_BIT = 0x1,
    TPR_OPEN_PATH_RESOURCE_FLAG_BITS_MAX_ENUM = INT32_MAX
} TprOpenPathResourceFlagBits;
typedef TprFlag_T TprOpenPathResourceFlags;

typedef enum TprOpenCapabilityResourceFlagBits {
    TPR_OPEN_CAPABILITY_RESOURCE_FLAG_BITS_MAX_ENUM = INT32_MAX
} TprOpenCapabilityResourceFlagBits;
typedef TprFlag_T TprOpenCapabilityResourceFlags;


typedef enum TprProtectResourceFlagBits {
    TPR_PROTECT_RESOURCE_READ_FLAG_BIT = 0x1,
    TPR_PROTECT_RESOURCE_WRITE_FLAG_BIT = 0x2,
    TPR_PROTECT_RESOURCE_RESIZE_FLAG_BIT = 0x4,
    TPR_PROTECT_RESOURCE_DESTROY_FLAG_BIT = 0x8,
    TPR_PROTECT_RESOURCE_FLAG_BITS_MAX_ENUM = INT32_MAX
} TprProtectResourceFlagBits;
typedef TprFlag_T TprProtectResourceFlags;


typedef enum TprEnumDirFlagBits {
    TPR_ENUM_DIR_DIRS_FLAG_BIT = 0x1,
    TPR_ENUM_DIR_NORMAL_FILES_FLAG_BIT = 0x2,
    TPR_ENUM_DIR_RUNTIME_LIBS_FLAG_BIT = 0x4,
    TPR_ENUM_DIR_EXECUTABLES_FLAG_BIT = 0x8,
    TPR_ENUM_DIR_FLAG_BITS_MAX_ENUM = INT32_MAX
} TprEnumDirFlagBits;
typedef TprFlag_T TprEnumDirFlags;



// opaque handles

typedef struct TprOArrayEntityDrawDesc_T* TprOArrayEntityDrawDesc;

typedef struct TprWindow { uint64_t _d; } TprWindow;
typedef struct TprResource { uint64_t _d; } TprResource;
typedef struct TprAsset { uint64_t _d; } TprAsset;



// handles

typedef struct TprEntityHandle {
    uint32_t id;
    uint32_t gen;
} TprEntity;

typedef struct TprComponent {
    uint32_t id;
} TprComponent;



// data structs

typedef struct TprEntityDrawDesc {
    TprEntityDrawDescFlags flags;
    TprEntity entityHandle;
} TprEntityDrawDesc;

typedef struct TprLifetime {
    uint64_t frames;
} TprLifetime;

typedef struct TprAssetSlot {

} TprAssetSlot;



// create infos

typedef struct TprMaterialDeclareInfo {
    uint32_t dtypeCount;
    TprDtype dtypes;
} TprTemplateDeclareRegisterInfo;

typedef struct TprWindowCreateInfo {
    TprCreateWindowFlags flags;
    const char* name;
    int32_t prefferedWidth;
    int32_t prefferedHeight;
} TprWindowCreateInfo;

typedef struct TprAssetParseInfo {
    TprResource resource;
    TprAssetType type;
    uint32_t index;
} TprAssetParseInfo;

typedef struct TprAssetLoadInfo {
    const TprAssetSlot* pSlots;
    TprResource data;
} TprAssetLoadInfo;

typedef struct TprContextCreateInfo {

} TprContextCreateInfo;



#endif  // TEMPOR_PLUGIN_CORE_H_
