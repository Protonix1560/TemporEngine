
#ifndef TEMPOR_PLUGIN_CORE_H_
#define TEMPOR_PLUGIN_CORE_H_

#include <cstdint>


// defs

#if defined(__cplusplus) && __cplusplus >= 201703L
    #define _TPR_NOEXCEPT_ATTR noexcept
#else
    #define _TPR_NOEXCEPT_ATTR
#endif

#if defined(__cplusplus)
    #define _TPR_NOEXCEPT noexcept
#else
    #define _TPR_NOEXCEPT
#endif

typedef uint32_t _TprFlag_T;
#define _TPR_MAX_ENUM INT32_MAX

typedef uint8_t TprBool8;
#define TPR_TRUE 1
#define TPR_FALSE 0


// enums

typedef enum TprLogStyle {
    TPR_LOG_STYLE_NORMAL = 0,
    TPR_LOG_STYLE_TIMESTAMP1 = 1,
    TPR_LOG_STYLE_ERROR1 = 2,
    TPR_LOG_STYLE_WARN1 = 3,
    TPR_LOG_STYLE_2IDENT = 4,
    TPR_LOG_STYLE_SUCCESS1 = 5,
    TPR_LOG_STYLE_ENDSTAMP1 = 6,
    TPR_LOG_STYLE_STARTSTAMP1 = 7,
    TPR_LOG_STYLE_6IDENT = 8,
    TPR_LOG_STYLE_PANIC1 = 9,
    _TPR_LOG_STYLE_MAX_ENUM = _TPR_MAX_ENUM
} TprLogStyle;

typedef enum TprLogLevel {
    TPR_LOG_LEVEL_PANIC = 1,
    TPR_LOG_LEVEL_ERROR = 2,
    TPR_LOG_LEVEL_WARN = 3,
    TPR_LOG_LEVEL_INFO = 4,
    TPR_LOG_LEVEL_DEBUG = 5,
    TPR_LOG_LEVEL_TRACE = 6,
    _TPR_LOG_LEVEL_MAX_ENUM = _TPR_MAX_ENUM
} TprLogLevel;

typedef enum TprResult {
    TPR_SUCCESS = 0,
    TPR_PANIC = -1,

    TPR_ERROR_NOT_LOADED = -2,
    TPR_ERROR_INVALID_VALUE = -3,
    TPR_ERROR_INVALID_OPERATION = -4,
    TPR_ERROR_DOESNT_EXIST = -5,
    TPR_ERROR_NOT_PERMITTED = -6,
    TPR_ERROR_WRONG_TYPE = -7,
    TPR_ERROR_OUT_OF_RANGE = -8,
    TPR_ERROR_OUT_OF_MEMORY = -9,

    TPR_COUNT_OVERFLOW = -10,
    TPR_UNKNOWN_ERROR = -11,
    TPR_INSUFFICIENT_INIT = -12,
    TPR_BAD_ALLOC = -13,
    TPR_PARSE_ERROR = -14,
    TPR_USER_CODE_ERROR = -15,
    TPR_NOT_SUPPORTED = -16,
    TPR_ALREADY_EXISTS = -17,
    TPR_VERSION_MISMATCH = -18,

    _TPR_RESULT_MAX_ENUM = _TPR_MAX_ENUM
} TprResult;

