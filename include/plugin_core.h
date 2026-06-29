
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

typedef uint8_t TprBool8;
#define TPR_TRUE 1
#define TPR_FALSE 0
typedef uint32_t TprFlag_T;
#define _TPR_MAX_ENUM INT32_MAX


// enums

typedef enum TprLogStyle {
    TPR_LOG_STYLE_STANDART = 0,
    TPR_LOG_STYLE_TIMESTAMP1 = 1,
    TPR_LOG_STYLE_ERROR1 = 2,
    TPR_LOG_STYLE_WARN1 = 3,
    TPR_LOG_STYLE_2IDENT = 4,
    TPR_LOG_STYLE_SUCCESS1 = 5,
    TPR_LOG_STYLE_ENDSTAMP1 = 6,
    TPR_LOG_STYLE_STARTSTAMP1 = 7,
    TPR_LOG_STYLE_6IDENT = 8,
    TPR_LOG_STYLE_PANIC1 = 9,
    TPR_LOG_STYLE_MAX_ENUM = _TPR_MAX_ENUM
} TprLogStyle;

typedef enum TprLogLevel {
    TPR_LOG_LEVEL_PANIC = 1,
    TPR_LOG_LEVEL_ERROR = 2,
    TPR_LOG_LEVEL_WARN = 3,
    TPR_LOG_LEVEL_INFO = 4,
    TPR_LOG_LEVEL_DEBUG = 5,
    TPR_LOG_LEVEL_TRACE = 6,
    TPR_LOG_LEVEL_MAX_ENUM = _TPR_MAX_ENUM
} TprLogLevel;

typedef enum TprResult {
    TPR_SUCCESS = 0,
    TPR_PANIC = -1,

    TPR_MODULE_NOT_LOADED = 1,

    TPR_ERROR_INVALID_VALUE = -2,
    TPR_ERROR_INVALID_OPERATION = -3,
    TPR_ERROR_DOESNT_EXIST = -4,
    TPR_ERROR_NOT_PERMITTED = -5,
    TPR_ERROR_WRONG_TYPE = -6,
    TPR_ERROR_OUT_OF_RANGE = -7,
    TPR_ERROR_OUT_OF_MEMORY = -8,

    TPR_COUNT_OVERFLOW = -8,
    TPR_UNKNOWN_ERROR = -9,
    TPR_INSUFFICIENT_INIT = -10,
    TPR_BAD_ALLOC = -11,
    TPR_PARSE_ERROR = -12,
    TPR_USER_CODE_ERROR = -13,
    TPR_NOT_SUPPORTED = -14,
    TPR_NOT_A_FILE = -15,
    TPR_NO_SUCH_FILE = -16,
    TPR_ALREADY_EXISTS = -17,
    TPR_VERSION_MISMATCH = -18,
    TPR_RESULT_MAX_ENUM = _TPR_MAX_ENUM
} TprResult;

typedef enum TprInputElement {
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

    TPR_MOUSE_WHEEL_UP = 10001,
    TPR_MOUSE_WHEEL_DOWN = 10002,

    TPR_MOUSE_MOTION = 100000,

    TPR_KEY_MAX_ENUM = _TPR_MAX_ENUM
} TprInputElement;

typedef enum TprSettingType {
    TPR_SETTING_TYPE_UNSET = 0,
    TPR_SETTING_TYPE_STRING = 1,
    TPR_SETTING_TYPE_INTEGER = 2,
    TPR_SETTING_TYPE_DOUBLE = 3,
    TPR_SETTING_TYPE_BOOL = 4,
    TPR_SETTING_TYPE_NULL = 5,
    TPR_SETTING_TYPE_ARRAY = 6,
    TPR_SETTING_TYPE_STRUCT = 7,
    TPR_SETTING_MAX_ENUM = _TPR_MAX_ENUM
} TprSettingType;