typedef enum TprInputDevice {
    TPR_KEY_A = 1,
    TPR_KEY_B = 2,
    TPR_KEY_C = 3,
    TPR_KEY_D = 4,
    TPR_KEY_E = 5,
    TPR_KEY_F = 6,
    TPR_KEY_G = 7,
    TPR_KEY_H = 8,
    TPR_KEY_I = 9,
    TPR_KEY_J = 10,
    TPR_KEY_K = 11,
    TPR_KEY_L = 12,
    TPR_KEY_M = 13,
    TPR_KEY_N = 14,
    TPR_KEY_O = 15,
    TPR_KEY_P = 16,
    TPR_KEY_Q = 17,
    TPR_KEY_R = 18,
    TPR_KEY_S = 19,
    TPR_KEY_T = 20,
    TPR_KEY_U = 21,
    TPR_KEY_V = 22,
    TPR_KEY_W = 23,
    TPR_KEY_X = 24,
    TPR_KEY_Y = 25,
    TPR_KEY_Z = 26,
    TPR_KEY_1 = 27,
    TPR_KEY_2 = 28,
    TPR_KEY_3 = 29,
    TPR_KEY_4 = 30,
    TPR_KEY_5 = 31,
    TPR_KEY_6 = 32,
    TPR_KEY_7 = 33,
    TPR_KEY_8 = 34,
    TPR_KEY_9 = 35,
    TPR_KEY_0 = 36,
    TPR_KEY_MINUS = 37,
    TPR_KEY_EQUAL = 38,
    TPR_KEY_BACKSLASH = 39,
    TPR_KEY_SLASH = 40,
    TPR_KEY_BRACKET_LEFT = 41,
    TPR_KEY_BRACKET_RIGHT = 42,
    TPR_KEY_SEMICOLON = 43,
    TPR_KEY_QUOTE = 44,
    TPR_KEY_SPACE = 45,
    TPR_KEY_LEFT_CTRL = 46,
    TPR_KEY_LEFT_SHIFT = 47,
    TPR_KEY_LEFT_SUPER = 48,
    TPR_KEY_LEFT_ALT = 49,
    TPR_KEY_TAB = 50,
    TPR_KEY_CAPS_LOCK = 51,
    TPR_KEY_TILDE = 52,
    TPR_KEY_ESCAPE = 53,
    TPR_KEY_F1 = 54,
    TPR_KEY_F2 = 55,
    TPR_KEY_F3 = 56,
    TPR_KEY_F4 = 57,
    TPR_KEY_F5 = 58,
    TPR_KEY_F6 = 59,
    TPR_KEY_F7 = 60,
    TPR_KEY_F8 = 61,
    TPR_KEY_F9 = 62,
    TPR_KEY_F10 = 63,
    TPR_KEY_F11 = 64,
    TPR_KEY_F12 = 65,
    TPR_KEY_RIGHT_CTRL = 66,
    TPR_KEY_RIGHT_SHIFT = 67,
    TPR_KEY_RIGHT_SUPER = 68,
    TPR_KEY_RIGHT_ALT = 69,
    TPR_KEY_ENTER = 70,
    TPR_KEY_BACKSPACE = 71,
    TPR_KEY_PRINT_SCREEN = 72,
    TPR_KEY_SCROLL_LOCK = 73,
    TPR_KEY_PAUSE_BREAK = 74,
    TPR_KEY_INSERT = 75,
    TPR_KEY_DELETE = 76,
    TPR_KEY_END = 77,
    TPR_KEY_HOME = 78,
    TPR_KEY_PAGE_UP = 79,
    TPR_KEY_PAGE_DOWN = 80,
    TPR_KEY_ARROW_UP = 81,
    TPR_KEY_ARROW_DOWN = 82,
    TPR_KEY_ARROW_LEFT = 83,
    TPR_KEY_ARROW_RIGHT = 84,
    TPR_KEY_NUM_LOCK = 85,
    TPR_KEY_NUMPAD_SLASH = 86,
    TPR_KEY_NUMPAD_STAR = 87,
    TPR_KEY_NUMPAD_MINUS = 88,
    TPR_KEY_NUMPAD_PLUS = 89,
    TPR_KEY_NUMPAD_EQUAL = 90,
    TPR_KEY_NUMPAD_ENTER = 91,
    TPR_KEY_NUMPAD_DOT = 92,
    TPR_KEY_NUMPAD_0 = 93,
    TPR_KEY_NUMPAD_1 = 94,
    TPR_KEY_NUMPAD_2 = 95,
    TPR_KEY_NUMPAD_3 = 96,
    TPR_KEY_NUMPAD_4 = 97,
    TPR_KEY_NUMPAD_5 = 98,
    TPR_KEY_NUMPAD_6 = 99,
    TPR_KEY_NUMPAD_7 = 100,
    TPR_KEY_NUMPAD_8 = 101,
    TPR_KEY_NUMPAD_9 = 102,
    TPR_KEY_COMMA = 103,
    TPR_KEY_DOT = 104,

    TPR_MOUSE_BUTTON1 = 1001,
    TPR_MOUSE_BUTTON2 = 1002,
    TPR_MOUSE_BUTTON3 = 1003,
    TPR_MOUSE_BUTTON4 = 1004,
    TPR_MOUSE_BUTTON5 = 1005,

    TPR_MOUSE_WHEEL = 2001,

    TPR_MOUSE_MOTION = 3001,

    TPR_WINDOW_SIZE = 4001,

    _TPR_INPUT_DEVICE_MAX_ENUM = _TPR_MAX_ENUM
} TprInputDevice;

typedef enum TprSettingType {
    TPR_SETTING_TYPE_UNSET = 0,
    TPR_SETTING_TYPE_STRING = 1,
    TPR_SETTING_TYPE_INTEGER = 2,
    TPR_SETTING_TYPE_DOUBLE = 3,
    TPR_SETTING_TYPE_BOOL = 4,
    TPR_SETTING_TYPE_NULL = 5,
    TPR_SETTING_TYPE_ARRAY = 6,
    TPR_SETTING_TYPE_STRUCT = 7,
    _TPR_SETTING_TYPE_MAX_ENUM = _TPR_MAX_ENUM
} TprSettingType;

typedef enum TprSeekWhence {
    TPR_SEEK_WHENCE_BEGIN = 0,
    TPR_SEEK_WHENCE_CURRENT = 1,
    TPR_SEEK_WHENCE_END = 2,
    _TPR_SEEK_WHENCE_MAX_ENUM = _TPR_MAX_ENUM
} TprSeekWhence;

typedef enum TprPathType {
    TPR_PATH_TYPE_FILE = 0,
    TPR_PATH_TYPE_DIRECTORY = 1,
    _TPR_PATH_TYPE_MAX_ENUM = _TPR_MAX_ENUM
} TprPathType;

typedef enum TprJobDuration {
    TPR_JOB_DURATION_LONG = 0,
    TPR_JOB_DURATION_SHORT = 1,
    _TPR_JOB_DURATION_MAX_ENUM = _TPR_MAX_ENUM
} TprJobDuration;

typedef enum TprJobTriggerType {
    TPR_JOB_TRIGGER_TYPE_SCHEDULE = 0,
    TPR_JOB_TRIGGER_TYPE_DEPENDENCIES = 1,
    _TPR_JOB_TRIGGER_TYPE_MAX_ENUM = _TPR_MAX_ENUM
} TprJobTriggerType;

typedef enum TprActionMeasureType {
    TPR_MEASURE_TYPE_ABSOLUTE = 0,
    TPR_MEASURE_TYPE_DIFFERENCE = 1,
    TPR_MEASURE_TYPE_DERIVATIVE = 2,
    _TPR_MEASURE_TYPE_MAX_ENUM = _TPR_MAX_ENUM
} TprActionMeasureType;


// flags