typedef enum TprJobType {
    TPR_JOB_TYPE_SHORT_TERM = 0,
    TPR_JOB_TYPE_LONG_TERM = 1,
    TPR_JOB_TYPE_MAX_ENUM = _TPR_MAX_ENUM
} TprJobType;

typedef enum TprSeekWhence {
    TPR_SEEK_WHENCE_BEGIN = 0,
    TPR_SEEK_WHENCE_CURRENT = 1,
    TPR_SEEK_WHENCE_END = 2,
    TPR_SEEK_WHENCE_MAX_ENUM = _TPR_MAX_ENUM
} TprSeekWhence;

typedef enum TprPathType {
    TPR_PATH_TYPE_FILE = 0,
    TPR_PATH_TYPE_DIRECTORY = 1,
    TPR_PATH_TYPE_MAX_ENUM = _TPR_MAX_ENUM
} TprPathType;


// flags

typedef enum TprCreateWindowFlagBits {
    TPR_CREATE_WINDOW_HIDDEN_FLAG_BIT = 0x1,
    TPR_CREATE_WINDOW_UNRESIZEABLE_FLAG_BIT = 0x2,
    TPR_CREATE_WINDOW_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateWindowFlagBits;
typedef TprFlag_T TprCreateWindowFlags;

typedef enum TprCreateActionFlagBits {
    TPR_CREATE_ACTION_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateActionFlagBits;
typedef TprFlag_T TprCreateActionFlags;

typedef enum TprCreateDepthDomainFlagBits {
    TPR_CREATE_DEPTH_DOMAIN_BEFORE_ANCHOR_FLAG_BIT = 0x1,
    TPR_CREATE_DEPTH_DOMAIN_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateDepthDomainFlagBits;
typedef TprFlag_T TprCreateDepthDomainFlags;

typedef enum TprCreateJobFlagBits {
    TPR_CREATE_JOB_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateJobFlagBits;
typedef TprFlag_T TprCreateJobFlags;

typedef enum TprCreateMeshFlagBits {
    TPR_CREATE_MESH_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateMeshFlagBits;
typedef TprFlag_T TprCreateMeshFlags;

typedef enum TprLoadMeshFlagBits {
    TPR_LOAD_MESH_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprLoadMeshFlagBits;
typedef TprFlag_T TprLoadMeshFlags;

typedef enum TprCreateRenderTargetFlagBits {
    TPR_CREATE_RENDER_TARGET_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateRenderTargetFlagBits;
typedef TprFlag_T TprCreateRenderTargetFlags;

typedef enum TprCreateObjectImageFlagBits {
    TPR_CREATE_OBJECT_IMAGE_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateObjectImageFlagBits;
typedef TprFlag_T TprCreateObjectImageFlags;

typedef enum TprSettingCapabilityFlagBits {
    TPR_SETTING_CAPABILITY_MODIFY_FLAG_BIT = 0x1,
    TPR_SETTING_CAPABILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprSettingCapabilityFlagBits;
typedef TprFlag_T TprSettingCapabilityFlags;

typedef enum TprOpenFileFlagBits {
    TPR_OPEN_FILE_SYNC_FLAG_BIT = 0x1,
    TPR_OPEN_FILE_ALWAYS_NEW_FLAG_BIT = 0x2,
    TPR_OPEN_FILE_NEW_IF_NONE_FLAG_BIT = 0x4,
    TPR_OPEN_FILE_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprOpenFileFlagBits;
typedef TprFlag_T TprOpenFileFlags;

typedef enum TprTouchFileFlagBits {
    // TPR_CREATE_NEW_FILE_VFS_FLAG_BIT = 0x1,  // TODO: add proper VFS
    TPR_CREATE_NEW_FILE_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprTouchFileFlagBits;
typedef TprFlag_T TprTouchFileFlags;

typedef enum TprCreateDirectoryFlagBits {
    // TPR_CREATE_DIRECTORY_VFS_FLAG_BIT = 0x1,  // TODO: add proper VFS
    TPR_CREATE_DIRECTORY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprCreateDirectoryFlagBits;
typedef TprFlag_T TprCreateDirectoryFlags;

typedef enum TprFileCapabilityFlagBits {
    TPR_FILE_CAPABILITY_WRITE_FLAG_BIT = 0x1,
    TPR_FILE_CAPABILITY_FLAG_BITS_MAX_ENUM = _TPR_MAX_ENUM
} TprFileCapabilityFlagBits;
typedef TprFlag_T TprFileCapabilityFlags;


// handles

typedef struct TprWindow { uint64_t _d; } TprWindow;
typedef struct TprFile { uint64_t _d; } TprFile;
typedef struct TprMesh { uint64_t _d; } TprMesh;
typedef struct TprAction { uint64_t _d; } TprAction;
typedef struct TprComponent { uint64_t _d; } TprComponent;
typedef struct TprSetting { uint64_t _d; } TprSetting;
typedef struct TprDepthDomain { uint64_t _d; } TprDepthDomain;
typedef struct TprRenderTarget {  uint64_t _d; } TprRenderTarget;
typedef struct TprObjectImage { uint64_t _d; } TprObjectImage;
typedef struct TprComponentChunk { uint64_t _d; } TprComponentChunk;
typedef struct TprJob { uint64_t _d; } TprJob;


// data structs

typedef struct TprEntity {
    uint32_t id; 
} TprEntity;

typedef struct TprInputElementVector {
    float x;
    float y;
    float z;
} TprInputElementVector;

typedef struct TprActionState {
    TprInputElementVector vector;
    TprBool8 state;
    uint32_t framesActive;
} TprActionState;

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

typedef struct TprMat4x4 {
    float x0, y0, z0, w0;
    float x1, y1, z1, w1;
    float x2, y2, z2, w2;
    float x3, y3, z3, w3;
} TprMat4x4;

typedef struct TprComponentRenderable {
    TprObjectImage image;
    TprMat4x4 transform;
} TprComponentRenderable;


// create infos

typedef struct TprWindowCreateInfo {
    TprCreateWindowFlags flags;
    const char* name;
    int32_t prefferedWidth;
    int32_t prefferedHeight;
} TprWindowCreateInfo;

typedef struct TprActionCreateInfo {
    TprCreateActionFlags flags;
    TprInputElement element;
    float highThreshold;
    float lowThreshold;
} TprActionCreateInfo;

typedef struct TprMeshCreateInfo {
    TprCreateMeshFlags flags;
    TprFile file;
    uint32_t index;
} TprMeshCreateInfo;

typedef struct TprMeshLoadInfo {
    TprLoadMeshFlags flags;
} TprMeshLoadInfo;

typedef struct TprDepthDomainCreateInfo {
    TprCreateDepthDomainFlags flags;
    const TprDepthDomain* pAnchor;
} TprDepthDomainCreateInfo;

typedef struct TprRenderTargetCreateInfo {
    TprCreateRenderTargetFlags flags;
    TprWindow window;
    TprDepthDomain depthDomain;
    TprViewport viewport;
    TprScissor scissor;
} TprRenderTargetCreateInfo;

typedef struct TprObjectImageCreateInfo {
    TprCreateObjectImageFlags flags;
    TprMesh mesh;
    uint32_t renderTargetCount;
    const TprRenderTarget* pRenderTargets;
} TprObjectImageCreateInfo;

typedef struct TprJobInfo {
    TprCreateJobFlags flags;
    TprJobType type;
    void(*func)(void* ctx) _TPR_NOEXCEPT_ATTR;
    void* ctx;
    float priority;
    uint32_t dependencyJobCount;
    const TprJob* pDependencyJobs;
} TprJobCreateInfo;


#endif  // TEMPOR_PLUGIN_CORE_H_