typedef enum TprCreateWindowFlagBits {
    TPR_CREATE_WINDOW_HIDDEN_FLAG_BIT = 0x1,
    TPR_CREATE_WINDOW_UNRESIZEABLE_FLAG_BIT = 0x2,
    _TPR_CREATE_WINDOW_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateWindowFlagBits;
typedef _TprFlag_T TprCreateWindowFlags;

typedef enum TprWindowCapabilityFlagBits {
    _TPR_WINDOW_CAPABILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprWindowCapabilityFlagBits;
typedef _TprFlag_T TprWindowCapabilityFlags;


typedef enum TprCreateActionFlagBits {
    _TPR_CREATE_ACTION_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateActionFlagBits;
typedef _TprFlag_T TprCreateActionFlags;

typedef enum TprActionCapabilityFlagBits {
    _TPR_ACTION_CAPABILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprActionCapabilityFlagBits;
typedef _TprFlag_T TprActionCapabilityFlags;


typedef enum TprCreateDepthDomainFlagBits {
    TPR_CREATE_DEPTH_DOMAIN_BEFORE_ANCHOR_FLAG_BIT = 0x1,
    _TPR_CREATE_DEPTH_DOMAIN_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateDepthDomainFlagBits;
typedef _TprFlag_T TprCreateDepthDomainFlags;

typedef enum TprDepthDomainCapabilityFlagBits {
    _TPR_DEPTH_DOMAIN_CAPABILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprDepthDomainCapabilityFlagBits;
typedef _TprFlag_T TprDepthDomainCapabilityFlags;


typedef enum TprCreateMeshFlagBits {
    _TPR_CREATE_MESH_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateMeshFlagBits;
typedef _TprFlag_T TprCreateMeshFlags;

typedef enum TprMeshCapabilityFlagBits {
    _TPR_MESH_CAPABILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprMeshCapabilityFlagBits;
typedef _TprFlag_T TprMeshCapabilityFlags;


typedef enum TprCreateRenderTargetFlagBits {
    _TPR_CREATE_RENDER_TARGET_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateRenderTargetFlagBits;
typedef _TprFlag_T TprCreateRenderTargetFlags;

typedef enum TprRenderTargetCapabilityFlagBits {
    _TPR_RENDER_TARGET_CAPAIBILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprRenderTargetCapabilityFlagBits;
typedef _TprFlag_T TprRenderTargetCapabilityFlags;


typedef enum TprCreateRenderTargetSetFlagBits {
    _TPR_CREATE_RENDER_TARGET_SET_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateRenderTargetSetFlagBits;
typedef _TprFlag_T TprCreateRenderTargetSetFlags;

typedef enum TprRenderTargetSetCapabilityFlagBits {
    _TPR_RENDER_TARGET_SET_CAPAIBILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprRenderTargetSetCapabilityFlagBits;
typedef _TprFlag_T TprRenderTargetSetCapabilityFlags;


typedef enum TprCreateEntityImageFlagBits {
    _TPR_CREATE_OBJECT_IMAGE_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateEntityImageFlagBits;
typedef _TprFlag_T TprCreateEntityImageFlags;

typedef enum TprEntityImageCapabilityFlagBits {
    _TPR_ENTITY_IMAGE_CAPABILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprEntityImageCapabilityFlagBits;
typedef _TprFlag_T TprEntityImageCapabilityFlags;


// typedef enum TprCreateSettingFlagBits {
//     _TPR_CREATE_SETTING_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
// } TprCreateSettingFlagBits;
// typedef _TprFlag_T TprCreateSettingFlags;

typedef enum TprSettingCapabilityFlagBits {
    TPR_SETTING_CAPABILITY_MODIFY_FLAG_BIT = 0x1,
    _TPR_SETTING_CAPABILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprSettingCapabilityFlagBits;
typedef _TprFlag_T TprSettingCapabilityFlags;


typedef enum TprOpenFileFlagBits {
    TPR_OPEN_FILE_SYNC_FLAG_BIT = 0x1,
    TPR_OPEN_FILE_ALWAYS_NEW_FLAG_BIT = 0x2,
    TPR_OPEN_FILE_NEW_IF_NONE_FLAG_BIT = 0x4,
    _TPR_OPEN_FILE_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprOpenFileFlagBits;
typedef _TprFlag_T TprOpenFileFlags;

typedef enum TprFileCapabilityFlagBits {
    TPR_FILE_CAPABILITY_WRITE_FLAG_BIT = 0x1,
    _TPR_FILE_CAPABILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprFileCapabilityFlagBits;
typedef _TprFlag_T TprFileCapabilityFlags;

typedef enum TprTouchFileFlagBits {
    // TPR_CREATE_NEW_FILE_VFS_FLAG_BIT = 0x1,  // TODO: add proper VFS
    _TPR_CREATE_NEW_FILE_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprTouchFileFlagBits;
typedef _TprFlag_T TprTouchFileFlags;

typedef enum TprCreateDirectoryFlagBits {
    // TPR_CREATE_DIRECTORY_VFS_FLAG_BIT = 0x1,  // TODO: add proper VFS
    _TPR_CREATE_DIRECTORY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateDirectoryFlagBits;
typedef _TprFlag_T TprCreateDirectoryFlags;


typedef enum TprCreateJobFlagBits {
    _TPR_CREATE_JOB_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateJobFlagBits;
typedef _TprFlag_T TprCreateJobFlags;

typedef enum TprJobCapabilityFlagBits {
    _TPR_JOB_CAPABILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprJobCapabilityFlagBits;
typedef _TprFlag_T TprJobCapabilityFlags;


// handles

typedef struct TprWindow { uint64_t _d; } TprWindow;
typedef struct TprFile { uint64_t _d; } TprFile;
typedef struct TprMesh { uint64_t _d; } TprMesh;
typedef struct TprAction { uint64_t _d; } TprAction;
typedef struct TprComponent { uint64_t _d; } TprComponent;
typedef struct TprSetting { uint64_t _d; } TprSetting;
typedef struct TprDepthDomain { uint64_t _d; } TprDepthDomain;
typedef struct TprRenderTarget {  uint64_t _d; } TprRenderTarget;
typedef struct TprRenderTargetSet {  uint64_t _d; } TprRenderTargetSet;
typedef struct TprEntityImage { uint64_t _d; } TprEntityImage;
typedef struct TprComponentChunk { uint64_t _d; } TprComponentChunk;
typedef struct TprJob { uint64_t _d; } TprJob;


// data structs

typedef struct TprVec4 {
    float x;
    float y;
    float z;
    float w;
} TprVec4;

typedef struct TprMat4x4 {
    float x0, y0, z0, w0;
    float x1, y1, z1, w1;
    float x2, y2, z2, w2;
    float x3, y3, z3, w3;
} TprMat4x4;

typedef struct TprEntity {
    uint32_t id; 
} TprEntity;

typedef struct TprActionState {
    TprVec4 vector;
    uint64_t timepoint;
} TprActionState;

typedef struct TprActionHistoryEntry {
    TprAction action;
    TprActionState state;
} TprActionHistoryEntry;

typedef struct TprScissor {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} TprScissor;

typedef struct TprViewport {
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
} TprViewport;

typedef struct TprComponentRenderable {
    TprMat4x4 transform;
    TprEntityImage entityImage;
    TprRenderTargetSet renderTargetSet;
} TprComponentRenderable;


// create infos

typedef struct TprWindowCreateInfo {
    TprCreateWindowFlags flags;
    const char* name;
    uint32_t width;
    uint32_t height;
} TprWindowCreateInfo;

typedef struct TprActionCreateInfo {
    TprCreateActionFlags flags;
    TprInputDevice device;
    TprActionMeasureType measureType;
    TprWindow window;
} TprActionCreateInfo;

typedef struct TprMeshCreateInfo {
    TprCreateMeshFlags flags;
    TprFile data;
    uint32_t index;
} TprMeshCreateInfo;

typedef struct TprDepthDomainCreateInfo {
    TprCreateDepthDomainFlags flags;
    TprDepthDomain anchor;
} TprDepthDomainCreateInfo;

typedef struct TprRenderTargetCreateInfo {
    TprCreateRenderTargetFlags flags;
    TprWindow window;
    TprDepthDomain depthDomain;
    TprViewport viewport;
    TprScissor scissor;
} TprRenderTargetCreateInfo;

typedef struct TprRenderTargetSetCreateInfo {
    TprCreateRenderTargetSetFlags flags;
    uint32_t targetCount;
    TprRenderTarget* pTargets;
} TprRenderTargetSetCreateInfo;

typedef struct TprEntityImageCreateInfo {
    TprCreateEntityImageFlags flags;
    TprMesh mesh;
} TprEntityImageCreateInfo;

typedef struct TprJobCreateInfo {
    TprCreateJobFlags flags;
    TprJobDuration duration;
    void* context;
    void(*function)(void* ctx, TprJob job);
    TprJobTriggerType triggerType;
    uint32_t dependencyCount;
    const TprJob* pDependencies;
} TprJobCreateInfo;


#endif  // TEMPOR_PLUGIN_CORE_H_
