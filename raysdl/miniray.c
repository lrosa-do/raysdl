#include "miniray.h"

//#include "raylib.h"                 // Declares module functions

#include <stdlib.h>                     // Required for: exit()
#include <stdio.h>                      // Required for: FILE, fopen(), fseek(), ftell(), fread(), fwrite(), fprintf(), vprintf(), fclose()
#include <stdarg.h>                     // Required for: va_list, va_start(), va_end()
#include <string.h>                     // Required for: strcpy(), strcat()


// Check if config flags have been externally provided on compilation line
#include "config.h"             // Defines module configuration flags



#define RLGL_IMPLEMENTATION
#include "rlgl.h"                   // OpenGL abstraction layer to OpenGL 1.1, 3.3+ or ES2

#define RAYMATH_IMPLEMENTATION      // Define external out-of-line implementation
#include "raymath.h"                // Vector3, Quaternion and Matrix functionality

    #define GESTURES_IMPLEMENTATION
    #include "rgestures.h"           // Gestures detection functionality
    #define CAMERA_IMPLEMENTATION
    #include "rcamera.h"             // Camera system functionality
    #define MSF_GIF_MALLOC(contextPointer, newSize) RL_MALLOC(newSize)
    #define MSF_GIF_REALLOC(contextPointer, oldMemory, oldSize, newSize) RL_REALLOC(oldMemory, newSize)
    #define MSF_GIF_FREE(contextPointer, oldMemory, oldSize) RL_FREE(oldMemory)

    #define MSF_GIF_IMPL
    #include "external/msf_gif.h"   // GIF recording functionality
    #define SINFL_IMPLEMENTATION
    #define SINFL_NO_SIMD
    #include "external/sinfl.h"     // Deflate (RFC 1951) decompressor
    #define SDEFL_IMPLEMENTATION
    #include "external/sdefl.h"     // Deflate (RFC 1951) compressor

#if (defined(__linux__) || defined(PLATFORM_WEB)) && _POSIX_C_SOURCE < 199309L
    #undef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 199309L // Required for: CLOCK_MONOTONIC if compiled with c99 without gnu ext.
#endif

// Platform specific defines to handle GetApplicationDirectory()
#if defined (PLATFORM_DESKTOP)
    #if defined(_WIN32)
        #ifndef MAX_PATH
            #define MAX_PATH 1025
        #endif
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void *hModule, void *lpFilename, unsigned long nSize);
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(void *hModule, void *lpFilename, unsigned long nSize);
    __declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned int cp, unsigned long flags, void *widestr, int cchwide, void *str, int cbmb, void *defchar, int *used_default);
    #elif defined(__linux__)
        #include <unistd.h>
    #elif defined(__APPLE__)
        #include <sys/syslimits.h>
        #include <mach-o/dyld.h>
    #endif // OSs
#endif // PLATFORM_DESKTOP

#include <stdlib.h>                 // Required for: srand(), rand(), atexit()
#include <stdio.h>                  // Required for: sprintf() [Used in OpenURL()]
#include <string.h>                 // Required for: strrchr(), strcmp(), strlen()
#include <time.h>                   // Required for: time() [Used in InitTimer()]
#include <math.h>                   // Required for: tan() [Used in BeginMode3D()], atan2f() [Used in LoadVrStereoConfig()]

#include <sys/stat.h>               // Required for: stat() [Used in GetFileModTime()]

#if defined(PLATFORM_DESKTOP) && defined(_WIN32) && (defined(_MSC_VER) || defined(__TINYC__))
    #define DIRENT_MALLOC RL_MALLOC
    #define DIRENT_FREE RL_FREE

    #include "external/dirent.h"    // Required for: DIR, opendir(), closedir() [Used in GetDirectoryFiles()]
#else
    #include <dirent.h>             // Required for: DIR, opendir(), closedir() [Used in GetDirectoryFiles()]
#endif

#if defined(_WIN32)
    #include <direct.h>             // Required for: _getch(), _chdir()
    #define GETCWD _getcwd          // NOTE: MSDN recommends not to use getcwd(), chdir()
    #define CHDIR _chdir
    #include <io.h>                 // Required for: _access() [Used in FileExists()]
#else
    #include <unistd.h>             // Required for: getch(), chdir() (POSIX), access()
    #define GETCWD getcwd
    #define CHDIR chdir
#endif

#if defined(PLATFORM_DESKTOP)
    #define GLFW_INCLUDE_NONE       // Disable the standard OpenGL header inclusion on GLFW3
                                    // NOTE: Already provided by rlgl implementation (on glad.h)
   // #include "GLFW/glfw3.h"         // GLFW3 library: Windows, OpenGL context and Input management
                                    // NOTE: GLFW3 already includes gl.h (OpenGL) headers

    // Support retrieving native window handlers
    #if defined(_WIN32)
        #define GLFW_EXPOSE_NATIVE_WIN32
       // #include "GLFW/glfw3native.h"       // WARNING: It requires customization to avoid windows.h inclusion!

        #if defined(SUPPORT_WINMM_HIGHRES_TIMER) && !defined(SUPPORT_BUSY_WAIT_LOOP)
            // NOTE: Those functions require linking with winmm library
            unsigned int __stdcall timeBeginPeriod(unsigned int uPeriod);
            unsigned int __stdcall timeEndPeriod(unsigned int uPeriod);
        #endif
    #endif
    #if defined(__linux__) || defined(__FreeBSD__)
        #include <sys/time.h>               // Required for: timespec, nanosleep(), select() - POSIX

        //#define GLFW_EXPOSE_NATIVE_X11      // WARNING: Exposing Xlib.h > X.h results in dup symbols for Font type
        //#define GLFW_EXPOSE_NATIVE_WAYLAND
        //#define GLFW_EXPOSE_NATIVE_MIR
      //  #include "GLFW/glfw3native.h"       // Required for: glfwGetX11Window()
    #endif
    #if defined(__APPLE__)
        #include <unistd.h>                 // Required for: usleep()

        //#define GLFW_EXPOSE_NATIVE_COCOA    // WARNING: Fails due to type redefinition
     //   #include "GLFW/glfw3native.h"       // Required for: glfwGetCocoaWindow()
    #endif
#endif

#if defined(PLATFORM_ANDROID)

    #include <EGL/egl.h>                    // Native platform windowing system interface
    //#include <GLES2/gl2.h>                // OpenGL ES 2.0 library (not required in this module, only in rlgl)
#endif



#if defined(PLATFORM_WEB)
  //  #define GLFW_INCLUDE_ES2            // GLFW3: Enable OpenGL ES 2.0 (translated to WebGL)
   // #include "GLFW/glfw3.h"             // GLFW3: Windows, OpenGL context and Input management
    #include <sys/time.h>               // Required for: timespec, nanosleep(), select() - POSIX

    #include <emscripten/emscripten.h>  // Emscripten functionality for C
    #include <emscripten/html5.h>       // Emscripten HTML5 library
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------


#ifndef MAX_FILEPATH_LENGTH
    #if defined(__linux__)
        #define MAX_FILEPATH_LENGTH     4096        // Maximum length for filepaths (Linux PATH_MAX default value)
    #else
        #define MAX_FILEPATH_LENGTH      512        // Maximum length supported for filepaths
    #endif
#endif

#ifndef MAX_KEYBOARD_KEYS
    #define MAX_KEYBOARD_KEYS            512        // Maximum number of keyboard keys supported
#endif
#ifndef MAX_MOUSE_BUTTONS
    #define MAX_MOUSE_BUTTONS              8        // Maximum number of mouse buttons supported
#endif
#ifndef MAX_GAMEPADS
    #define MAX_GAMEPADS                   4        // Maximum number of gamepads supported
#endif
#ifndef MAX_GAMEPAD_AXIS
    #define MAX_GAMEPAD_AXIS               8        // Maximum number of axis supported (per gamepad)
#endif
#ifndef MAX_GAMEPAD_BUTTONS
    #define MAX_GAMEPAD_BUTTONS           32        // Maximum number of buttons supported (per gamepad)
#endif
#ifndef MAX_TOUCH_POINTS
    #define MAX_TOUCH_POINTS               8        // Maximum number of touch points supported
#endif
#ifndef MAX_KEY_PRESSED_QUEUE
    #define MAX_KEY_PRESSED_QUEUE         16        // Maximum number of keys in the key input queue
#endif
#ifndef MAX_CHAR_PRESSED_QUEUE
    #define MAX_CHAR_PRESSED_QUEUE        16        // Maximum number of characters in the char input queue
#endif

#if defined(SUPPORT_DATA_STORAGE)
    #ifndef STORAGE_DATA_FILE
        #define STORAGE_DATA_FILE  "storage.data"   // Automatic storage filename
    #endif
#endif

#ifndef MAX_DECOMPRESSION_SIZE
    #define MAX_DECOMPRESSION_SIZE        64        // Maximum size allocated for decompression in MB
#endif

// Flags operation macros
#define FLAG_SET(n, f) ((n) |= (f))
#define FLAG_CLEAR(n, f) ((n) &= ~(f))
#define FLAG_TOGGLE(n, f) ((n) ^= (f))
#define FLAG_CHECK(n, f) ((n) & (f))


typedef struct { int x; int y; } Point;
typedef struct { unsigned int width; unsigned int height; } Size;

// Core global state context data
typedef struct CoreData {
    struct {

//        GLFWwindow *handle;                 // GLFW window handle (graphic device)


        const char *title;                  // Window text title const pointer
        unsigned int flags;                 // Configuration flags (bit based), keeps window state
        bool ready;                         // Check if window has been initialized successfully
        bool fullscreen;                    // Check if fullscreen mode is enabled
        bool shouldClose;                   // Check if window set for closing
        bool resizedLastFrame;              // Check if window has been resized last frame

        Point position;                     // Window position on screen (required on fullscreen toggle)
        Size display;                       // Display width and height (monitor, device-screen, LCD, ...)
        Size screen;                        // Screen width and height (used render area)
        Size currentFbo;                    // Current render width and height (depends on active fbo)
        Size render;                        // Framebuffer width and height (render area, including black bars if required)
        Point renderOffset;                 // Offset from render area (must be divided by 2)
        Matrix screenScale;                 // Matrix to scale screen (framebuffer rendering)

        char **dropFilesPath;               // Store dropped files paths as strings
        int dropFileCount;                  // Count dropped files strings

    } Window;

    struct {
        const char *basePath;               // Base path for data storage
    } Storage;
    struct {

        struct {
            int exitKey;                    // Default exit key
            char currentKeyState[MAX_KEYBOARD_KEYS];        // Registers current frame key state
            char previousKeyState[MAX_KEYBOARD_KEYS];       // Registers previous frame key state

            int keyPressedQueue[MAX_KEY_PRESSED_QUEUE];     // Input keys queue
            int keyPressedQueueCount;       // Input keys queue count

            int charPressedQueue[MAX_CHAR_PRESSED_QUEUE];   // Input characters queue (unicode)
            int charPressedQueueCount;      // Input characters queue count


            int defaultFileFlags;           // Default IO file flags
          int fd;                         // File descriptor for the evdev keyboard

        } Keyboard;
        struct {
            Vector2 offset;                 // Mouse offset
            Vector2 scale;                  // Mouse scaling
            Vector2 currentPosition;        // Mouse position on screen
            Vector2 previousPosition;       // Previous mouse position

            int cursor;                     // Tracks current mouse cursor
            bool cursorHidden;              // Track if cursor is hidden
            bool cursorOnScreen;            // Tracks if cursor is inside client area

            char currentButtonState[MAX_MOUSE_BUTTONS];     // Registers current mouse button state
            char previousButtonState[MAX_MOUSE_BUTTONS];    // Registers previous mouse button state
            float currentWheelMove;         // Registers current mouse wheel variation
            float previousWheelMove;        // Registers previous mouse wheel variation

        } Mouse;
        struct {
            int pointCount;                             // Number of touch points active
            int pointId[MAX_TOUCH_POINTS];              // Point identifiers
            Vector2 position[MAX_TOUCH_POINTS];         // Touch position on screen
            char currentTouchState[MAX_TOUCH_POINTS];   // Registers current touch state
            char previousTouchState[MAX_TOUCH_POINTS];  // Registers previous touch state
        } Touch;
        struct {
            int lastButtonPressed;          // Register last gamepad button pressed
            int axisCount;                  // Register number of available gamepad axis
            bool ready[MAX_GAMEPADS];       // Flag to know if gamepad is ready
            char name[MAX_GAMEPADS][64];    // Gamepad name holder
            char currentButtonState[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS];     // Current gamepad buttons state
            char previousButtonState[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS];    // Previous gamepad buttons state
            float axisState[MAX_GAMEPADS][MAX_GAMEPAD_AXIS];                // Gamepad axis state

        } Gamepad;
    } Input;
    struct {
        double current;                     // Current time measure
        double previous;                    // Previous time measure
        double update;                      // Time measure for frame update
        double draw;                        // Time measure for frame draw
        double frame;                       // Time measure for one frame
        double target;                      // Desired time for one frame, if 0 not applied
#if defined(PLATFORM_ANDROID)
        unsigned long long base;            // Base time measure for hi-res timer
#endif
        unsigned int frameCounter;          // Frame counter
    } Time;
} CoreData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static CoreData CORE = { 0 };               // Global CORE state context

static char **dirFilesPath = NULL;          // Store directory files paths as strings
static int dirFileCount = 0;                // Count directory files strings

const char *raylibVersion = RAYLIB_VERSION; // raylib version symbol, it could be required for some bindings


static int screenshotCounter = 0;           // Screenshots counter

static int gifFrameCounter = 0;             // GIF frames counter
static bool gifRecording = false;           // GIF recording state
static MsfGifState gifState = { 0 };        // MSGIF context state


#define MAX_CODE_AUTOMATION_EVENTS      16384

typedef enum AutomationEventType {
    EVENT_NONE = 0,
    // Input events
    INPUT_KEY_UP,                   // param[0]: key
    INPUT_KEY_DOWN,                 // param[0]: key
    INPUT_KEY_PRESSED,              // param[0]: key
    INPUT_KEY_RELEASED,             // param[0]: key
    INPUT_MOUSE_BUTTON_UP,          // param[0]: button
    INPUT_MOUSE_BUTTON_DOWN,        // param[0]: button
    INPUT_MOUSE_POSITION,           // param[0]: x, param[1]: y
    INPUT_MOUSE_WHEEL_MOTION,       // param[0]: delta
    INPUT_GAMEPAD_CONNECT,          // param[0]: gamepad
    INPUT_GAMEPAD_DISCONNECT,       // param[0]: gamepad
    INPUT_GAMEPAD_BUTTON_UP,        // param[0]: button
    INPUT_GAMEPAD_BUTTON_DOWN,      // param[0]: button
    INPUT_GAMEPAD_AXIS_MOTION,      // param[0]: axis, param[1]: delta
    INPUT_TOUCH_UP,                 // param[0]: id
    INPUT_TOUCH_DOWN,               // param[0]: id
    INPUT_TOUCH_POSITION,           // param[0]: x, param[1]: y
    INPUT_GESTURE,                  // param[0]: gesture
    // Window events
    WINDOW_CLOSE,                   // no params
    WINDOW_MAXIMIZE,                // no params
    WINDOW_MINIMIZE,                // no params
    WINDOW_RESIZE,                  // param[0]: width, param[1]: height
    // Custom events
    ACTION_TAKE_SCREENSHOT,
    ACTION_SETTARGETFPS
} AutomationEventType;

// Event type
// Used to enable events flags
typedef enum {
    EVENT_INPUT_KEYBOARD    = 0,
    EVENT_INPUT_MOUSE       = 1,
    EVENT_INPUT_GAMEPAD     = 2,
    EVENT_INPUT_TOUCH       = 4,
    EVENT_INPUT_GESTURE     = 8,
    EVENT_WINDOW            = 16,
    EVENT_CUSTOM            = 32
} EventType;

static const char *autoEventTypeName[] = {
    "EVENT_NONE",
    "INPUT_KEY_UP",
    "INPUT_KEY_DOWN",
    "INPUT_KEY_PRESSED",
    "INPUT_KEY_RELEASED",
    "INPUT_MOUSE_BUTTON_UP",
    "INPUT_MOUSE_BUTTON_DOWN",
    "INPUT_MOUSE_POSITION",
    "INPUT_MOUSE_WHEEL_MOTION",
    "INPUT_GAMEPAD_CONNECT",
    "INPUT_GAMEPAD_DISCONNECT",
    "INPUT_GAMEPAD_BUTTON_UP",
    "INPUT_GAMEPAD_BUTTON_DOWN",
    "INPUT_GAMEPAD_AXIS_MOTION",
    "INPUT_TOUCH_UP",
    "INPUT_TOUCH_DOWN",
    "INPUT_TOUCH_POSITION",
    "INPUT_GESTURE",
    "WINDOW_CLOSE",
    "WINDOW_MAXIMIZE",
    "WINDOW_MINIMIZE",
    "WINDOW_RESIZE",
    "ACTION_TAKE_SCREENSHOT",
    "ACTION_SETTARGETFPS"
};

// Automation Event (20 bytes)
typedef struct AutomationEvent {
    unsigned int frame;                 // Event frame
    unsigned int type;                  // Event type (AutoEventType)
    int params[3];                      // Event parameters (if required)
} AutomationEvent;

static AutomationEvent *events = NULL;        // Events array
static unsigned int eventCount = 0;     // Events count
static bool eventsPlaying = false;      // Play events
static bool eventsRecording = false;    // Record events

//static short eventsEnabled = 0b0000001111111111;    // Events enabled for checking

//-----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Other Modules Functions Declaration (required by core)
//----------------------------------------------------------------------------------

extern void LoadFontDefault(void);          // [Module: text] Loads default font on InitWindow()
extern void UnloadFontDefault(void);        // [Module: text] Unloads default font from GPU memory

//----------------------------------------------------------------------------------
// Module specific Functions Declaration
//----------------------------------------------------------------------------------
static void InitTimer(void);                            // Initialize timer (hi-resolution if available)
static bool InitGraphicsDevice(int width, int height);  // Initialize graphics device
static void SetupFramebuffer(int width, int height);    // Setup main framebuffer
static void SetupViewport(int width, int height);       // Set viewport for a provided width and height

#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
static void ErrorCallback(int error, const char *description);                             // GLFW3 Error Callback, runs on GLFW3 error
// Window callbacks events
static void WindowSizeCallback(int width, int height);                 // GLFW3 WindowSize Callback, runs when window is resized
#if !defined(PLATFORM_WEB)
static void WindowMaximizeCallback( int maximized);                     // GLFW3 Window Maximize Callback, runs when window is maximized
#endif
static void WindowIconifyCallback( int iconified);                      // GLFW3 WindowIconify Callback, runs when window is minimized/restored
static void WindowFocusCallback( int focused);                          // GLFW3 WindowFocus Callback, runs when window get/lose focus
static void WindowDropCallback( int count, const char **paths);         // GLFW3 Window Drop Callback, runs when drop files into window
// Input callbacks events
static void KeyCallback(int key, int scancode, int action, int mods);  // GLFW3 Keyboard Callback, runs on key pressed
static void CharCallback( unsigned int key);                            // GLFW3 Char Key Callback, runs on key pressed (get char value)
static void MouseButtonCallback( int button, int action, int mods);     // GLFW3 Mouse Button Callback, runs on mouse button pressed
static void MouseCursorPosCallback( double x, double y);                // GLFW3 Cursor Position Callback, runs on mouse move
static void MouseScrollCallback( double xoffset, double yoffset);       // GLFW3 Srolling Callback, runs on mouse wheel
static void CursorEnterCallback( int enter);                            // GLFW3 Cursor Enter Callback, cursor enters client area
#endif



#if defined(SUPPORT_EVENTS_AUTOMATION)
static void LoadAutomationEvents(const char *fileName);     // Load automation events from file
static void ExportAutomationEvents(const char *fileName);   // Export recorded automation events into a file
static void RecordAutomationEvent(unsigned int frame);      // Record frame events (to internal events array)
static void PlayAutomationEvent(unsigned int frame);        // Play frame events (from internal events array)
#endif

#if defined(_WIN32)
// NOTE: We declare Sleep() function symbol to avoid including windows.h (kernel32.lib linkage required)
void __stdcall Sleep(unsigned long msTimeout);              // Required for: WaitTime()
#endif


//----------------------------------------------------------------------------------
// Module Functions Definition - Window and OpenGL Context Functions
//----------------------------------------------------------------------------------


// Initialize window and OpenGL context
// NOTE: data parameter could be used to pass any kind of required data to the initialization
void InitWindow(int width, int height, const char *title)
{
    TRACELOG(LOG_INFO, "Initializing raylib %s", RAYLIB_VERSION);
    if ((title != NULL) && (title[0] != 0)) CORE.Window.title = title;

    // Initialize required global values different than 0
    CORE.Input.Keyboard.exitKey = KEY_ESCAPE;
    CORE.Input.Mouse.scale = (Vector2){ 1.0f, 1.0f };
    CORE.Input.Mouse.cursor = MOUSE_CURSOR_ARROW;
    CORE.Input.Gamepad.lastButtonPressed = -1;

#if defined(PLATFORM_ANDROID)
    CORE.Window.screen.width = width;
    CORE.Window.screen.height = height;
    CORE.Window.currentFbo.width = width;
    CORE.Window.currentFbo.height = height;

    // TODO: Automatic orientation doesn't seem to work
    if (width <= height)
    {

        TRACELOG(LOG_WARNING, "ANDROID: Window orientation changed to portrait");
    }
    else
    {

        TRACELOG(LOG_WARNING, "ANDROID: Window orientation changed to landscape");
    }



    TRACELOG(LOG_INFO, "ANDROID: App initialized successfully");
#endif // defined

#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
    // Initialize graphics device (display device and OpenGL context)
    // NOTE: returns true if window and graphic device has been initialized successfully
    CORE.Window.ready = InitGraphicsDevice(width, height);

    // If graphic device is no properly initialized, we end program
    if (!CORE.Window.ready)
    {
        TRACELOG(LOG_FATAL, "Failed to initialize Graphic Device");
        return;
    }

    // Initialize hi-res timer
    InitTimer();

    // Initialize random seed
    srand((unsigned int)time(NULL));

    // Initialize base path for storage
    CORE.Storage.basePath = GetWorkingDirectory();

    // Load default font
    // WARNING: External function: Module required: rtext
    LoadFontDefault();
    Rectangle rec = GetFontDefault().recs[95];
    // NOTE: We setup a 1px padding on char rectangle to avoid pixel bleeding on MSAA filtering
    SetShapesTexture(GetFontDefault().texture, (Rectangle){ rec.x + 1, rec.y + 1, rec.width - 2, rec.height - 2 }); // WARNING: Module required: rshapes
    // Set default texture and rectangle to be used for shapes drawing
    // NOTE: rlgl default texture is a 1x1 pixel UNCOMPRESSED_R8G8B8A8
    Texture2D texture = { rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    SetShapesTexture(texture, (Rectangle){ 0.0f, 0.0f, 1.0f, 1.0f });    // WARNING: Module required: rshapes
    if ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0)
    {
        // Set default font texture filter for HighDPI (blurry)
        // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
        rlTextureParameters(GetFontDefault().texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
        rlTextureParameters(GetFontDefault().texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
    }
#endif // defined


    CORE.Input.Mouse.currentPosition.x = (float)CORE.Window.screen.width/2.0f;
    CORE.Input.Mouse.currentPosition.y = (float)CORE.Window.screen.height/2.0f;


    events = (AutomationEvent *)malloc(MAX_CODE_AUTOMATION_EVENTS*sizeof(AutomationEvent));
    CORE.Time.frameCounter = 0;
}

// Close window and unload OpenGL context
void CloseWindow(void)
{
#if defined(SUPPORT_GIF_RECORDING)
    if (gifRecording)
    {
        MsfGifResult result = msf_gif_end(&gifState);
        msf_gif_free(result);
        gifRecording = false;
    }
#endif


    UnloadFontDefault();        // WARNING: Module required: rtext

    rlglClose();                // De-init rlgl

//    glfwDestroyWindow(CORE.Window.handle);
//    glfwTerminate();



    free(events);

    CORE.Window.ready = false;
    TRACELOG(LOG_INFO, "Window closed successfully");
}

// Check if KEY_ESCAPE pressed or Close icon pressed
bool WindowShouldClose(void)
{
#if defined(PLATFORM_WEB)
    // Emterpreter-Async required to run sync code
    // https://github.com/emscripten-core/emscripten/wiki/Emterpreter#emterpreter-async-run-synchronous-code
    // By default, this function is never called on a web-ready raylib example because we encapsulate
    // frame code in a UpdateDrawFrame() function, to allow browser manage execution asynchronously
    // but now emscripten allows sync code to be executed in an interpreted way, using emterpreter!
    emscripten_sleep(16);
    return false;
#endif

#if defined(PLATFORM_DESKTOP)
    if (CORE.Window.ready)
    {
        // While window minimized, stop loop execution
     //   while (IsWindowState(FLAG_WINDOW_MINIMIZED) && !IsWindowState(FLAG_WINDOW_ALWAYS_RUN)) glfwWaitEvents();

   //     CORE.Window.shouldClose = glfwWindowShouldClose(CORE.Window.handle);

        // Reset close status for next frame
     //   glfwSetWindowShouldClose(CORE.Window.handle, GLFW_FALSE);

        return CORE.Window.shouldClose;
    }
    else return true;
#endif

#if defined(PLATFORM_ANDROID)
    if (CORE.Window.ready) return CORE.Window.shouldClose;
    else return true;
#endif
}

// Check if window has been initialized successfully
bool IsWindowReady(void)
{
    return CORE.Window.ready;
}

// Check if window is currently fullscreen
bool IsWindowFullscreen(void)
{
    return CORE.Window.fullscreen;
}

// Check if window is currently hidden
bool IsWindowHidden(void)
{
#if defined(PLATFORM_DESKTOP)
    return ((CORE.Window.flags & FLAG_WINDOW_HIDDEN) > 0);
#endif
    return false;
}

// Check if window has been minimized
bool IsWindowMinimized(void)
{
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
    return ((CORE.Window.flags & FLAG_WINDOW_MINIMIZED) > 0);
#else
    return false;
#endif
}

// Check if window has been maximized (only PLATFORM_DESKTOP)
bool IsWindowMaximized(void)
{
#if defined(PLATFORM_DESKTOP)
    return ((CORE.Window.flags & FLAG_WINDOW_MAXIMIZED) > 0);
#else
    return false;
#endif
}

// Check if window has the focus
bool IsWindowFocused(void)
{
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
    return ((CORE.Window.flags & FLAG_WINDOW_UNFOCUSED) == 0);
#else
    return true;
#endif
}

// Check if window has been resizedLastFrame
bool IsWindowResized(void)
{
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
    return CORE.Window.resizedLastFrame;
#else
    return false;
#endif
}

// Check if one specific window flag is enabled
bool IsWindowState(unsigned int flag)
{
    return ((CORE.Window.flags & flag) > 0);
}

// Toggle fullscreen mode (only PLATFORM_DESKTOP)
void ToggleFullscreen(void)
{
/*
#if defined(PLATFORM_DESKTOP)
    // NOTE: glfwSetWindowMonitor() doesn't work properly (bugs)
    if (!CORE.Window.fullscreen)
    {
        // Store previous window position (in case we exit fullscreen)
        glfwGetWindowPos(CORE.Window.handle, &CORE.Window.position.x, &CORE.Window.position.y);

        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

        int monitorIndex = GetCurrentMonitor();

        // Use current monitor, so we correctly get the display the window is on
        GLFWmonitor* monitor = monitorIndex < monitorCount ?  monitors[monitorIndex] : NULL;

        if (!monitor)
        {
            TRACELOG(LOG_WARNING, "GLFW: Failed to get monitor");

            CORE.Window.fullscreen = false;          // Toggle fullscreen flag
            CORE.Window.flags &= ~FLAG_FULLSCREEN_MODE;

            glfwSetWindowMonitor(CORE.Window.handle, NULL, 0, 0, CORE.Window.screen.width, CORE.Window.screen.height, GLFW_DONT_CARE);
            return;
        }

        CORE.Window.fullscreen = true;          // Toggle fullscreen flag
        CORE.Window.flags |= FLAG_FULLSCREEN_MODE;

        glfwSetWindowMonitor(CORE.Window.handle, monitor, 0, 0, CORE.Window.screen.width, CORE.Window.screen.height, GLFW_DONT_CARE);
    }
    else
    {
        CORE.Window.fullscreen = false;          // Toggle fullscreen flag
        CORE.Window.flags &= ~FLAG_FULLSCREEN_MODE;

        glfwSetWindowMonitor(CORE.Window.handle, NULL, CORE.Window.position.x, CORE.Window.position.y, CORE.Window.screen.width, CORE.Window.screen.height, GLFW_DONT_CARE);
    }

    // Try to enable GPU V-Sync, so frames are limited to screen refresh rate (60Hz -> 60 FPS)
    // NOTE: V-Sync can be enabled by graphic driver configuration
    if (CORE.Window.flags & FLAG_VSYNC_HINT) glfwSwapInterval(1);
#endif


    CORE.Window.fullscreen = !CORE.Window.fullscreen;          // Toggle fullscreen flag
#endif
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_RPI) || defined(PLATFORM_DRM)
    TRACELOG(LOG_WARNING, "SYSTEM: Failed to toggle to windowed mode");
#endif*/
}

// Set window state: maximized, if resizable (only PLATFORM_DESKTOP)
void MaximizeWindow(void)
{/*
#if defined(PLATFORM_DESKTOP)
    if (glfwGetWindowAttrib(CORE.Window.handle, GLFW_RESIZABLE) == GLFW_TRUE)
    {
        glfwMaximizeWindow(CORE.Window.handle);
        CORE.Window.flags |= FLAG_WINDOW_MAXIMIZED;
    }
#endif*/
}

// Set window state: minimized (only PLATFORM_DESKTOP)
void MinimizeWindow(void)
{/*
#if defined(PLATFORM_DESKTOP)
    // NOTE: Following function launches callback that sets appropiate flag!
    glfwIconifyWindow(CORE.Window.handle);
#endif*/
}

// Set window state: not minimized/maximized (only PLATFORM_DESKTOP)
void RestoreWindow(void)
{
/*
#if defined(PLATFORM_DESKTOP)
    if (glfwGetWindowAttrib(CORE.Window.handle, GLFW_RESIZABLE) == GLFW_TRUE)
    {
        // Restores the specified window if it was previously iconified (minimized) or maximized
        glfwRestoreWindow(CORE.Window.handle);
        CORE.Window.flags &= ~FLAG_WINDOW_MINIMIZED;
        CORE.Window.flags &= ~FLAG_WINDOW_MAXIMIZED;
    }
#endif
*/
}

// Set window configuration state using flags
void SetWindowState(unsigned int flags)
{
/*
#if defined(PLATFORM_DESKTOP)
    // Check previous state and requested state to apply required changes
    // NOTE: In most cases the functions already change the flags internally

    // State change: FLAG_VSYNC_HINT
    if (((CORE.Window.flags & FLAG_VSYNC_HINT) != (flags & FLAG_VSYNC_HINT)) && ((flags & FLAG_VSYNC_HINT) > 0))
    {
        glfwSwapInterval(1);
        CORE.Window.flags |= FLAG_VSYNC_HINT;
    }

    // State change: FLAG_FULLSCREEN_MODE
    if ((CORE.Window.flags & FLAG_FULLSCREEN_MODE) != (flags & FLAG_FULLSCREEN_MODE))
    {
        ToggleFullscreen();     // NOTE: Window state flag updated inside function
    }

    // State change: FLAG_WINDOW_RESIZABLE
    if (((CORE.Window.flags & FLAG_WINDOW_RESIZABLE) != (flags & FLAG_WINDOW_RESIZABLE)) && ((flags & FLAG_WINDOW_RESIZABLE) > 0))
    {
        glfwSetWindowAttrib(CORE.Window.handle, GLFW_RESIZABLE, GLFW_TRUE);
        CORE.Window.flags |= FLAG_WINDOW_RESIZABLE;
    }

    // State change: FLAG_WINDOW_UNDECORATED
    if (((CORE.Window.flags & FLAG_WINDOW_UNDECORATED) != (flags & FLAG_WINDOW_UNDECORATED)) && (flags & FLAG_WINDOW_UNDECORATED))
    {
        glfwSetWindowAttrib(CORE.Window.handle, GLFW_DECORATED, GLFW_FALSE);
        CORE.Window.flags |= FLAG_WINDOW_UNDECORATED;
    }

    // State change: FLAG_WINDOW_HIDDEN
    if (((CORE.Window.flags & FLAG_WINDOW_HIDDEN) != (flags & FLAG_WINDOW_HIDDEN)) && ((flags & FLAG_WINDOW_HIDDEN) > 0))
    {
        glfwHideWindow(CORE.Window.handle);
        CORE.Window.flags |= FLAG_WINDOW_HIDDEN;
    }

    // State change: FLAG_WINDOW_MINIMIZED
    if (((CORE.Window.flags & FLAG_WINDOW_MINIMIZED) != (flags & FLAG_WINDOW_MINIMIZED)) && ((flags & FLAG_WINDOW_MINIMIZED) > 0))
    {
        //GLFW_ICONIFIED
        MinimizeWindow();       // NOTE: Window state flag updated inside function
    }

    // State change: FLAG_WINDOW_MAXIMIZED
    if (((CORE.Window.flags & FLAG_WINDOW_MAXIMIZED) != (flags & FLAG_WINDOW_MAXIMIZED)) && ((flags & FLAG_WINDOW_MAXIMIZED) > 0))
    {
        //GLFW_MAXIMIZED
        MaximizeWindow();       // NOTE: Window state flag updated inside function
    }

    // State change: FLAG_WINDOW_UNFOCUSED
    if (((CORE.Window.flags & FLAG_WINDOW_UNFOCUSED) != (flags & FLAG_WINDOW_UNFOCUSED)) && ((flags & FLAG_WINDOW_UNFOCUSED) > 0))
    {
        glfwSetWindowAttrib(CORE.Window.handle, GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
        CORE.Window.flags |= FLAG_WINDOW_UNFOCUSED;
    }

    // State change: FLAG_WINDOW_TOPMOST
    if (((CORE.Window.flags & FLAG_WINDOW_TOPMOST) != (flags & FLAG_WINDOW_TOPMOST)) && ((flags & FLAG_WINDOW_TOPMOST) > 0))
    {
        glfwSetWindowAttrib(CORE.Window.handle, GLFW_FLOATING, GLFW_TRUE);
        CORE.Window.flags |= FLAG_WINDOW_TOPMOST;
    }

    // State change: FLAG_WINDOW_ALWAYS_RUN
    if (((CORE.Window.flags & FLAG_WINDOW_ALWAYS_RUN) != (flags & FLAG_WINDOW_ALWAYS_RUN)) && ((flags & FLAG_WINDOW_ALWAYS_RUN) > 0))
    {
        CORE.Window.flags |= FLAG_WINDOW_ALWAYS_RUN;
    }

    // The following states can not be changed after window creation

    // State change: FLAG_WINDOW_TRANSPARENT
    if (((CORE.Window.flags & FLAG_WINDOW_TRANSPARENT) != (flags & FLAG_WINDOW_TRANSPARENT)) && ((flags & FLAG_WINDOW_TRANSPARENT) > 0))
    {
        TRACELOG(LOG_WARNING, "WINDOW: Framebuffer transparency can only by configured before window initialization");
    }

    // State change: FLAG_WINDOW_HIGHDPI
    if (((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) != (flags & FLAG_WINDOW_HIGHDPI)) && ((flags & FLAG_WINDOW_HIGHDPI) > 0))
    {
        TRACELOG(LOG_WARNING, "WINDOW: High DPI can only by configured before window initialization");
    }

    // State change: FLAG_MSAA_4X_HINT
    if (((CORE.Window.flags & FLAG_MSAA_4X_HINT) != (flags & FLAG_MSAA_4X_HINT)) && ((flags & FLAG_MSAA_4X_HINT) > 0))
    {
        TRACELOG(LOG_WARNING, "WINDOW: MSAA can only by configured before window initialization");
    }

    // State change: FLAG_INTERLACED_HINT
    if (((CORE.Window.flags & FLAG_INTERLACED_HINT) != (flags & FLAG_INTERLACED_HINT)) && ((flags & FLAG_INTERLACED_HINT) > 0))
    {
        TRACELOG(LOG_WARNING, "RPI: Interlaced mode can only by configured before window initialization");
    }
#endif*/
}

// Clear window configuration state flags
void ClearWindowState(unsigned int flags)
{/*
#if defined(PLATFORM_DESKTOP)
    // Check previous state and requested state to apply required changes
    // NOTE: In most cases the functions already change the flags internally

    // State change: FLAG_VSYNC_HINT
    if (((CORE.Window.flags & FLAG_VSYNC_HINT) > 0) && ((flags & FLAG_VSYNC_HINT) > 0))
    {
        glfwSwapInterval(0);
        CORE.Window.flags &= ~FLAG_VSYNC_HINT;
    }

    // State change: FLAG_FULLSCREEN_MODE
    if (((CORE.Window.flags & FLAG_FULLSCREEN_MODE) > 0) && ((flags & FLAG_FULLSCREEN_MODE) > 0))
    {
        ToggleFullscreen();     // NOTE: Window state flag updated inside function
    }

    // State change: FLAG_WINDOW_RESIZABLE
    if (((CORE.Window.flags & FLAG_WINDOW_RESIZABLE) > 0) && ((flags & FLAG_WINDOW_RESIZABLE) > 0))
    {
        glfwSetWindowAttrib(CORE.Window.handle, GLFW_RESIZABLE, GLFW_FALSE);
        CORE.Window.flags &= ~FLAG_WINDOW_RESIZABLE;
    }

    // State change: FLAG_WINDOW_UNDECORATED
    if (((CORE.Window.flags & FLAG_WINDOW_UNDECORATED) > 0) && ((flags & FLAG_WINDOW_UNDECORATED) > 0))
    {
        glfwSetWindowAttrib(CORE.Window.handle, GLFW_DECORATED, GLFW_TRUE);
        CORE.Window.flags &= ~FLAG_WINDOW_UNDECORATED;
    }

    // State change: FLAG_WINDOW_HIDDEN
    if (((CORE.Window.flags & FLAG_WINDOW_HIDDEN) > 0) && ((flags & FLAG_WINDOW_HIDDEN) > 0))
    {
        glfwShowWindow(CORE.Window.handle);
        CORE.Window.flags &= ~FLAG_WINDOW_HIDDEN;
    }

    // State change: FLAG_WINDOW_MINIMIZED
    if (((CORE.Window.flags & FLAG_WINDOW_MINIMIZED) > 0) && ((flags & FLAG_WINDOW_MINIMIZED) > 0))
    {
        RestoreWindow();       // NOTE: Window state flag updated inside function
    }

    // State change: FLAG_WINDOW_MAXIMIZED
    if (((CORE.Window.flags & FLAG_WINDOW_MAXIMIZED) > 0) && ((flags & FLAG_WINDOW_MAXIMIZED) > 0))
    {
        RestoreWindow();       // NOTE: Window state flag updated inside function
    }

    // State change: FLAG_WINDOW_UNFOCUSED
    if (((CORE.Window.flags & FLAG_WINDOW_UNFOCUSED) > 0) && ((flags & FLAG_WINDOW_UNFOCUSED) > 0))
    {
        glfwSetWindowAttrib(CORE.Window.handle, GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
        CORE.Window.flags &= ~FLAG_WINDOW_UNFOCUSED;
    }

    // State change: FLAG_WINDOW_TOPMOST
    if (((CORE.Window.flags & FLAG_WINDOW_TOPMOST) > 0) && ((flags & FLAG_WINDOW_TOPMOST) > 0))
    {
        glfwSetWindowAttrib(CORE.Window.handle, GLFW_FLOATING, GLFW_FALSE);
        CORE.Window.flags &= ~FLAG_WINDOW_TOPMOST;
    }

    // State change: FLAG_WINDOW_ALWAYS_RUN
    if (((CORE.Window.flags & FLAG_WINDOW_ALWAYS_RUN) > 0) && ((flags & FLAG_WINDOW_ALWAYS_RUN) > 0))
    {
        CORE.Window.flags &= ~FLAG_WINDOW_ALWAYS_RUN;
    }

    // The following states can not be changed after window creation

    // State change: FLAG_WINDOW_TRANSPARENT
    if (((CORE.Window.flags & FLAG_WINDOW_TRANSPARENT) > 0) && ((flags & FLAG_WINDOW_TRANSPARENT) > 0))
    {
        TRACELOG(LOG_WARNING, "WINDOW: Framebuffer transparency can only by configured before window initialization");
    }

    // State change: FLAG_WINDOW_HIGHDPI
    if (((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0) && ((flags & FLAG_WINDOW_HIGHDPI) > 0))
    {
        TRACELOG(LOG_WARNING, "WINDOW: High DPI can only by configured before window initialization");
    }

    // State change: FLAG_MSAA_4X_HINT
    if (((CORE.Window.flags & FLAG_MSAA_4X_HINT) > 0) && ((flags & FLAG_MSAA_4X_HINT) > 0))
    {
        TRACELOG(LOG_WARNING, "WINDOW: MSAA can only by configured before window initialization");
    }

    // State change: FLAG_INTERLACED_HINT
    if (((CORE.Window.flags & FLAG_INTERLACED_HINT) > 0) && ((flags & FLAG_INTERLACED_HINT) > 0))
    {
        TRACELOG(LOG_WARNING, "RPI: Interlaced mode can only by configured before window initialization");
    }
#endif*/
}

// Set icon for window (only PLATFORM_DESKTOP)
// NOTE: Image must be in RGBA format, 8bit per channel
void SetWindowIcon(Image image)
{/*
#if defined(PLATFORM_DESKTOP)
    if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
    {
        GLFWimage icon[1] = { 0 };

        icon[0].width = image.width;
        icon[0].height = image.height;
        icon[0].pixels = (unsigned char *)image.data;

        // NOTE 1: We only support one image icon
        // NOTE 2: The specified image data is copied before this function returns
        glfwSetWindowIcon(CORE.Window.handle, 1, icon);
    }
    else TRACELOG(LOG_WARNING, "GLFW: Window icon image must be in R8G8B8A8 pixel format");
#endif*/
}

// Set title for window (only PLATFORM_DESKTOP)
void SetWindowTitle(const char *title)
{
    CORE.Window.title = title;
#if defined(PLATFORM_DESKTOP)
 //   glfwSetWindowTitle(CORE.Window.handle, title);
#endif
}

// Set window position on screen (windowed mode)
void SetWindowPosition(int x, int y)
{
#if defined(PLATFORM_DESKTOP)
 //   glfwSetWindowPos(CORE.Window.handle, x, y);
#endif
}

// Set monitor for the current window (fullscreen mode)
void SetWindowMonitor(int monitor)
{/*
#if defined(PLATFORM_DESKTOP)
    int monitorCount = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

    if ((monitor >= 0) && (monitor < monitorCount))
    {
        TRACELOG(LOG_INFO, "GLFW: Selected fullscreen monitor: [%i] %s", monitor, glfwGetMonitorName(monitors[monitor]));

        const GLFWvidmode *mode = glfwGetVideoMode(monitors[monitor]);
        glfwSetWindowMonitor(CORE.Window.handle, monitors[monitor], 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else TRACELOG(LOG_WARNING, "GLFW: Failed to find selected monitor");
#endif*/
}

// Set window minimum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMinSize(int width, int height)
{/*
#if defined(PLATFORM_DESKTOP)
    const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowSizeLimits(CORE.Window.handle, width, height, mode->width, mode->height);
#endif*/
}

// Set window dimensions
void SetWindowSize(int width, int height)
{/*
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
    glfwSetWindowSize(CORE.Window.handle, width, height);
#endif*/
}

// Set window opacity, value opacity is between 0.0 and 1.0
void SetWindowOpacity(float opacity)
{/*
#if defined(PLATFORM_DESKTOP)
    if (opacity >= 1.0f) opacity = 1.0f;
    else if (opacity <= 0.0f) opacity = 0.0f;
//    glfwSetWindowOpacity(CORE.Window.handle, opacity);
#endif*/
}

// Get current screen width
int GetScreenWidth(void)
{
    return CORE.Window.screen.width;
}

// Get current screen height
int GetScreenHeight(void)
{
    return CORE.Window.screen.height;
}

// Get current render width which is equal to screen width * dpi scale
int GetRenderWidth(void)
{
    return CORE.Window.render.width;
}

// Get current screen height which is equal to screen height * dpi scale
int GetRenderHeight(void)
{
    return CORE.Window.render.height;
}

// Get native window handle
void *GetWindowHandle(void)
{
/*
#if defined(PLATFORM_DESKTOP) && defined(_WIN32)
    // NOTE: Returned handle is: void *HWND (windows.h)
    return glfwGetWin32Window(CORE.Window.handle);
#endif
#if defined(__linux__)
    // NOTE: Returned handle is: unsigned long Window (X.h)
    // typedef unsigned long XID;
    // typedef XID Window;
    //unsigned long id = (unsigned long)glfwGetX11Window(window);
    return NULL;    // TODO: Find a way to return value... cast to void *?
#endif
#if defined(__APPLE__)
    // NOTE: Returned handle is: (objc_object *)
    return NULL;    // TODO: return (void *)glfwGetCocoaWindow(window);
#endif*/

    return NULL;
}

// Get number of monitors
int GetMonitorCount(void)
{/*
#if defined(PLATFORM_DESKTOP)
    int monitorCount;
    glfwGetMonitors(&monitorCount);
    return monitorCount;
#else
    return 1;
#endif*/
return 1;
}

// Get number of monitors
int GetCurrentMonitor(void)
{
/*
#if defined(PLATFORM_DESKTOP)
    int monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    GLFWmonitor* monitor = NULL;

    if (monitorCount == 1) // easy out
        return 0;

    if (IsWindowFullscreen())
    {
        monitor = glfwGetWindowMonitor(CORE.Window.handle);
        for (int i = 0; i < monitorCount; i++)
        {
            if (monitors[i] == monitor)
                return i;
        }
        return 0;
    }
    else
    {
        int x = 0;
        int y = 0;

        glfwGetWindowPos(CORE.Window.handle, &x, &y);

        for (int i = 0; i < monitorCount; i++)
        {
            int mx = 0;
            int my = 0;

            int width = 0;
            int height = 0;

            monitor = monitors[i];
            glfwGetMonitorWorkarea(monitor, &mx, &my, &width, &height);
            if (x >= mx && x <= (mx + width) && y >= my && y <= (my + height))
                return i;
        }
    }
    return 0;
#else*/
    return 0;

}

// Get selected monitor width
Vector2 GetMonitorPosition(int monitor)
{/*
#if defined(PLATFORM_DESKTOP)
    int monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    if ((monitor >= 0) && (monitor < monitorCount))
    {
        int x, y;
        glfwGetMonitorPos(monitors[monitor], &x, &y);

        return (Vector2){ (float)x, (float)y };
    }
    else TRACELOG(LOG_WARNING, "GLFW: Failed to find selected monitor");
#endif*/
    return (Vector2){ 0, 0 };
}

// Get selected monitor width (max available by monitor)
int GetMonitorWidth(int monitor)
{/*
#if defined(PLATFORM_DESKTOP)
    int monitorCount;
    GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

    if ((monitor >= 0) && (monitor < monitorCount))
    {
        int count = 0;
        const GLFWvidmode *modes = glfwGetVideoModes(monitors[monitor], &count);

        // We return the maximum resolution available, the last one in the modes array
        if (count > 0) return modes[count - 1].width;
        else TRACELOG(LOG_WARNING, "GLFW: Failed to find video mode for selected monitor");
    }
    else TRACELOG(LOG_WARNING, "GLFW: Failed to find selected monitor");
#endif*/
    return 0;
}

// Get selected monitor width (max available by monitor)
int GetMonitorHeight(int monitor)
{/*
#if defined(PLATFORM_DESKTOP)
    int monitorCount;
    GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

    if ((monitor >= 0) && (monitor < monitorCount))
    {
        int count = 0;
        const GLFWvidmode *modes = glfwGetVideoModes(monitors[monitor], &count);

        // We return the maximum resolution available, the last one in the modes array
        if (count > 0) return modes[count - 1].height;
        else TRACELOG(LOG_WARNING, "GLFW: Failed to find video mode for selected monitor");
    }
    else TRACELOG(LOG_WARNING, "GLFW: Failed to find selected monitor");
#endif*/
    return 0;
}

// Get selected monitor physical width in millimetres
int GetMonitorPhysicalWidth(int monitor)
{/*
#if defined(PLATFORM_DESKTOP)
    int monitorCount;
    GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

    if ((monitor >= 0) && (monitor < monitorCount))
    {
        int physicalWidth;
        glfwGetMonitorPhysicalSize(monitors[monitor], &physicalWidth, NULL);
        return physicalWidth;
    }
    else TRACELOG(LOG_WARNING, "GLFW: Failed to find selected monitor");
#endif*/
    return 0;
}

// Get primary monitor physical height in millimetres
int GetMonitorPhysicalHeight(int monitor)
{/*
#if defined(PLATFORM_DESKTOP)
    int monitorCount;
    GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

    if ((monitor >= 0) && (monitor < monitorCount))
    {
        int physicalHeight;
        glfwGetMonitorPhysicalSize(monitors[monitor], NULL, &physicalHeight);
        return physicalHeight;
    }
    else TRACELOG(LOG_WARNING, "GLFW: Failed to find selected monitor");
#endif*/
    return 0;
}

int GetMonitorRefreshRate(int monitor)
{/*
#if defined(PLATFORM_DESKTOP)
    int monitorCount;
    GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

    if ((monitor >= 0) && (monitor < monitorCount))
    {
        const GLFWvidmode *vidmode = glfwGetVideoMode(monitors[monitor]);
        return vidmode->refreshRate;
    }
    else TRACELOG(LOG_WARNING, "GLFW: Failed to find selected monitor");
#endif
#if defined(PLATFORM_DRM)
    if ((CORE.Window.connector) && (CORE.Window.modeIndex >= 0))
    {
        return CORE.Window.connector->modes[CORE.Window.modeIndex].vrefresh;
    }
#endif*/
    return 0;
}

// Get window position XY on monitor
Vector2 GetWindowPosition(void)
{
    int x = 0;
    int y = 0;
#if defined(PLATFORM_DESKTOP)
  //  glfwGetWindowPos(CORE.Window.handle, &x, &y);
#endif
    return (Vector2){ (float)x, (float)y };
}

// Get window scale DPI factor
Vector2 GetWindowScaleDPI(void)
{
    Vector2 scale = { 1.0f, 1.0f };
/*
#if defined(PLATFORM_DESKTOP)
    float xdpi = 1.0;
    float ydpi = 1.0;
    Vector2 windowPos = GetWindowPosition();

    int monitorCount = 0;
    GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

    // Check window monitor
    for (int i = 0; i < monitorCount; i++)
    {
        glfwGetMonitorContentScale(monitors[i], &xdpi, &ydpi);

        int xpos, ypos, width, height;
        glfwGetMonitorWorkarea(monitors[i], &xpos, &ypos, &width, &height);

        if ((windowPos.x >= xpos) && (windowPos.x < xpos + width) &&
            (windowPos.y >= ypos) && (windowPos.y < ypos + height))
        {
            scale.x = xdpi;
            scale.y = ydpi;
            break;
        }
    }
#endif
*/
    return scale;
}

// Get the human-readable, UTF-8 encoded name of the primary monitor
const char *GetMonitorName(int monitor)
{/*
#if defined(PLATFORM_DESKTOP)
    int monitorCount;
    GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

    if ((monitor >= 0) && (monitor < monitorCount))
    {
        return glfwGetMonitorName(monitors[monitor]);
    }
    else TRACELOG(LOG_WARNING, "GLFW: Failed to find selected monitor");
#endif*/
    return "";
}

// Get clipboard text content
// NOTE: returned string is allocated and freed by GLFW
const char *GetClipboardText(void)
{/*
#if defined(PLATFORM_DESKTOP)
    return glfwGetClipboardString(CORE.Window.handle);
#endif
#if defined(PLATFORM_WEB)
    return emscripten_run_script_string("navigator.clipboard.readText()");
#endif*/
    return NULL;
}

// Set clipboard text content
void SetClipboardText(const char *text)
{/*
#if defined(PLATFORM_DESKTOP)
    glfwSetClipboardString(CORE.Window.handle, text);
#endif
#if defined(PLATFORM_WEB)
    emscripten_run_script(TextFormat("navigator.clipboard.writeText('%s')", text));
#endif*/
}

// Show mouse cursor
void ShowCursor(void)
{
#if defined(PLATFORM_DESKTOP)
  //  glfwSetInputMode(CORE.Window.handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
#endif

    CORE.Input.Mouse.cursorHidden = false;
}

// Hides mouse cursor
void HideCursor(void)
{
#if defined(PLATFORM_DESKTOP)
   // glfwSetInputMode(CORE.Window.handle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
#endif

    CORE.Input.Mouse.cursorHidden = true;
}

// Check if cursor is not visible
bool IsCursorHidden(void)
{
    return CORE.Input.Mouse.cursorHidden;
}

// Enables cursor (unlock cursor)
void EnableCursor(void)
{
#if defined(PLATFORM_DESKTOP)
  //  glfwSetInputMode(CORE.Window.handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
#endif
#if defined(PLATFORM_WEB)
   // emscripten_exit_pointerlock();
#endif

    CORE.Input.Mouse.cursorHidden = false;
}

// Disables cursor (lock cursor)
void DisableCursor(void)
{
#if defined(PLATFORM_DESKTOP)
  //  glfwSetInputMode(CORE.Window.handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#endif
#if defined(PLATFORM_WEB)
    emscripten_request_pointerlock("#canvas", 1);
#endif

    CORE.Input.Mouse.cursorHidden = true;
}

// Check if cursor is on the current screen.
bool IsCursorOnScreen(void)
{
    return CORE.Input.Mouse.cursorOnScreen;
}

// Set background color (framebuffer clear color)
void ClearBackground(Color color)
{
    rlClearColor(color.r, color.g, color.b, color.a);   // Set clear color
    rlClearScreenBuffers();                             // Clear current framebuffers
}

// Setup canvas (framebuffer) to start drawing
void BeginDrawing(void)
{
    // WARNING: Previously to BeginDrawing() other render textures drawing could happen,
    // consequently the measure for update vs draw is not accurate (only the total frame time is accurate)

    CORE.Time.current = GetTime();      // Number of elapsed seconds since InitTimer()
    CORE.Time.update = CORE.Time.current - CORE.Time.previous;
    CORE.Time.previous = CORE.Time.current;

    rlLoadIdentity();                   // Reset current matrix (modelview)
    rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling

    //rlTranslatef(0.375, 0.375, 0);    // HACK to have 2D pixel-perfect drawing on OpenGL 1.1
                                        // NOTE: Not required with OpenGL 3.3+
}

// End canvas drawing and swap buffers (double buffering)
void EndDrawing(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

#if defined(SUPPORT_MODULE_RSHAPES) && defined(SUPPORT_MOUSE_CURSOR_POINT)
    // Draw a small rectangle on mouse position for user reference
    if (!CORE.Input.Mouse.cursorHidden)
    {
        DrawRectangle(CORE.Input.Mouse.currentPosition.x, CORE.Input.Mouse.currentPosition.y, 3, 3, MAROON);    // WARNING: Module required: rshapes
        rlDrawRenderBatchActive();  // Update and draw internal render batch
    }
#endif

#if defined(SUPPORT_GIF_RECORDING)
    // Draw record indicator
    if (gifRecording)
    {
        #define GIF_RECORD_FRAMERATE    10
        gifFrameCounter++;

        // NOTE: We record one gif frame every 10 game frames
        if ((gifFrameCounter%GIF_RECORD_FRAMERATE) == 0)
        {
            // Get image data for the current frame (from backbuffer)
            // NOTE: This process is quite slow... :(
            unsigned char *screenData = rlReadScreenPixels(CORE.Window.screen.width, CORE.Window.screen.height);
            msf_gif_frame(&gifState, screenData, 10, 16, CORE.Window.screen.width*4);

            RL_FREE(screenData);    // Free image data
        }

    #if defined(SUPPORT_MODULE_RSHAPES) && defined(SUPPORT_MODULE_RTEXT)
        if (((gifFrameCounter/15)%2) == 1)
        {
            DrawCircle(30, CORE.Window.screen.height - 20, 10, MAROON);                 // WARNING: Module required: rshapes
            DrawText("GIF RECORDING", 50, CORE.Window.screen.height - 25, 10, RED);     // WARNING: Module required: rtext
        }
    #endif

        rlDrawRenderBatchActive();  // Update and draw internal render batch
    }
#endif

#if defined(SUPPORT_EVENTS_AUTOMATION)
    // Draw record/play indicator
    if (eventsRecording)
    {
        gifFrameCounter++;

        if (((gifFrameCounter/15)%2) == 1)
        {
            DrawCircle(30, CORE.Window.screen.height - 20, 10, MAROON);
            DrawText("EVENTS RECORDING", 50, CORE.Window.screen.height - 25, 10, RED);
        }

        rlDrawRenderBatchActive();  // Update and draw internal render batch
    }
    else if (eventsPlaying)
    {
        gifFrameCounter++;

        if (((gifFrameCounter/15)%2) == 1)
        {
            DrawCircle(30, CORE.Window.screen.height - 20, 10, LIME);
            DrawText("EVENTS PLAYING", 50, CORE.Window.screen.height - 25, 10, GREEN);
        }

        rlDrawRenderBatchActive();  // Update and draw internal render batch
    }
#endif

#if !defined(SUPPORT_CUSTOM_FRAME_CONTROL)
    SwapScreenBuffer();                  // Copy back buffer to front buffer (screen)

    // Frame time control system
    CORE.Time.current = GetTime();
    CORE.Time.draw = CORE.Time.current - CORE.Time.previous;
    CORE.Time.previous = CORE.Time.current;

    CORE.Time.frame = CORE.Time.update + CORE.Time.draw;

    // Wait for some milliseconds...
    if (CORE.Time.frame < CORE.Time.target)
    {
        WaitTime((float)(CORE.Time.target - CORE.Time.frame)*1000.0f);

        CORE.Time.current = GetTime();
        double waitTime = CORE.Time.current - CORE.Time.previous;
        CORE.Time.previous = CORE.Time.current;

        CORE.Time.frame += waitTime;    // Total frame time: update + draw + wait
    }

    PollInputEvents();      // Poll user events (before next frame update)
#endif

#if defined(SUPPORT_EVENTS_AUTOMATION)
    // Events recording and playing logic
    if (eventsRecording) RecordAutomationEvent(CORE.Time.frameCounter);
    else if (eventsPlaying)
    {
        // TODO: When should we play? After/before/replace PollInputEvents()?
        if (CORE.Time.frameCounter >= eventCount) eventsPlaying = false;
        PlayAutomationEvent(CORE.Time.frameCounter);
    }
#endif

    CORE.Time.frameCounter++;
}

// Initialize 2D mode with custom camera (2D)
void BeginMode2D(Camera2D camera)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlLoadIdentity();               // Reset current matrix (modelview)

    // Apply 2d camera transformation to modelview
    rlMultMatrixf(MatrixToFloat(GetCameraMatrix2D(camera)));

    // Apply screen scaling if required
    rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale));
}

// Ends 2D mode with custom camera
void EndMode2D(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlLoadIdentity();               // Reset current matrix (modelview)
    rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling if required
}

// Initializes 3D mode with custom camera (3D)
void BeginMode3D(Camera3D camera)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlMatrixMode(RL_PROJECTION);    // Switch to projection matrix
    rlPushMatrix();                 // Save previous matrix, which contains the settings for the 2d ortho projection
    rlLoadIdentity();               // Reset current matrix (projection)

    float aspect = (float)CORE.Window.currentFbo.width/(float)CORE.Window.currentFbo.height;

    // NOTE: zNear and zFar values are important when computing depth buffer values
    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Setup perspective projection
        double top = RL_CULL_DISTANCE_NEAR*tan(camera.fovy*0.5*DEG2RAD);
        double right = top*aspect;

        rlFrustum(-right, right, -top, top, RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        // Setup orthographic projection
        double top = camera.fovy/2.0;
        double right = top*aspect;

        rlOrtho(-right, right, -top,top, RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    }

    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)

    // Setup Camera view
    Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);
    rlMultMatrixf(MatrixToFloat(matView));      // Multiply modelview matrix by view matrix (camera)

    rlEnableDepthTest();            // Enable DEPTH_TEST for 3D
}

// Ends 3D mode and returns to default 2D orthographic mode
void EndMode3D(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlMatrixMode(RL_PROJECTION);    // Switch to projection matrix
    rlPopMatrix();                  // Restore previous matrix (projection) from matrix stack

    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)

    rlMultMatrixf(MatrixToFloat(CORE.Window.screenScale)); // Apply screen scaling if required

    rlDisableDepthTest();           // Disable DEPTH_TEST for 2D
}

// Initializes render texture for drawing
void BeginTextureMode(RenderTexture2D target)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlEnableFramebuffer(target.id); // Enable render target

    // Set viewport to framebuffer size
    rlViewport(0, 0, target.texture.width, target.texture.height);

    rlMatrixMode(RL_PROJECTION);    // Switch to projection matrix
    rlLoadIdentity();               // Reset current matrix (projection)

    // Set orthographic projection to current framebuffer size
    // NOTE: Configured top-left corner as (0, 0)
    rlOrtho(0, target.texture.width, target.texture.height, 0, 0.0f, 1.0f);

    rlMatrixMode(RL_MODELVIEW);     // Switch back to modelview matrix
    rlLoadIdentity();               // Reset current matrix (modelview)

    //rlScalef(0.0f, -1.0f, 0.0f);  // Flip Y-drawing (?)

    // Setup current width/height for proper aspect ratio
    // calculation when using BeginMode3D()
    CORE.Window.currentFbo.width = target.texture.width;
    CORE.Window.currentFbo.height = target.texture.height;
}

// Ends drawing to render texture
void EndTextureMode(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlDisableFramebuffer();         // Disable render target (fbo)

    // Set viewport to default framebuffer size
    SetupViewport(CORE.Window.render.width, CORE.Window.render.height);

    // Reset current fbo to screen size
    CORE.Window.currentFbo.width = CORE.Window.render.width;
    CORE.Window.currentFbo.height = CORE.Window.render.height;
}

// Begin custom shader mode
void BeginShaderMode(Shader shader)
{
    rlSetShader(shader.id, shader.locs);
}

// End custom shader mode (returns to default shader)
void EndShaderMode(void)
{
    rlSetShader(rlGetShaderIdDefault(), rlGetShaderLocsDefault());
}

// Begin blending mode (alpha, additive, multiplied, subtract, custom)
// NOTE: Blend modes supported are enumerated in BlendMode enum
void BeginBlendMode(int mode)
{
    rlSetBlendMode(mode);
}

// End blending mode (reset to default: alpha blending)
void EndBlendMode(void)
{
    rlSetBlendMode(BLEND_ALPHA);
}

// Begin scissor mode (define screen area for following drawing)
// NOTE: Scissor rec refers to bottom-left corner, we change it to upper-left
void BeginScissorMode(int x, int y, int width, int height)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch

    rlEnableScissorTest();

#if defined(__APPLE__)
    Vector2 scale = GetWindowScaleDPI();
    rlScissor((int)(x*scale.x), (int)(GetScreenHeight()*scale.y - (((y + height)*scale.y))), (int)(width*scale.x), (int)(height*scale.y));
#else
    if ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0)
    {
        Vector2 scale = GetWindowScaleDPI();
        rlScissor((int)(x*scale.x), (int)(CORE.Window.currentFbo.height - (y + height)*scale.y), (int)(width*scale.x), (int)(height*scale.y));
    }
    else
    {
        rlScissor(x, CORE.Window.currentFbo.height - (y + height), width, height);
    }
#endif
}

// End scissor mode
void EndScissorMode(void)
{
    rlDrawRenderBatchActive();      // Update and draw internal render batch
    rlDisableScissorTest();
}

// Begin VR drawing configuration
void BeginVrStereoMode(VrStereoConfig config)
{
    rlEnableStereoRender();

    // Set stereo render matrices
    rlSetMatrixProjectionStereo(config.projection[0], config.projection[1]);
    rlSetMatrixViewOffsetStereo(config.viewOffset[0], config.viewOffset[1]);
}

// End VR drawing process (and desktop mirror)
void EndVrStereoMode(void)
{
    rlDisableStereoRender();
}

// Load VR stereo config for VR simulator device parameters
VrStereoConfig LoadVrStereoConfig(VrDeviceInfo device)
{
    VrStereoConfig config = { 0 };

    if ((rlGetVersion() == OPENGL_33) || (rlGetVersion() == OPENGL_ES_20))
    {
        // Compute aspect ratio
        float aspect = ((float)device.hResolution*0.5f)/(float)device.vResolution;

        // Compute lens parameters
        float lensShift = (device.hScreenSize*0.25f - device.lensSeparationDistance*0.5f)/device.hScreenSize;
        config.leftLensCenter[0] = 0.25f + lensShift;
        config.leftLensCenter[1] = 0.5f;
        config.rightLensCenter[0] = 0.75f - lensShift;
        config.rightLensCenter[1] = 0.5f;
        config.leftScreenCenter[0] = 0.25f;
        config.leftScreenCenter[1] = 0.5f;
        config.rightScreenCenter[0] = 0.75f;
        config.rightScreenCenter[1] = 0.5f;

        // Compute distortion scale parameters
        // NOTE: To get lens max radius, lensShift must be normalized to [-1..1]
        float lensRadius = fabsf(-1.0f - 4.0f*lensShift);
        float lensRadiusSq = lensRadius*lensRadius;
        float distortionScale = device.lensDistortionValues[0] +
                                device.lensDistortionValues[1]*lensRadiusSq +
                                device.lensDistortionValues[2]*lensRadiusSq*lensRadiusSq +
                                device.lensDistortionValues[3]*lensRadiusSq*lensRadiusSq*lensRadiusSq;

        float normScreenWidth = 0.5f;
        float normScreenHeight = 1.0f;
        config.scaleIn[0] = 2.0f/normScreenWidth;
        config.scaleIn[1] = 2.0f/normScreenHeight/aspect;
        config.scale[0] = normScreenWidth*0.5f/distortionScale;
        config.scale[1] = normScreenHeight*0.5f*aspect/distortionScale;

        // Fovy is normally computed with: 2*atan2f(device.vScreenSize, 2*device.eyeToScreenDistance)
        // ...but with lens distortion it is increased (see Oculus SDK Documentation)
        //float fovy = 2.0f*atan2f(device.vScreenSize*0.5f*distortionScale, device.eyeToScreenDistance);     // Really need distortionScale?
        float fovy = 2.0f*(float)atan2f(device.vScreenSize*0.5f, device.eyeToScreenDistance);

        // Compute camera projection matrices
        float projOffset = 4.0f*lensShift;      // Scaled to projection space coordinates [-1..1]
        Matrix proj = MatrixPerspective(fovy, aspect, RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);

        config.projection[0] = MatrixMultiply(proj, MatrixTranslate(projOffset, 0.0f, 0.0f));
        config.projection[1] = MatrixMultiply(proj, MatrixTranslate(-projOffset, 0.0f, 0.0f));

        // Compute camera transformation matrices
        // NOTE: Camera movement might seem more natural if we model the head.
        // Our axis of rotation is the base of our head, so we might want to add
        // some y (base of head to eye level) and -z (center of head to eye protrusion) to the camera positions.
        config.viewOffset[0] = MatrixTranslate(-device.interpupillaryDistance*0.5f, 0.075f, 0.045f);
        config.viewOffset[1] = MatrixTranslate(device.interpupillaryDistance*0.5f, 0.075f, 0.045f);

        // Compute eyes Viewports
        /*
        config.eyeViewportRight[0] = 0;
        config.eyeViewportRight[1] = 0;
        config.eyeViewportRight[2] = device.hResolution/2;
        config.eyeViewportRight[3] = device.vResolution;

        config.eyeViewportLeft[0] = device.hResolution/2;
        config.eyeViewportLeft[1] = 0;
        config.eyeViewportLeft[2] = device.hResolution/2;
        config.eyeViewportLeft[3] = device.vResolution;
        */
    }
    else TRACELOG(LOG_WARNING, "RLGL: VR Simulator not supported on OpenGL 1.1");

    return config;
}

// Unload VR stereo config properties
void UnloadVrStereoConfig(VrStereoConfig config)
{
    //...
}

// Load shader from files and bind default locations
// NOTE: If shader string is NULL, using default vertex/fragment shaders
Shader LoadShader(const char *vsFileName, const char *fsFileName)
{
    Shader shader = { 0 };

    char *vShaderStr = NULL;
    char *fShaderStr = NULL;

    if (vsFileName != NULL) vShaderStr = LoadFileText(vsFileName);
    if (fsFileName != NULL) fShaderStr = LoadFileText(fsFileName);

    shader = LoadShaderFromMemory(vShaderStr, fShaderStr);

    UnloadFileText(vShaderStr);
    UnloadFileText(fShaderStr);

    return shader;
}

// Load shader from code strings and bind default locations
RLAPI Shader LoadShaderFromMemory(const char *vsCode, const char *fsCode)
{
    Shader shader = { 0 };
    shader.locs = (int *)RL_CALLOC(RL_MAX_SHADER_LOCATIONS, sizeof(int));

    // NOTE: All locations must be reseted to -1 (no location)
    for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; i++) shader.locs[i] = -1;

    shader.id = rlLoadShaderCode(vsCode, fsCode);

    // After shader loading, we TRY to set default location names
    if (shader.id > 0)
    {
        // Default shader attribute locations have been binded before linking:
        //          vertex position location    = 0
        //          vertex texcoord location    = 1
        //          vertex normal location      = 2
        //          vertex color location       = 3
        //          vertex tangent location     = 4
        //          vertex texcoord2 location   = 5

        // NOTE: If any location is not found, loc point becomes -1

        // Get handles to GLSL input attibute locations
        shader.locs[SHADER_LOC_VERTEX_POSITION] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_POSITION);
        shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_TEXCOORD);
        shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_TEXCOORD2);
        shader.locs[SHADER_LOC_VERTEX_NORMAL] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_NORMAL);
        shader.locs[SHADER_LOC_VERTEX_TANGENT] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_TANGENT);
        shader.locs[SHADER_LOC_VERTEX_COLOR] = rlGetLocationAttrib(shader.id, RL_DEFAULT_SHADER_ATTRIB_NAME_COLOR);

        // Get handles to GLSL uniform locations (vertex shader)
        shader.locs[SHADER_LOC_MATRIX_MVP] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_MVP);
        shader.locs[SHADER_LOC_MATRIX_VIEW] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_VIEW);
        shader.locs[SHADER_LOC_MATRIX_PROJECTION] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_PROJECTION);
        shader.locs[SHADER_LOC_MATRIX_MODEL] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_MODEL);
        shader.locs[SHADER_LOC_MATRIX_NORMAL] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_NORMAL);

        // Get handles to GLSL uniform locations (fragment shader)
        shader.locs[SHADER_LOC_COLOR_DIFFUSE] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_UNIFORM_NAME_COLOR);
        shader.locs[SHADER_LOC_MAP_DIFFUSE] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE0);  // SHADER_LOC_MAP_ALBEDO
        shader.locs[SHADER_LOC_MAP_SPECULAR] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE1); // SHADER_LOC_MAP_METALNESS
        shader.locs[SHADER_LOC_MAP_NORMAL] = rlGetLocationUniform(shader.id, RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE2);
    }

    return shader;
}

// Unload shader from GPU memory (VRAM)
void UnloadShader(Shader shader)
{
    if (shader.id != rlGetShaderIdDefault())
    {
        rlUnloadShaderProgram(shader.id);
        RL_FREE(shader.locs);
    }
}

// Get shader uniform location
int GetShaderLocation(Shader shader, const char *uniformName)
{
    return rlGetLocationUniform(shader.id, uniformName);
}

// Get shader attribute location
int GetShaderLocationAttrib(Shader shader, const char *attribName)
{
    return rlGetLocationAttrib(shader.id, attribName);
}

// Set shader uniform value
void SetShaderValue(Shader shader, int locIndex, const void *value, int uniformType)
{
    SetShaderValueV(shader, locIndex, value, uniformType, 1);
}

// Set shader uniform value vector
void SetShaderValueV(Shader shader, int locIndex, const void *value, int uniformType, int count)
{
    rlEnableShader(shader.id);
    rlSetUniform(locIndex, value, uniformType, count);
    //rlDisableShader();      // Avoid reseting current shader program, in case other uniforms are set
}

// Set shader uniform value (matrix 4x4)
void SetShaderValueMatrix(Shader shader, int locIndex, Matrix mat)
{
    rlEnableShader(shader.id);
    rlSetUniformMatrix(locIndex, mat);
    //rlDisableShader();
}

// Set shader uniform value for texture
void SetShaderValueTexture(Shader shader, int locIndex, Texture2D texture)
{
    rlEnableShader(shader.id);
    rlSetUniformSampler(locIndex, texture.id);
    //rlDisableShader();
}

// Get a ray trace from mouse position
Ray GetMouseRay(Vector2 mouse, Camera camera)
{
    Ray ray = { 0 };

    // Calculate normalized device coordinates
    // NOTE: y value is negative
    float x = (2.0f*mouse.x)/(float)GetScreenWidth() - 1.0f;
    float y = 1.0f - (2.0f*mouse.y)/(float)GetScreenHeight();
    float z = 1.0f;

    // Store values in a vector
    Vector3 deviceCoords = { x, y, z };

    // Calculate view matrix from camera look at
    Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

    Matrix matProj = MatrixIdentity();

    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Calculate projection matrix from perspective
        matProj = MatrixPerspective(camera.fovy*DEG2RAD, ((double)GetScreenWidth()/(double)GetScreenHeight()), RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        float aspect = (float)CORE.Window.screen.width/(float)CORE.Window.screen.height;
        double top = camera.fovy/2.0;
        double right = top*aspect;

        // Calculate projection matrix from orthographic
        matProj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
    }

    // Unproject far/near points
    Vector3 nearPoint = Vector3Unproject((Vector3){ deviceCoords.x, deviceCoords.y, 0.0f }, matProj, matView);
    Vector3 farPoint = Vector3Unproject((Vector3){ deviceCoords.x, deviceCoords.y, 1.0f }, matProj, matView);

    // Unproject the mouse cursor in the near plane.
    // We need this as the source position because orthographic projects, compared to perspect doesn't have a
    // convergence point, meaning that the "eye" of the camera is more like a plane than a point.
    Vector3 cameraPlanePointerPos = Vector3Unproject((Vector3){ deviceCoords.x, deviceCoords.y, -1.0f }, matProj, matView);

    // Calculate normalized direction vector
    Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

    if (camera.projection == CAMERA_PERSPECTIVE) ray.position = camera.position;
    else if (camera.projection == CAMERA_ORTHOGRAPHIC) ray.position = cameraPlanePointerPos;

    // Apply calculated vectors to ray
    ray.direction = direction;

    return ray;
}

// Get transform matrix for camera
Matrix GetCameraMatrix(Camera camera)
{
    return MatrixLookAt(camera.position, camera.target, camera.up);
}

// Get camera 2d transform matrix
Matrix GetCameraMatrix2D(Camera2D camera)
{
    Matrix matTransform = { 0 };
    // The camera in world-space is set by
    //   1. Move it to target
    //   2. Rotate by -rotation and scale by (1/zoom)
    //      When setting higher scale, it's more intuitive for the world to become bigger (= camera become smaller),
    //      not for the camera getting bigger, hence the invert. Same deal with rotation.
    //   3. Move it by (-offset);
    //      Offset defines target transform relative to screen, but since we're effectively "moving" screen (camera)
    //      we need to do it into opposite direction (inverse transform)

    // Having camera transform in world-space, inverse of it gives the modelview transform.
    // Since (A*B*C)' = C'*B'*A', the modelview is
    //   1. Move to offset
    //   2. Rotate and Scale
    //   3. Move by -target
    Matrix matOrigin = MatrixTranslate(-camera.target.x, -camera.target.y, 0.0f);
    Matrix matRotation = MatrixRotate((Vector3){ 0.0f, 0.0f, 1.0f }, camera.rotation*DEG2RAD);
    Matrix matScale = MatrixScale(camera.zoom, camera.zoom, 1.0f);
    Matrix matTranslation = MatrixTranslate(camera.offset.x, camera.offset.y, 0.0f);

    matTransform = MatrixMultiply(MatrixMultiply(matOrigin, MatrixMultiply(matScale, matRotation)), matTranslation);

    return matTransform;
}

// Get the screen space position from a 3d world space position
Vector2 GetWorldToScreen(Vector3 position, Camera camera)
{
    Vector2 screenPosition = GetWorldToScreenEx(position, camera, GetScreenWidth(), GetScreenHeight());

    return screenPosition;
}

// Get size position for a 3d world space position (useful for texture drawing)
Vector2 GetWorldToScreenEx(Vector3 position, Camera camera, int width, int height)
{
    // Calculate projection matrix (from perspective instead of frustum
    Matrix matProj = MatrixIdentity();

    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Calculate projection matrix from perspective
        matProj = MatrixPerspective(camera.fovy*DEG2RAD, ((double)width/(double)height), RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        float aspect = (float)CORE.Window.screen.width/(float)CORE.Window.screen.height;
        double top = camera.fovy/2.0;
        double right = top*aspect;

        // Calculate projection matrix from orthographic
        matProj = MatrixOrtho(-right, right, -top, top, RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    }

    // Calculate view matrix from camera look at (and transpose it)
    Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

    // TODO: Why not use Vector3Transform(Vector3 v, Matrix mat)?

    // Convert world position vector to quaternion
    Quaternion worldPos = { position.x, position.y, position.z, 1.0f };

    // Transform world position to view
    worldPos = QuaternionTransform(worldPos, matView);

    // Transform result to projection (clip space position)
    worldPos = QuaternionTransform(worldPos, matProj);

    // Calculate normalized device coordinates (inverted y)
    Vector3 ndcPos = { worldPos.x/worldPos.w, -worldPos.y/worldPos.w, worldPos.z/worldPos.w };

    // Calculate 2d screen position vector
    Vector2 screenPosition = { (ndcPos.x + 1.0f)/2.0f*(float)width, (ndcPos.y + 1.0f)/2.0f*(float)height };

    return screenPosition;
}

// Get the screen space position for a 2d camera world space position
Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera)
{
    Matrix matCamera = GetCameraMatrix2D(camera);
    Vector3 transform = Vector3Transform((Vector3){ position.x, position.y, 0 }, matCamera);

    return (Vector2){ transform.x, transform.y };
}

// Get the world space position for a 2d camera screen space position
Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera)
{
    Matrix invMatCamera = MatrixInvert(GetCameraMatrix2D(camera));
    Vector3 transform = Vector3Transform((Vector3){ position.x, position.y, 0 }, invMatCamera);

    return (Vector2){ transform.x, transform.y };
}

// Set target FPS (maximum)
void SetTargetFPS(int fps)
{
    if (fps < 1) CORE.Time.target = 0.0;
    else CORE.Time.target = 1.0/(double)fps;

    TRACELOG(LOG_INFO, "TIMER: Target time per frame: %02.03f milliseconds", (float)CORE.Time.target*1000.0f);
}

// Get current FPS
// NOTE: We calculate an average framerate
int GetFPS(void)
{
    int fps = 0;

#if !defined(SUPPORT_CUSTOM_FRAME_CONTROL)
    #define FPS_CAPTURE_FRAMES_COUNT    30      // 30 captures
    #define FPS_AVERAGE_TIME_SECONDS   0.5f     // 500 millisecondes
    #define FPS_STEP (FPS_AVERAGE_TIME_SECONDS/FPS_CAPTURE_FRAMES_COUNT)

    static int index = 0;
    static float history[FPS_CAPTURE_FRAMES_COUNT] = { 0 };
    static float average = 0, last = 0;
    float fpsFrame = GetFrameTime();

    if (fpsFrame == 0) return 0;

    if ((GetTime() - last) > FPS_STEP)
    {
        last = (float)GetTime();
        index = (index + 1)%FPS_CAPTURE_FRAMES_COUNT;
        average -= history[index];
        history[index] = fpsFrame/FPS_CAPTURE_FRAMES_COUNT;
        average += history[index];
    }

    fps = (int)roundf(1.0f/average);
#endif

    return fps;
}

// Get time in seconds for last frame drawn (delta time)
float GetFrameTime(void)
{
    return (float)CORE.Time.frame;
}

// Get elapsed time measure in seconds since InitTimer()
// NOTE: On PLATFORM_DESKTOP InitTimer() is called on InitWindow()
// NOTE: On PLATFORM_DESKTOP, timer is initialized on glfwInit()
double GetTime(void)
{
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
    return SDL_GetTicks();   // Elapsed time since glfwInit()
#endif

/*
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long int time = (unsigned long long int)ts.tv_sec*1000000000LLU + (unsigned long long int)ts.tv_nsec;

    return (double)(time - CORE.Time.base)*1e-9;  // Elapsed time since InitTimer()
*/
}

// Setup window configuration flags (view FLAGS)
// NOTE: This function is expected to be called before window creation,
// because it setups some flags for the window creation process.
// To configure window states after creation, just use SetWindowState()
void SetConfigFlags(unsigned int flags)
{
    // Selected flags are set but not evaluated at this point,
    // flag evaluation happens at InitWindow() or SetWindowState()
    CORE.Window.flags |= flags;
}

// NOTE TRACELOG() function is located in [utils.h]

// Takes a screenshot of current screen (saved a .png)
void TakeScreenshot(const char *fileName)
{
#if defined(SUPPORT_MODULE_RTEXTURES)
    unsigned char *imgData = rlReadScreenPixels(CORE.Window.render.width, CORE.Window.render.height);
    Image image = { imgData, CORE.Window.render.width, CORE.Window.render.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

    char path[2048] = { 0 };
    strcpy(path, TextFormat("%s/%s", CORE.Storage.basePath, fileName));

    ExportImage(image, path);           // WARNING: Module required: rtextures
    RL_FREE(imgData);

#if defined(PLATFORM_WEB)
    // Download file from MEMFS (emscripten memory filesystem)
    // saveFileFromMEMFSToDisk() function is defined in raylib/src/shell.html
    emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", GetFileName(path), GetFileName(path)));
#endif

    TRACELOG(LOG_INFO, "SYSTEM: [%s] Screenshot taken successfully", path);
#else
    TRACELOG(LOG_WARNING,"IMAGE: ExportImage() requires module: rtextures");
#endif
}

// Get a random value between min and max (both included)
int GetRandomValue(int min, int max)
{
    if (min > max)
    {
        int tmp = max;
        max = min;
        min = tmp;
    }

    return (rand()%(abs(max - min) + 1) + min);
}

// Set the seed for the random number generator
void SetRandomSeed(unsigned int seed)
{
    srand(seed);
}

// Check if the file exists
bool FileExists(const char *fileName)
{
    bool result = false;

#if defined(_WIN32)
    if (_access(fileName, 0) != -1) result = true;
#else
    if (access(fileName, F_OK) != -1) result = true;
#endif

    // NOTE: Alternatively, stat() can be used instead of access()
    //#include <sys/stat.h>
    //struct stat statbuf;
    //if (stat(filename, &statbuf) == 0) result = true;

    return result;
}

// Check file extension
// NOTE: Extensions checking is not case-sensitive
bool IsFileExtension(const char *fileName, const char *ext)
{
    bool result = false;
    const char *fileExt = GetFileExtension(fileName);

    if (fileExt != NULL)
    {
#if defined(SUPPORT_MODULE_RTEXT) && defined(SUPPORT_TEXT_MANIPULATION)
        int extCount = 0;
        const char **checkExts = TextSplit(ext, ';', &extCount); // WARNING: Module required: rtext

        char fileExtLower[16] = { 0 };
        strcpy(fileExtLower, TextToLower(fileExt));  // WARNING: Module required: rtext

        for (int i = 0; i < extCount; i++)
        {
            if (strcmp(fileExtLower, TextToLower(checkExts[i])) == 0)
            {
                result = true;
                break;
            }
        }
#else
        if (strcmp(fileExt, ext) == 0) result = true;
#endif
    }

    return result;
}

// Check if a directory path exists
bool DirectoryExists(const char *dirPath)
{
    bool result = false;
    DIR *dir = opendir(dirPath);

    if (dir != NULL)
    {
        result = true;
        closedir(dir);
    }

    return result;
}

// Get file length in bytes
// NOTE: GetFileSize() conflicts with windows.h
int GetFileLength(const char *fileName)
{
    int size = 0;

    FILE *file = fopen(fileName, "rb");

    if (file != NULL)
    {
        fseek(file, 0L, SEEK_END);
        size = (int)ftell(file);
        fclose(file);
    }

    return size;
}

// Get pointer to extension for a filename string (includes the dot: .png)
const char *GetFileExtension(const char *fileName)
{
    const char *dot = strrchr(fileName, '.');

    if (!dot || dot == fileName) return NULL;

    return dot;
}

// String pointer reverse break: returns right-most occurrence of charset in s
static const char *strprbrk(const char *s, const char *charset)
{
    const char *latestMatch = NULL;
    for (; s = strpbrk(s, charset), s != NULL; latestMatch = s++) { }
    return latestMatch;
}

// Get pointer to filename for a path string
const char *GetFileName(const char *filePath)
{
    const char *fileName = NULL;
    if (filePath != NULL) fileName = strprbrk(filePath, "\\/");

    if (!fileName) return filePath;

    return fileName + 1;
}

// Get filename string without extension (uses static string)
const char *GetFileNameWithoutExt(const char *filePath)
{
    #define MAX_FILENAMEWITHOUTEXT_LENGTH   128

    static char fileName[MAX_FILENAMEWITHOUTEXT_LENGTH] = { 0 };
    memset(fileName, 0, MAX_FILENAMEWITHOUTEXT_LENGTH);

    if (filePath != NULL) strcpy(fileName, GetFileName(filePath));   // Get filename with extension

    int size = (int)strlen(fileName);   // Get size in bytes

    for (int i = 0; (i < size) && (i < MAX_FILENAMEWITHOUTEXT_LENGTH); i++)
    {
        if (fileName[i] == '.')
        {
            // NOTE: We break on first '.' found
            fileName[i] = '\0';
            break;
        }
    }

    return fileName;
}

// Get directory for a given filePath
const char *GetDirectoryPath(const char *filePath)
{
/*
    // NOTE: Directory separator is different in Windows and other platforms,
    // fortunately, Windows also support the '/' separator, that's the one should be used
    #if defined(_WIN32)
        char separator = '\\';
    #else
        char separator = '/';
    #endif
*/
    const char *lastSlash = NULL;
    static char dirPath[MAX_FILEPATH_LENGTH] = { 0 };
    memset(dirPath, 0, MAX_FILEPATH_LENGTH);

    // In case provided path does not contain a root drive letter (C:\, D:\) nor leading path separator (\, /),
    // we add the current directory path to dirPath
    if (filePath[1] != ':' && filePath[0] != '\\' && filePath[0] != '/')
    {
        // For security, we set starting path to current directory,
        // obtained path will be concated to this
        dirPath[0] = '.';
        dirPath[1] = '/';
    }

    lastSlash = strprbrk(filePath, "\\/");
    if (lastSlash)
    {
        if (lastSlash == filePath)
        {
            // The last and only slash is the leading one: path is in a root directory
            dirPath[0] = filePath[0];
            dirPath[1] = '\0';
        }
        else
        {
            // NOTE: Be careful, strncpy() is not safe, it does not care about '\0'
            memcpy(dirPath + (filePath[1] != ':' && filePath[0] != '\\' && filePath[0] != '/' ? 2 : 0), filePath, strlen(filePath) - (strlen(lastSlash) - 1));
            dirPath[strlen(filePath) - strlen(lastSlash) + (filePath[1] != ':' && filePath[0] != '\\' && filePath[0] != '/' ? 2 : 0)] = '\0';  // Add '\0' manually
        }
    }

    return dirPath;
}

// Get previous directory path for a given path
const char *GetPrevDirectoryPath(const char *dirPath)
{
    static char prevDirPath[MAX_FILEPATH_LENGTH] = { 0 };
    memset(prevDirPath, 0, MAX_FILEPATH_LENGTH);
    int pathLen = (int)strlen(dirPath);

    if (pathLen <= 3) strcpy(prevDirPath, dirPath);

    for (int i = (pathLen - 1); (i >= 0) && (pathLen > 3); i--)
    {
        if ((dirPath[i] == '\\') || (dirPath[i] == '/'))
        {
            // Check for root: "C:\" or "/"
            if (((i == 2) && (dirPath[1] ==':')) || (i == 0)) i++;

            strncpy(prevDirPath, dirPath, i);
            break;
        }
    }

    return prevDirPath;
}

// Get current working directory
const char *GetWorkingDirectory(void)
{
    static char currentDir[MAX_FILEPATH_LENGTH] = { 0 };
    memset(currentDir, 0, MAX_FILEPATH_LENGTH);

    char *path = GETCWD(currentDir, MAX_FILEPATH_LENGTH - 1);

    return path;
}

const char *GetApplicationDirectory(void)
{
    static char appDir[MAX_FILEPATH_LENGTH] = { 0 };
    memset(appDir, 0, MAX_FILEPATH_LENGTH);

#if defined(_WIN32)
    int len = 0;
#if defined (UNICODE)
    unsigned short widePath[MAX_PATH];
    len = GetModuleFileNameW(NULL, widePath, MAX_PATH);
    len = WideCharToMultiByte(0, 0, widePath, len, appDir, MAX_PATH, NULL, NULL);
#else
    len = GetModuleFileNameA(NULL, appDir, MAX_PATH);
#endif
    if (len > 0)
    {
        for (int i = len; i >= 0; --i)
        {
            if (appDir[i] == '\\')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '\\';
    }

#elif defined(__linux__)
    unsigned int size = sizeof(appDir);
    ssize_t len = readlink("/proc/self/exe", appDir, size);

    if (len > 0)
    {
        for (int i = len; i >= 0; --i)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }
#elif defined(__APPLE__)
    uint32_t size = sizeof(appDir);

    if (_NSGetExecutablePath(appDir, &size) == 0)
    {
        int len = strlen(appDir);
        for (int i = len; i >= 0; --i)
        {
            if (appDir[i] == '/')
            {
                appDir[i + 1] = '\0';
                break;
            }
        }
    }
    else
    {
        appDir[0] = '.';
        appDir[1] = '/';
    }
#endif

    return appDir;
}

// Get filenames in a directory path
// NOTE: Files count is returned by parameters pointer
char **GetDirectoryFiles(const char *dirPath, int *fileCount)
{
    ClearDirectoryFiles();

    int counter = 0;
    struct dirent *entity;
    DIR *dir = opendir(dirPath);

    if (dir != NULL) // It's a directory
    {
        // Count files
        while ((entity = readdir(dir)) != NULL) counter++;

        dirFileCount = counter;
        *fileCount = dirFileCount;

        // Memory allocation for dirFileCount
        dirFilesPath = (char **)RL_MALLOC(dirFileCount*sizeof(char *));
        for (int i = 0; i < dirFileCount; i++) dirFilesPath[i] = (char *)RL_MALLOC(MAX_FILEPATH_LENGTH*sizeof(char));

        // Reset our position in the directory to the beginning
        rewinddir(dir);

        // Read file names
        for (int i = 0; (entity = readdir(dir)) != NULL; i++) strcpy(dirFilesPath[i], entity->d_name);

        closedir(dir);
    }
    else TRACELOG(LOG_WARNING, "FILEIO: Failed to open requested directory");  // Maybe it's a file...

    return dirFilesPath;
}

// Clear directory files paths buffers
void ClearDirectoryFiles(void)
{
    if (dirFileCount > 0)
    {
        for (int i = 0; i < dirFileCount; i++) RL_FREE(dirFilesPath[i]);

        RL_FREE(dirFilesPath);
    }

    dirFileCount = 0;
}

// Change working directory, returns true on success
bool ChangeDirectory(const char *dir)
{
    bool result = CHDIR(dir);

    if (result != 0) TRACELOG(LOG_WARNING, "SYSTEM: Failed to change to directory: %s", dir);

    return (result == 0);
}

// Check if a file has been dropped into window
bool IsFileDropped(void)
{
    if (CORE.Window.dropFileCount > 0) return true;
    else return false;
}

// Get dropped files names
char **GetDroppedFiles(int *count)
{
    *count = CORE.Window.dropFileCount;
    return CORE.Window.dropFilesPath;
}

// Clear dropped files paths buffer
void ClearDroppedFiles(void)
{
    if (CORE.Window.dropFileCount > 0)
    {
        for (int i = 0; i < CORE.Window.dropFileCount; i++) RL_FREE(CORE.Window.dropFilesPath[i]);

        RL_FREE(CORE.Window.dropFilesPath);

        CORE.Window.dropFileCount = 0;
    }
}

// Get file modification time (last write time)
long GetFileModTime(const char *fileName)
{
    struct stat result = { 0 };

    if (stat(fileName, &result) == 0)
    {
        time_t mod = result.st_mtime;

        return (long)mod;
    }

    return 0;
}

long GetFileSize(const char *fileName)
{
    struct stat result = { 0 };

    if (stat(fileName, &result) == 0)
    {
        int mod = result.st_size;

        return (long)mod;
    }

    return 0;
}
// Compress data (DEFLATE algorythm)
unsigned char *CompressData(const unsigned char *data, int dataSize, int *compDataSize)
{
    #define COMPRESSION_QUALITY_DEFLATE  8

    unsigned char *compData = NULL;

#if defined(SUPPORT_COMPRESSION_API)
    // Compress data and generate a valid DEFLATE stream
    struct sdefl sdefl = { 0 };
    int bounds = sdefl_bound(dataSize);
    compData = (unsigned char *)RL_CALLOC(bounds, 1);
    *compDataSize = sdeflate(&sdefl, compData, data, dataSize, COMPRESSION_QUALITY_DEFLATE);   // Compression level 8, same as stbwi

    TraceLog(LOG_INFO, "SYSTEM: Compress data: Original size: %i -> Comp. size: %i", dataSize, *compDataSize);
#endif

    return compData;
}

// Decompress data (DEFLATE algorythm)
unsigned char *DecompressData(const unsigned char *compData, int compDataSize, int *dataSize)
{
    unsigned char *data = NULL;

#if defined(SUPPORT_COMPRESSION_API)
    // Decompress data from a valid DEFLATE stream
    data = RL_CALLOC(MAX_DECOMPRESSION_SIZE*1024*1024, 1);
    int length = sinflate(data, MAX_DECOMPRESSION_SIZE, compData, compDataSize);
    unsigned char *temp = RL_REALLOC(data, length);

    if (temp != NULL) data = temp;
    else TRACELOG(LOG_WARNING, "SYSTEM: Failed to re-allocate required decompression memory");

    *dataSize = length;

    TraceLog(LOG_INFO, "SYSTEM: Decompress data: Comp. size: %i -> Original size: %i", compDataSize, *dataSize);
#endif

    return data;
}

// Encode data to Base64 string
char *EncodeDataBase64(const unsigned char *data, int dataSize, int *outputSize)
{
    static const unsigned char base64encodeTable[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
    };

    static const int modTable[] = { 0, 2, 1 };

    *outputSize = 4*((dataSize + 2)/3);

    char *encodedData = RL_MALLOC(*outputSize);

    if (encodedData == NULL) return NULL;

    for (int i = 0, j = 0; i < dataSize;)
    {
        unsigned int octetA = (i < dataSize)? (unsigned char)data[i++] : 0;
        unsigned int octetB = (i < dataSize)? (unsigned char)data[i++] : 0;
        unsigned int octetC = (i < dataSize)? (unsigned char)data[i++] : 0;

        unsigned int triple = (octetA << 0x10) + (octetB << 0x08) + octetC;

        encodedData[j++] = base64encodeTable[(triple >> 3*6) & 0x3F];
        encodedData[j++] = base64encodeTable[(triple >> 2*6) & 0x3F];
        encodedData[j++] = base64encodeTable[(triple >> 1*6) & 0x3F];
        encodedData[j++] = base64encodeTable[(triple >> 0*6) & 0x3F];
    }

    for (int i = 0; i < modTable[dataSize%3]; i++) encodedData[*outputSize - 1 - i] = '=';  // Padding character

    return encodedData;
}

// Decode Base64 string data
unsigned char *DecodeDataBase64(const unsigned char *data, int *outputSize)
{
    static const unsigned char base64decodeTable[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 62, 0, 0, 0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
        11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0, 0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
        37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
    };

    // Get output size of Base64 input data
    int outSize = 0;
    for (int i = 0; data[4*i] != 0; i++)
    {
        if (data[4*i + 3] == '=')
        {
            if (data[4*i + 2] == '=') outSize += 1;
            else outSize += 2;
        }
        else outSize += 3;
    }

    // Allocate memory to store decoded Base64 data
    unsigned char *decodedData = (unsigned char *)RL_MALLOC(outSize);

    for (int i = 0; i < outSize/3; i++)
    {
        unsigned char a = base64decodeTable[(int)data[4*i]];
        unsigned char b = base64decodeTable[(int)data[4*i + 1]];
        unsigned char c = base64decodeTable[(int)data[4*i + 2]];
        unsigned char d = base64decodeTable[(int)data[4*i + 3]];

        decodedData[3*i] = (a << 2) | (b >> 4);
        decodedData[3*i + 1] = (b << 4) | (c >> 2);
        decodedData[3*i + 2] = (c << 6) | d;
    }

    if (outSize%3 == 1)
    {
        int n = outSize/3;
        unsigned char a = base64decodeTable[(int)data[4*n]];
        unsigned char b = base64decodeTable[(int)data[4*n + 1]];
        decodedData[outSize - 1] = (a << 2) | (b >> 4);
    }
    else if (outSize%3 == 2)
    {
        int n = outSize/3;
        unsigned char a = base64decodeTable[(int)data[4*n]];
        unsigned char b = base64decodeTable[(int)data[4*n + 1]];
        unsigned char c = base64decodeTable[(int)data[4*n + 2]];
        decodedData[outSize - 2] = (a << 2) | (b >> 4);
        decodedData[outSize - 1] = (b << 4) | (c >> 2);
    }

    *outputSize = outSize;
    return decodedData;
}

// Save integer value to storage file (to defined position)
// NOTE: Storage positions is directly related to file memory layout (4 bytes each integer)
bool SaveStorageValue(unsigned int position, int value)
{
    bool success = false;

#if defined(SUPPORT_DATA_STORAGE)
    char path[512] = { 0 };
    strcpy(path, TextFormat("%s/%s", CORE.Storage.basePath, STORAGE_DATA_FILE));

    unsigned int dataSize = 0;
    unsigned int newDataSize = 0;
    unsigned char *fileData = LoadFileData(path, &dataSize);
    unsigned char *newFileData = NULL;

    if (fileData != NULL)
    {
        if (dataSize <= (position*sizeof(int)))
        {
            // Increase data size up to position and store value
            newDataSize = (position + 1)*sizeof(int);
            newFileData = (unsigned char *)RL_REALLOC(fileData, newDataSize);

            if (newFileData != NULL)
            {
                // RL_REALLOC succeded
                int *dataPtr = (int *)newFileData;
                dataPtr[position] = value;
            }
            else
            {
                // RL_REALLOC failed
                TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to realloc data (%u), position in bytes (%u) bigger than actual file size", path, dataSize, position*sizeof(int));

                // We store the old size of the file
                newFileData = fileData;
                newDataSize = dataSize;
            }
        }
        else
        {
            // Store the old size of the file
            newFileData = fileData;
            newDataSize = dataSize;

            // Replace value on selected position
            int *dataPtr = (int *)newFileData;
            dataPtr[position] = value;
        }

        success = SaveFileData(path, newFileData, newDataSize);
        RL_FREE(newFileData);

        TRACELOG(LOG_INFO, "FILEIO: [%s] Saved storage value: %i", path, value);
    }
    else
    {
        TRACELOG(LOG_INFO, "FILEIO: [%s] File created successfully", path);

        dataSize = (position + 1)*sizeof(int);
        fileData = (unsigned char *)RL_MALLOC(dataSize);
        int *dataPtr = (int *)fileData;
        dataPtr[position] = value;

        success = SaveFileData(path, fileData, dataSize);
        UnloadFileData(fileData);

        TRACELOG(LOG_INFO, "FILEIO: [%s] Saved storage value: %i", path, value);
    }
#endif

    return success;
}

// Load integer value from storage file (from defined position)
// NOTE: If requested position could not be found, value 0 is returned
int LoadStorageValue(unsigned int position)
{
    int value = 0;

#if defined(SUPPORT_DATA_STORAGE)
    char path[512] = { 0 };
    strcpy(path, TextFormat("%s/%s", CORE.Storage.basePath, STORAGE_DATA_FILE));

    unsigned int dataSize = 0;
    unsigned char *fileData = LoadFileData(path, &dataSize);

    if (fileData != NULL)
    {
        if (dataSize < (position*4)) TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to find storage position: %i", path, position);
        else
        {
            int *dataPtr = (int *)fileData;
            value = dataPtr[position];
        }

        UnloadFileData(fileData);

        TRACELOG(LOG_INFO, "FILEIO: [%s] Loaded storage value: %i", path, value);
    }
#endif
    return value;
}

// Open URL with default system browser (if available)
// NOTE: This function is only safe to use if you control the URL given.
// A user could craft a malicious string performing another action.
// Only call this function yourself not with user input or make sure to check the string yourself.
// Ref: https://github.com/raysan5/raylib/issues/686
void OpenURL(const char *url)
{
    // Small security check trying to avoid (partially) malicious code...
    // sorry for the inconvenience when you hit this point...
    if (strchr(url, '\'') != NULL)
    {
        TRACELOG(LOG_WARNING, "SYSTEM: Provided URL is not valid");
    }
    else
    {
#if defined(PLATFORM_DESKTOP)
        char *cmd = (char *)RL_CALLOC(strlen(url) + 10, sizeof(char));
    #if defined(_WIN32)
        sprintf(cmd, "explorer \"%s\"", url);
    #endif
    #if defined(__linux__) || defined(__FreeBSD__)
        sprintf(cmd, "xdg-open '%s'", url); // Alternatives: firefox, x-www-browser
    #endif
    #if defined(__APPLE__)
        sprintf(cmd, "open '%s'", url);
    #endif
        int result = system(cmd);
        if (result == -1) TRACELOG(LOG_WARNING, "OpenURL() child process could not be created");
        RL_FREE(cmd);
#endif
#if defined(PLATFORM_WEB)
        emscripten_run_script(TextFormat("window.open('%s', '_blank')", url));
#endif
#if defined(PLATFORM_ANDROID)
        JNIEnv *env = NULL;
        JavaVM *vm = CORE.Android.app->activity->vm;
        (*vm)->AttachCurrentThread(vm, &env, NULL);

        jstring urlString = (*env)->NewStringUTF(env, url);
        jclass uriClass = (*env)->FindClass(env, "android/net/Uri");
        jmethodID uriParse = (*env)->GetStaticMethodID(env, uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
        jobject uri = (*env)->CallStaticObjectMethod(env, uriClass, uriParse, urlString);

        jclass intentClass = (*env)->FindClass(env, "android/content/Intent");
        jfieldID actionViewId = (*env)->GetStaticFieldID(env, intentClass, "ACTION_VIEW", "Ljava/lang/String;");
        jobject actionView = (*env)->GetStaticObjectField(env, intentClass, actionViewId);
        jmethodID newIntent = (*env)->GetMethodID(env, intentClass, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
        jobject intent = (*env)->AllocObject(env, intentClass);

        (*env)->CallVoidMethod(env, intent, newIntent, actionView, uri);
        jclass activityClass = (*env)->FindClass(env, "android/app/Activity");
        jmethodID startActivity = (*env)->GetMethodID(env, activityClass, "startActivity", "(Landroid/content/Intent;)V");
        (*env)->CallVoidMethod(env, CORE.Android.app->activity->clazz, startActivity, intent);

        (*vm)->DetachCurrentThread(vm);
#endif
    }
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Input (Keyboard, Mouse, Gamepad) Functions
//----------------------------------------------------------------------------------
// Check if a key has been pressed once
bool IsKeyPressed(int key)
{
    bool pressed = false;

    if ((CORE.Input.Keyboard.previousKeyState[key] == 0) && (CORE.Input.Keyboard.currentKeyState[key] == 1)) pressed = true;

    return pressed;
}

// Check if a key is being pressed (key held down)
bool IsKeyDown(int key)
{
    if (CORE.Input.Keyboard.currentKeyState[key] == 1) return true;
    else return false;
}

// Check if a key has been released once
bool IsKeyReleased(int key)
{
    bool released = false;

    if ((CORE.Input.Keyboard.previousKeyState[key] == 1) && (CORE.Input.Keyboard.currentKeyState[key] == 0)) released = true;

    return released;
}

// Check if a key is NOT being pressed (key not held down)
bool IsKeyUp(int key)
{
    if (CORE.Input.Keyboard.currentKeyState[key] == 0) return true;
    else return false;
}

// Get the last key pressed
int GetKeyPressed(void)
{
    int value = 0;

    if (CORE.Input.Keyboard.keyPressedQueueCount > 0)
    {
        // Get character from the queue head
        value = CORE.Input.Keyboard.keyPressedQueue[0];

        // Shift elements 1 step toward the head.
        for (int i = 0; i < (CORE.Input.Keyboard.keyPressedQueueCount - 1); i++)
            CORE.Input.Keyboard.keyPressedQueue[i] = CORE.Input.Keyboard.keyPressedQueue[i + 1];

        // Reset last character in the queue
        CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount] = 0;
        CORE.Input.Keyboard.keyPressedQueueCount--;
    }

    return value;
}

// Get the last char pressed
int GetCharPressed(void)
{
    int value = 0;

    if (CORE.Input.Keyboard.charPressedQueueCount > 0)
    {
        // Get character from the queue head
        value = CORE.Input.Keyboard.charPressedQueue[0];

        // Shift elements 1 step toward the head.
        for (int i = 0; i < (CORE.Input.Keyboard.charPressedQueueCount - 1); i++)
            CORE.Input.Keyboard.charPressedQueue[i] = CORE.Input.Keyboard.charPressedQueue[i + 1];

        // Reset last character in the queue
        CORE.Input.Keyboard.charPressedQueue[CORE.Input.Keyboard.charPressedQueueCount] = 0;
        CORE.Input.Keyboard.charPressedQueueCount--;
    }

    return value;
}

// Set a custom key to exit program
// NOTE: default exitKey is ESCAPE
void SetExitKey(int key)
{
#if !defined(PLATFORM_ANDROID)
    CORE.Input.Keyboard.exitKey = key;
#endif
}

// NOTE: Gamepad support not implemented in emscripten GLFW3 (PLATFORM_WEB)

// Check if a gamepad is available
bool IsGamepadAvailable(int gamepad)
{
    bool result = false;

    if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad]) result = true;

    return result;
}

// Get gamepad internal name id
const char *GetGamepadName(int gamepad)
{
/*
#if defined(PLATFORM_DESKTOP)
    if (CORE.Input.Gamepad.ready[gamepad]) return glfwGetJoystickName(gamepad);
    else return NULL;
#endif
#if defined(PLATFORM_RPI) || defined(PLATFORM_DRM)
    if (CORE.Input.Gamepad.ready[gamepad]) ioctl(CORE.Input.Gamepad.streamId[gamepad], JSIOCGNAME(64), &CORE.Input.Gamepad.name[gamepad]);
    return CORE.Input.Gamepad.name[gamepad];
#endif
#if defined(PLATFORM_WEB)
    return CORE.Input.Gamepad.name[gamepad];
#endif*/
    return NULL;
}

// Get gamepad axis count
int GetGamepadAxisCount(int gamepad)
{
#if defined(PLATFORM_RPI) || defined(PLATFORM_DRM)
    int axisCount = 0;
  //  if (CORE.Input.Gamepad.ready[gamepad]) ioctl(CORE.Input.Gamepad.streamId[gamepad], JSIOCGAXES, &axisCount);
    CORE.Input.Gamepad.axisCount = axisCount;
#endif

    return CORE.Input.Gamepad.axisCount;
}

// Get axis movement vector for a gamepad
float GetGamepadAxisMovement(int gamepad, int axis)
{
    float value = 0;

  //  if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (axis < MAX_GAMEPAD_AXIS) &&
  //      (fabsf(CORE.Input.Gamepad.axisState[gamepad][axis]) > 0.1f)) value = CORE.Input.Gamepad.axisState[gamepad][axis];      // 0.1f = GAMEPAD_AXIS_MINIMUM_DRIFT/DELTA

    return value;
}

// Check if a gamepad button has been pressed once
bool IsGamepadButtonPressed(int gamepad, int button)
{
    bool pressed = false;

   // if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button < MAX_GAMEPAD_BUTTONS) &&
   //     (CORE.Input.Gamepad.previousButtonState[gamepad][button] == 0) && (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 1)) pressed = true;

    return pressed;
}

// Check if a gamepad button is being pressed
bool IsGamepadButtonDown(int gamepad, int button)
{
    bool result = false;

   // if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button < MAX_GAMEPAD_BUTTONS) &&
    //    (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 1)) result = true;

    return result;
}

// Check if a gamepad button has NOT been pressed once
bool IsGamepadButtonReleased(int gamepad, int button)
{
    bool released = false;

   // if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button < MAX_GAMEPAD_BUTTONS) &&
  //      (CORE.Input.Gamepad.previousButtonState[gamepad][button] == 1) && (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 0)) released = true;

    return released;
}

// Check if a gamepad button is NOT being pressed
bool IsGamepadButtonUp(int gamepad, int button)
{
    bool result = false;

   // if ((gamepad < MAX_GAMEPADS) && CORE.Input.Gamepad.ready[gamepad] && (button < MAX_GAMEPAD_BUTTONS) &&
  //      (CORE.Input.Gamepad.currentButtonState[gamepad][button] == 0)) result = true;

    return result;
}

// Get the last gamepad button pressed
int GetGamepadButtonPressed(void)
{
    return CORE.Input.Gamepad.lastButtonPressed;
}

// Set internal gamepad mappings
int SetGamepadMappings(const char *mappings)
{
    int result = 0;

#if defined(PLATFORM_DESKTOP)
//    result = glfwUpdateGamepadMappings(mappings);
#endif

    return result;
}

// Check if a mouse button has been pressed once
bool IsMouseButtonPressed(int button)
{
    bool pressed = false;

    if ((CORE.Input.Mouse.currentButtonState[button] == 1) && (CORE.Input.Mouse.previousButtonState[button] == 0)) pressed = true;

    // Map touches to mouse buttons checking
    if ((CORE.Input.Touch.currentTouchState[button] == 1) && (CORE.Input.Touch.previousTouchState[button] == 0)) pressed = true;

    return pressed;
}

// Check if a mouse button is being pressed
bool IsMouseButtonDown(int button)
{
    bool down = false;

    if (CORE.Input.Mouse.currentButtonState[button] == 1) down = true;

    // Map touches to mouse buttons checking
    if (CORE.Input.Touch.currentTouchState[button] == 1) down = true;

    return down;
}

// Check if a mouse button has been released once
bool IsMouseButtonReleased(int button)
{
    bool released = false;

    if ((CORE.Input.Mouse.currentButtonState[button] == 0) && (CORE.Input.Mouse.previousButtonState[button] == 1)) released = true;

    // Map touches to mouse buttons checking
    if ((CORE.Input.Touch.currentTouchState[button] == 0) && (CORE.Input.Touch.previousTouchState[button] == 1)) released = true;

    return released;
}

// Check if a mouse button is NOT being pressed
bool IsMouseButtonUp(int button)
{
    return !IsMouseButtonDown(button);
}

// Get mouse position X
int GetMouseX(void)
{
#if defined(PLATFORM_ANDROID)
    return (int)CORE.Input.Touch.position[0].x;
#else
    return (int)((CORE.Input.Mouse.currentPosition.x + CORE.Input.Mouse.offset.x)*CORE.Input.Mouse.scale.x);
#endif
}

// Get mouse position Y
int GetMouseY(void)
{
#if defined(PLATFORM_ANDROID)
    return (int)CORE.Input.Touch.position[0].y;
#else
    return (int)((CORE.Input.Mouse.currentPosition.y + CORE.Input.Mouse.offset.y)*CORE.Input.Mouse.scale.y);
#endif
}

// Get mouse position XY
Vector2 GetMousePosition(void)
{
    Vector2 position = { 0 };

#if defined(PLATFORM_ANDROID) || defined(PLATFORM_WEB)
    position = GetTouchPosition(0);
#else
    position.x = (CORE.Input.Mouse.currentPosition.x + CORE.Input.Mouse.offset.x)*CORE.Input.Mouse.scale.x;
    position.y = (CORE.Input.Mouse.currentPosition.y + CORE.Input.Mouse.offset.y)*CORE.Input.Mouse.scale.y;
#endif

    return position;
}

// Get mouse delta between frames
Vector2 GetMouseDelta(void)
{
    Vector2 delta = {0};

    delta.x = CORE.Input.Mouse.currentPosition.x - CORE.Input.Mouse.previousPosition.x;
    delta.y = CORE.Input.Mouse.currentPosition.y - CORE.Input.Mouse.previousPosition.y;

    return delta;
}

// Set mouse position XY
void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
    // NOTE: emscripten not implemented
//    glfwSetCursorPos(CORE.Window.handle, CORE.Input.Mouse.currentPosition.x, CORE.Input.Mouse.currentPosition.y);
#endif
}

// Set mouse offset
// NOTE: Useful when rendering to different size targets
void SetMouseOffset(int offsetX, int offsetY)
{
    CORE.Input.Mouse.offset = (Vector2){ (float)offsetX, (float)offsetY };
}

// Set mouse scaling
// NOTE: Useful when rendering to different size targets
void SetMouseScale(float scaleX, float scaleY)
{
    CORE.Input.Mouse.scale = (Vector2){ scaleX, scaleY };
}

// Get mouse wheel movement Y
float GetMouseWheelMove(void)
{
#if defined(PLATFORM_ANDROID)
    return 0.0f;
#endif
#if defined(PLATFORM_WEB)
    return CORE.Input.Mouse.currentWheelMove/100.0f;
#endif

    return CORE.Input.Mouse.currentWheelMove;
}

// Set mouse cursor
// NOTE: This is a no-op on platforms other than PLATFORM_DESKTOP
void SetMouseCursor(int cursor)
{
/*
#if defined(PLATFORM_DESKTOP)
    CORE.Input.Mouse.cursor = cursor;
    if (cursor == MOUSE_CURSOR_DEFAULT) glfwSetCursor(CORE.Window.handle, NULL);
    else
    {
        // NOTE: We are relating internal GLFW enum values to our MouseCursor enum values
        glfwSetCursor(CORE.Window.handle, glfwCreateStandardCursor(0x00036000 + cursor));
    }
#endif*/
}

// Get touch position X for touch point 0 (relative to screen size)
int GetTouchX(void)
{
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_WEB)
    return (int)CORE.Input.Touch.position[0].x;
#else   // PLATFORM_DESKTOP, PLATFORM_RPI, PLATFORM_DRM
    return GetMouseX();
#endif
}

// Get touch position Y for touch point 0 (relative to screen size)
int GetTouchY(void)
{
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_WEB)
    return (int)CORE.Input.Touch.position[0].y;
#else   // PLATFORM_DESKTOP, PLATFORM_RPI, PLATFORM_DRM
    return GetMouseY();
#endif
}

// Get touch position XY for a touch point index (relative to screen size)
// TODO: Touch position should be scaled depending on display size and render size
Vector2 GetTouchPosition(int index)
{
    Vector2 position = { -1.0f, -1.0f };

#if defined(PLATFORM_DESKTOP)
    // TODO: GLFW does not support multi-touch input just yet
    // https://www.codeproject.com/Articles/668404/Programming-for-Multi-Touch
    // https://docs.microsoft.com/en-us/windows/win32/wintouch/getting-started-with-multi-touch-messages
    if (index == 0) position = GetMousePosition();
#endif
#if defined(PLATFORM_ANDROID)
    if (index < MAX_TOUCH_POINTS) position = CORE.Input.Touch.position[index];
    else TRACELOG(LOG_WARNING, "INPUT: Required touch point out of range (Max touch points: %i)", MAX_TOUCH_POINTS);

    if ((CORE.Window.screen.width > CORE.Window.display.width) || (CORE.Window.screen.height > CORE.Window.display.height))
    {
        position.x = position.x*((float)CORE.Window.screen.width/(float)(CORE.Window.display.width - CORE.Window.renderOffset.x)) - CORE.Window.renderOffset.x/2;
        position.y = position.y*((float)CORE.Window.screen.height/(float)(CORE.Window.display.height - CORE.Window.renderOffset.y)) - CORE.Window.renderOffset.y/2;
    }
    else
    {
        position.x = position.x*((float)CORE.Window.render.width/(float)CORE.Window.display.width) - CORE.Window.renderOffset.x/2;
        position.y = position.y*((float)CORE.Window.render.height/(float)CORE.Window.display.height) - CORE.Window.renderOffset.y/2;
    }
#endif
#if defined(PLATFORM_WEB) || defined(PLATFORM_RPI) || defined(PLATFORM_DRM)
    if (index < MAX_TOUCH_POINTS) position = CORE.Input.Touch.position[index];
    else TRACELOG(LOG_WARNING, "INPUT: Required touch point out of range (Max touch points: %i)", MAX_TOUCH_POINTS);
#endif

    return position;
}

// Get touch point identifier for given index
int GetTouchPointId(int index)
{
    int id = -1;

    if (index < MAX_TOUCH_POINTS) id = CORE.Input.Touch.pointId[index];

    return id;
}

// Get number of touch points
int GetTouchPointCount(void)
{
    return CORE.Input.Touch.pointCount;
}

//----------------------------------------------------------------------------------
// Module specific Functions Definition
//----------------------------------------------------------------------------------

// Initialize display device and framebuffer
// NOTE: width and height represent the screen (framebuffer) desired size, not actual display size
// If width or height are 0, default display size will be used for framebuffer size
// NOTE: returns false in case graphic device could not be created
static bool InitGraphicsDevice(int width, int height)
{
    CORE.Window.screen.width = width;            // User desired width
    CORE.Window.screen.height = height;          // User desired height
    CORE.Window.screenScale = MatrixIdentity();  // No draw scaling required by default

    // NOTE: Framebuffer (render area - CORE.Window.render.width, CORE.Window.render.height) could include black bars...
    // ...in top-down or left-right to match display aspect ratio (no weird scalings)



 /*   if (!glfwInit())
    {
        TRACELOG(LOG_WARNING, "GLFW: Failed to initialize GLFW");
        return false;
    }
*/
    // NOTE: Getting video modes is not implemented in emscripten GLFW3 version
#if defined(PLATFORM_DESKTOP)
   /* // Find monitor resolution
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    if (!monitor)
    {
        TRACELOG(LOG_WARNING, "GLFW: Failed to get primary monitor");
        return false;
    }
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
*/
    CORE.Window.display.width = CORE.Window.screen.width;
    CORE.Window.display.height = CORE.Window.screen.height;

    // Set screen width/height to the display width/height if they are 0
    if (CORE.Window.screen.width == 0) CORE.Window.screen.width = CORE.Window.display.width;
    if (CORE.Window.screen.height == 0) CORE.Window.screen.height = CORE.Window.display.height;
#endif  // PLATFORM_DESKTOP

#if defined(PLATFORM_WEB)
    CORE.Window.display.width = CORE.Window.screen.width;
    CORE.Window.display.height = CORE.Window.screen.height;
#endif  // PLATFORM_WEB

  //  glfwDefaultWindowHints();                       // Set default windows hints
/*
    // Check window creation flags
    if ((CORE.Window.flags & FLAG_FULLSCREEN_MODE) > 0) CORE.Window.fullscreen = true;

    if ((CORE.Window.flags & FLAG_WINDOW_HIDDEN) > 0) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Visible window
    else glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);     // Window initially hidden

    if ((CORE.Window.flags & FLAG_WINDOW_UNDECORATED) > 0) glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // Border and buttons on Window
    else glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);   // Decorated window

    if ((CORE.Window.flags & FLAG_WINDOW_RESIZABLE) > 0) glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // Resizable window
    else glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // Avoid window being resizable

    // Disable FLAG_WINDOW_MINIMIZED, not supported on initialization
    if ((CORE.Window.flags & FLAG_WINDOW_MINIMIZED) > 0) CORE.Window.flags &= ~FLAG_WINDOW_MINIMIZED;

    // Disable FLAG_WINDOW_MAXIMIZED, not supported on initialization
    if ((CORE.Window.flags & FLAG_WINDOW_MAXIMIZED) > 0) CORE.Window.flags &= ~FLAG_WINDOW_MAXIMIZED;

    if ((CORE.Window.flags & FLAG_WINDOW_UNFOCUSED) > 0) glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    else glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);

    if ((CORE.Window.flags & FLAG_WINDOW_TOPMOST) > 0) glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    else glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);
*/
    // NOTE: Some GLFW flags are not supported on HTML5
#if defined(PLATFORM_DESKTOP)
/*    if ((CORE.Window.flags & FLAG_WINDOW_TRANSPARENT) > 0) glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);     // Transparent framebuffer
    else glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);  // Opaque framebuffer

    if ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0)
    {
        // Resize window content area based on the monitor content scale.
        // NOTE: This hint only has an effect on platforms where screen coordinates and pixels always map 1:1 such as Windows and X11.
        // On platforms like macOS the resolution of the framebuffer is changed independently of the window size.
    //    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);   // Scale content area based on the monitor content scale where window is placed on
    #if defined(__APPLE__)
     //   glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
    #endif
    }
   // else glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);*/
#endif

    if (CORE.Window.flags & FLAG_MSAA_4X_HINT)
    {
        // NOTE: MSAA is only enabled for main framebuffer, not user-created FBOs
        TRACELOG(LOG_INFO, "DISPLAY: Trying to enable MSAA x4");
     //   glfwWindowHint(GLFW_SAMPLES, 4);   // Tries to enable multisampling x4 (MSAA), default is 0
    }

    // NOTE: When asking for an OpenGL context version, most drivers provide highest supported version
    // with forward compatibility to older OpenGL versions.
    // For example, if using OpenGL 1.1, driver can provide a 4.3 context forward compatible.
/*
    // Check selection OpenGL version
    if (rlGetVersion() == OPENGL_21)
    {
       glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);          // Choose OpenGL major version (just hint)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);          // Choose OpenGL minor version (just hint)
    }
    else if (rlGetVersion() == OPENGL_33)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);          // Choose OpenGL major version (just hint)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);          // Choose OpenGL minor version (just hint)
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Profiles Hint: Only 3.3 and above!
                                                                       // Values: GLFW_OPENGL_CORE_PROFILE, GLFW_OPENGL_ANY_PROFILE, GLFW_OPENGL_COMPAT_PROFILE
#if defined(__APPLE__)
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);  // OSX Requires fordward compatibility
#else
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE); // Fordward Compatibility Hint: Only 3.3 and above!
#endif
        //glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE); // Request OpenGL DEBUG context
    }
    else if (rlGetVersion() == OPENGL_43)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);          // Choose OpenGL major version (just hint)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);          // Choose OpenGL minor version (just hint)
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
#if defined(RLGL_ENABLE_OPENGL_DEBUG_CONTEXT)
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);   // Enable OpenGL Debug Context
#endif
    }
    else if (rlGetVersion() == OPENGL_ES_20)                    // Request OpenGL ES 2.0 context
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#if defined(PLATFORM_DESKTOP)
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#else
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
#endif
    }
*/
#if defined(PLATFORM_DESKTOP)
    // NOTE: GLFW 3.4+ defers initialization of the Joystick subsystem on the first call to any Joystick related functions.
    // Forcing this initialization here avoids doing it on PollInputEvents() called by EndDrawing() after first frame has been just drawn.
    // The initialization will still happen and possible delays still occur, but before the window is shown, which is a nicer experience.
    // REF: https://github.com/raysan5/raylib/issues/1554
  //  if (MAX_GAMEPADS > 0) glfwSetJoystickCallback(NULL);
#endif
/*
    if (CORE.Window.fullscreen)
    {
        // remember center for switchinging from fullscreen to window
        CORE.Window.position.x = CORE.Window.display.width/2 - CORE.Window.screen.width/2;
        CORE.Window.position.y = CORE.Window.display.height/2 - CORE.Window.screen.height/2;

        if (CORE.Window.position.x < 0) CORE.Window.position.x = 0;
        if (CORE.Window.position.y < 0) CORE.Window.position.y = 0;

        // Obtain recommended CORE.Window.display.width/CORE.Window.display.height from a valid videomode for the monitor
        int count = 0;
        const GLFWvidmode *modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);

        // Get closest video mode to desired CORE.Window.screen.width/CORE.Window.screen.height
        for (int i = 0; i < count; i++)
        {
            if ((unsigned int)modes[i].width >= CORE.Window.screen.width)
            {
                if ((unsigned int)modes[i].height >= CORE.Window.screen.height)
                {
                    CORE.Window.display.width = modes[i].width;
                    CORE.Window.display.height = modes[i].height;
                    break;
                }
            }
        }

#if defined(PLATFORM_DESKTOP)
        // If we are windowed fullscreen, ensures that window does not minimize when focus is lost
        if ((CORE.Window.screen.height == CORE.Window.display.height) && (CORE.Window.screen.width == CORE.Window.display.width))
        {
            glfwWindowHint(GLFW_AUTO_ICONIFY, 0);
        }
#endif
        TRACELOG(LOG_WARNING, "SYSTEM: Closest fullscreen videomode: %i x %i", CORE.Window.display.width, CORE.Window.display.height);

        // NOTE: ISSUE: Closest videomode could not match monitor aspect-ratio, for example,
        // for a desired screen size of 800x450 (16:9), closest supported videomode is 800x600 (4:3),
        // framebuffer is rendered correctly but once displayed on a 16:9 monitor, it gets stretched
        // by the sides to fit all monitor space...

        // Try to setup the most appropiate fullscreen framebuffer for the requested screenWidth/screenHeight
        // It considers device display resolution mode and setups a framebuffer with black bars if required (render size/offset)
        // Modified global variables: CORE.Window.screen.width/CORE.Window.screen.height - CORE.Window.render.width/CORE.Window.render.height - CORE.Window.renderOffset.x/CORE.Window.renderOffset.y - CORE.Window.screenScale
        // TODO: It is a quite cumbersome solution to display size vs requested size, it should be reviewed or removed...
        // HighDPI monitors are properly considered in a following similar function: SetupViewport()
        SetupFramebuffer(CORE.Window.display.width, CORE.Window.display.height);

        CORE.Window.handle = glfwCreateWindow(CORE.Window.display.width, CORE.Window.display.height, (CORE.Window.title != 0)? CORE.Window.title : " ", glfwGetPrimaryMonitor(), NULL);

        // NOTE: Full-screen change, not working properly...
        //glfwSetWindowMonitor(CORE.Window.handle, glfwGetPrimaryMonitor(), 0, 0, CORE.Window.screen.width, CORE.Window.screen.height, GLFW_DONT_CARE);
    }
    else
    {
        // No-fullscreen window creation
        CORE.Window.handle = glfwCreateWindow(CORE.Window.screen.width, CORE.Window.screen.height, (CORE.Window.title != 0)? CORE.Window.title : " ", NULL, NULL);

        if (CORE.Window.handle)
        {
#if defined(PLATFORM_DESKTOP)
            // Center window on screen
            int windowPosX = CORE.Window.display.width/2 - CORE.Window.screen.width/2;
            int windowPosY = CORE.Window.display.height/2 - CORE.Window.screen.height/2;

            if (windowPosX < 0) windowPosX = 0;
            if (windowPosY < 0) windowPosY = 0;

            glfwSetWindowPos(CORE.Window.handle, windowPosX, windowPosY);
#endif
            CORE.Window.render.width = CORE.Window.screen.width;
            CORE.Window.render.height = CORE.Window.screen.height;
        }
    }

    if (!CORE.Window.handle)
    {
        glfwTerminate();
        TRACELOG(LOG_WARNING, "GLFW: Failed to initialize Window");
        return false;
    }

    // Set window callback events
    glfwSetWindowSizeCallback(CORE.Window.handle, WindowSizeCallback);      // NOTE: Resizing not allowed by default!
#if !defined(PLATFORM_WEB)
    glfwSetWindowMaximizeCallback(CORE.Window.handle, WindowMaximizeCallback);
#endif
    glfwSetWindowIconifyCallback(CORE.Window.handle, WindowIconifyCallback);
    glfwSetWindowFocusCallback(CORE.Window.handle, WindowFocusCallback);
    glfwSetDropCallback(CORE.Window.handle, WindowDropCallback);
    // Set input callback events
    glfwSetKeyCallback(CORE.Window.handle, KeyCallback);
    glfwSetCharCallback(CORE.Window.handle, CharCallback);
    glfwSetMouseButtonCallback(CORE.Window.handle, MouseButtonCallback);
    glfwSetCursorPosCallback(CORE.Window.handle, MouseCursorPosCallback);   // Track mouse position changes
    glfwSetScrollCallback(CORE.Window.handle, MouseScrollCallback);
    glfwSetCursorEnterCallback(CORE.Window.handle, CursorEnterCallback);

    glfwMakeContextCurrent(CORE.Window.handle);

#if !defined(PLATFORM_WEB)
    glfwSwapInterval(0);        // No V-Sync by default
#endif
*/
    // Try to enable GPU V-Sync, so frames are limited to screen refresh rate (60Hz -> 60 FPS)
    // NOTE: V-Sync can be enabled by graphic driver configuration
    if (CORE.Window.flags & FLAG_VSYNC_HINT)
    {
        // WARNING: It seems to hits a critical render path in Intel HD Graphics
     //   glfwSwapInterval(1);
        TRACELOG(LOG_INFO, "DISPLAY: Trying to enable VSYNC");
    }

    int fbWidth = CORE.Window.screen.width;
    int fbHeight = CORE.Window.screen.height;

#if defined(PLATFORM_DESKTOP)
  /*  if ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0)
    {
        // NOTE: On APPLE platforms system should manage window/input scaling and also framebuffer scaling
        // Framebuffer scaling should be activated with: glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
    #if !defined(__APPLE__)
        glfwGetFramebufferSize(CORE.Window.handle, &fbWidth, &fbHeight);

        // Screen scaling matrix is required in case desired screen area is different than display area
        CORE.Window.screenScale = MatrixScale((float)fbWidth/CORE.Window.screen.width, (float)fbHeight/CORE.Window.screen.height, 1.0f);

        // Mouse input scaling for the new screen size
        SetMouseScale((float)CORE.Window.screen.width/fbWidth, (float)CORE.Window.screen.height/fbHeight);
    #endif
    }*/


    CORE.Window.render.width = fbWidth;
    CORE.Window.render.height = fbHeight;
    CORE.Window.currentFbo.width = fbWidth;
    CORE.Window.currentFbo.height = fbHeight;

    TRACELOG(LOG_INFO, "DISPLAY: Device initialized successfully");
    TRACELOG(LOG_INFO, "    > Display size: %i x %i", CORE.Window.display.width, CORE.Window.display.height);
    TRACELOG(LOG_INFO, "    > Screen size:  %i x %i", CORE.Window.screen.width, CORE.Window.screen.height);
    TRACELOG(LOG_INFO, "    > Render size:  %i x %i", CORE.Window.render.width, CORE.Window.render.height);
    TRACELOG(LOG_INFO, "    > Viewport offsets: %i, %i", CORE.Window.renderOffset.x, CORE.Window.renderOffset.y);

#endif  // PLATFORM_DESKTOP || PLATFORM_WEB

/*
#if defined(PLATFORM_ANDROID)
    EGLint displayFormat = 0;

    // EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is guaranteed to be accepted by ANativeWindow_setBuffersGeometry()
    // As soon as we picked a EGLConfig, we can safely reconfigure the ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID
    eglGetConfigAttrib(CORE.Window.device, CORE.Window.config, EGL_NATIVE_VISUAL_ID, &displayFormat);

    // At this point we need to manage render size vs screen size
    // NOTE: This function use and modify global module variables:
    //  -> CORE.Window.screen.width/CORE.Window.screen.height
    //  -> CORE.Window.render.width/CORE.Window.render.height
    //  -> CORE.Window.screenScale
    SetupFramebuffer(CORE.Window.display.width, CORE.Window.display.height);

    ANativeWindow_setBuffersGeometry(CORE.Android.app->window, CORE.Window.render.width, CORE.Window.render.height, displayFormat);
    //ANativeWindow_setBuffersGeometry(CORE.Android.app->window, 0, 0, displayFormat);       // Force use of native display size

    CORE.Window.surface = eglCreateWindowSurface(CORE.Window.device, CORE.Window.config, CORE.Android.app->window, NULL);
#endif  // PLATFORM_ANDROID
*/

    // Load OpenGL extensions
    // NOTE: GL procedures address loader is required to load extensions
    //rlLoadExtensions(glfwGetProcAddress);
    rlLoadExtensions();

    // Initialize OpenGL context (states and resources)
    // NOTE: CORE.Window.currentFbo.width and CORE.Window.currentFbo.height not used, just stored as globals in rlgl
    rlglInit(CORE.Window.currentFbo.width, CORE.Window.currentFbo.height);

    // Setup default viewport
    // NOTE: It updated CORE.Window.render.width and CORE.Window.render.height
    SetupViewport(CORE.Window.currentFbo.width, CORE.Window.currentFbo.height);

    ClearBackground(RAYWHITE);      // Default background color for raylib games :P

#if defined(PLATFORM_ANDROID)
    CORE.Window.ready = true;
#endif

    if ((CORE.Window.flags & FLAG_WINDOW_MINIMIZED) > 0) MinimizeWindow();

    return true;
}

// Set viewport for a provided width and height
static void SetupViewport(int width, int height)
{
    CORE.Window.render.width = width;
    CORE.Window.render.height = height;

    // Set viewport width and height
    // NOTE: We consider render size (scaled) and offset in case black bars are required and
    // render area does not match full display area (this situation is only applicable on fullscreen mode)

    rlViewport(CORE.Window.renderOffset.x/2, CORE.Window.renderOffset.y/2, CORE.Window.render.width, CORE.Window.render.height);

    rlMatrixMode(RL_PROJECTION);        // Switch to projection matrix
    rlLoadIdentity();                   // Reset current matrix (projection)

    // Set orthographic projection to current framebuffer size
    // NOTE: Configured top-left corner as (0, 0)
    rlOrtho(0, CORE.Window.render.width, CORE.Window.render.height, 0, 0.0f, 1.0f);

    rlMatrixMode(RL_MODELVIEW);         // Switch back to modelview matrix
    rlLoadIdentity();                   // Reset current matrix (modelview)
}

// Compute framebuffer size relative to screen size and display size
// NOTE: Global variables CORE.Window.render.width/CORE.Window.render.height and CORE.Window.renderOffset.x/CORE.Window.renderOffset.y can be modified
static void SetupFramebuffer(int width, int height)
{
    // Calculate CORE.Window.render.width and CORE.Window.render.height, we have the display size (input params) and the desired screen size (global var)
    if ((CORE.Window.screen.width > CORE.Window.display.width) || (CORE.Window.screen.height > CORE.Window.display.height))
    {
        TRACELOG(LOG_WARNING, "DISPLAY: Downscaling required: Screen size (%ix%i) is bigger than display size (%ix%i)", CORE.Window.screen.width, CORE.Window.screen.height, CORE.Window.display.width, CORE.Window.display.height);

        // Downscaling to fit display with border-bars
        float widthRatio = (float)CORE.Window.display.width/(float)CORE.Window.screen.width;
        float heightRatio = (float)CORE.Window.display.height/(float)CORE.Window.screen.height;

        if (widthRatio <= heightRatio)
        {
            CORE.Window.render.width = CORE.Window.display.width;
            CORE.Window.render.height = (int)round((float)CORE.Window.screen.height*widthRatio);
            CORE.Window.renderOffset.x = 0;
            CORE.Window.renderOffset.y = (CORE.Window.display.height - CORE.Window.render.height);
        }
        else
        {
            CORE.Window.render.width = (int)round((float)CORE.Window.screen.width*heightRatio);
            CORE.Window.render.height = CORE.Window.display.height;
            CORE.Window.renderOffset.x = (CORE.Window.display.width - CORE.Window.render.width);
            CORE.Window.renderOffset.y = 0;
        }

        // Screen scaling required
        float scaleRatio = (float)CORE.Window.render.width/(float)CORE.Window.screen.width;
        CORE.Window.screenScale = MatrixScale(scaleRatio, scaleRatio, 1.0f);

        // NOTE: We render to full display resolution!
        // We just need to calculate above parameters for downscale matrix and offsets
        CORE.Window.render.width = CORE.Window.display.width;
        CORE.Window.render.height = CORE.Window.display.height;

        TRACELOG(LOG_WARNING, "DISPLAY: Downscale matrix generated, content will be rendered at (%ix%i)", CORE.Window.render.width, CORE.Window.render.height);
    }
    else if ((CORE.Window.screen.width < CORE.Window.display.width) || (CORE.Window.screen.height < CORE.Window.display.height))
    {
        // Required screen size is smaller than display size
        TRACELOG(LOG_INFO, "DISPLAY: Upscaling required: Screen size (%ix%i) smaller than display size (%ix%i)", CORE.Window.screen.width, CORE.Window.screen.height, CORE.Window.display.width, CORE.Window.display.height);

        if ((CORE.Window.screen.width == 0) || (CORE.Window.screen.height == 0))
        {
            CORE.Window.screen.width = CORE.Window.display.width;
            CORE.Window.screen.height = CORE.Window.display.height;
        }

        // Upscaling to fit display with border-bars
        float displayRatio = (float)CORE.Window.display.width/(float)CORE.Window.display.height;
        float screenRatio = (float)CORE.Window.screen.width/(float)CORE.Window.screen.height;

        if (displayRatio <= screenRatio)
        {
            CORE.Window.render.width = CORE.Window.screen.width;
            CORE.Window.render.height = (int)round((float)CORE.Window.screen.width/displayRatio);
            CORE.Window.renderOffset.x = 0;
            CORE.Window.renderOffset.y = (CORE.Window.render.height - CORE.Window.screen.height);
        }
        else
        {
            CORE.Window.render.width = (int)round((float)CORE.Window.screen.height*displayRatio);
            CORE.Window.render.height = CORE.Window.screen.height;
            CORE.Window.renderOffset.x = (CORE.Window.render.width - CORE.Window.screen.width);
            CORE.Window.renderOffset.y = 0;
        }
    }
    else
    {
        CORE.Window.render.width = CORE.Window.screen.width;
        CORE.Window.render.height = CORE.Window.screen.height;
        CORE.Window.renderOffset.x = 0;
        CORE.Window.renderOffset.y = 0;
    }
}

// Initialize hi-resolution timer
static void InitTimer(void)
{
// Setting a higher resolution can improve the accuracy of time-out intervals in wait functions.
// However, it can also reduce overall system performance, because the thread scheduler switches tasks more often.
// High resolutions can also prevent the CPU power management system from entering power-saving modes.
// Setting a higher resolution does not improve the accuracy of the high-resolution performance counter.
#if defined(_WIN32) && defined(SUPPORT_WINMM_HIGHRES_TIMER) && !defined(SUPPORT_BUSY_WAIT_LOOP)
    timeBeginPeriod(1);                 // Setup high-resolution timer to 1ms (granularity of 1-2 ms)
#endif

#if defined(PLATFORM_ANDROID) || defined(PLATFORM_RPI) || defined(PLATFORM_DRM)
    struct timespec now = { 0 };

    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0)  // Success
    {
        CORE.Time.base = (unsigned long long int)now.tv_sec*1000000000LLU + (unsigned long long int)now.tv_nsec;
    }
    else TRACELOG(LOG_WARNING, "TIMER: Hi-resolution timer not available");
#endif

    CORE.Time.previous = GetTime();     // Get time as double
}

// Wait for some milliseconds (stop program execution)
// NOTE: Sleep() granularity could be around 10 ms, it means, Sleep() could
// take longer than expected... for that reason we use the busy wait loop
// Ref: http://stackoverflow.com/questions/43057578/c-programming-win32-games-sleep-taking-longer-than-expected
// Ref: http://www.geisswerks.com/ryan/FAQS/timing.html --> All about timming on Win32!
void WaitTime(float ms)
{
#if defined(SUPPORT_BUSY_WAIT_LOOP)
    double previousTime = GetTime();
    double currentTime = 0.0;

    // Busy wait loop
    while ((currentTime - previousTime) < ms/1000.0f) currentTime = GetTime();
#else
    #if defined(SUPPORT_PARTIALBUSY_WAIT_LOOP)
        double busyWait = ms*0.05;     // NOTE: We are using a busy wait of 5% of the time
        ms -= (float)busyWait;
    #endif

    // System halt functions
    #if defined(_WIN32)
        Sleep((unsigned int)ms);
    #endif
    #if defined(__linux__) || defined(__FreeBSD__) || defined(__EMSCRIPTEN__)
        struct timespec req = { 0 };
        time_t sec = (int)(ms/1000.0f);
        ms -= (sec*1000);
        req.tv_sec = sec;
        req.tv_nsec = ms*1000000L;

        // NOTE: Use nanosleep() on Unix platforms... usleep() it's deprecated.
        while (nanosleep(&req, &req) == -1) continue;
    #endif
    #if defined(__APPLE__)
        usleep(ms*1000.0f);
    #endif

    #if defined(SUPPORT_PARTIALBUSY_WAIT_LOOP)
        double previousTime = GetTime();
        double currentTime = 0.0;

        // Partial busy wait loop (only a fraction of the total wait time)
        while ((currentTime - previousTime) < busyWait/1000.0f) currentTime = GetTime();
    #endif
#endif
}

// Swap back buffer with front buffer (screen drawing)
void SwapScreenBuffer(void)
{
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
//    glfwSwapBuffers(CORE.Window.handle);
#endif

}

// Register all input events
void PollInputEvents(void)
{
#if defined(SUPPORT_GESTURES_SYSTEM)
    // NOTE: Gestures update must be called every frame to reset gestures correctly
    // because ProcessGestureEvent() is just called on an event, not every frame
    UpdateGestures();
#endif

    // Reset keys/chars pressed registered
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;



#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
    // Keyboard/Mouse input polling (automatically managed by GLFW3 through callback)

    // Register previous keys states
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];

    // Register previous mouse states
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) CORE.Input.Mouse.previousButtonState[i] = CORE.Input.Mouse.currentButtonState[i];

    // Register previous mouse wheel state
    CORE.Input.Mouse.previousWheelMove = CORE.Input.Mouse.currentWheelMove;
    CORE.Input.Mouse.currentWheelMove = 0.0f;

    // Register previous mouse position
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;
#endif

    // Register previous touch states
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];

    // Reset touch positions
    // TODO: It resets on PLATFORM_WEB the mouse position and not filled again until a move-event,
    // so, if mouse is not moved it returns a (0, 0) position... this behaviour should be reviewed!
    //for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.position[i] = (Vector2){ 0, 0 };

#if defined(PLATFORM_DESKTOP)
    // Check if gamepads are ready
    // NOTE: We do it here in case of disconnection
    for (int i = 0; i < MAX_GAMEPADS; i++)
    {
//        if (glfwJoystickPresent(i)) CORE.Input.Gamepad.ready[i] = true;
//        else CORE.Input.Gamepad.ready[i] = false;
    }
/*
    // Register gamepads buttons events
    for (int i = 0; i < MAX_GAMEPADS; i++)
    {
        if (CORE.Input.Gamepad.ready[i])     // Check if gamepad is available
        {
            // Register previous gamepad states
            for (int k = 0; k < MAX_GAMEPAD_BUTTONS; k++) CORE.Input.Gamepad.previousButtonState[i][k] = CORE.Input.Gamepad.currentButtonState[i][k];

            // Get current gamepad state
            // NOTE: There is no callback available, so we get it manually
            // Get remapped buttons
            GLFWgamepadstate state = { 0 };
            glfwGetGamepadState(i, &state); // This remapps all gamepads so they have their buttons mapped like an xbox controller

            const unsigned char *buttons = state.buttons;

            for (int k = 0; (buttons != NULL) && (k < GLFW_GAMEPAD_BUTTON_DPAD_LEFT + 1) && (k < MAX_GAMEPAD_BUTTONS); k++)
            {
                GamepadButton button = -1;

                switch (k)
                {
                    case GLFW_GAMEPAD_BUTTON_Y: button = GAMEPAD_BUTTON_RIGHT_FACE_UP; break;
                    case GLFW_GAMEPAD_BUTTON_B: button = GAMEPAD_BUTTON_RIGHT_FACE_RIGHT; break;
                    case GLFW_GAMEPAD_BUTTON_A: button = GAMEPAD_BUTTON_RIGHT_FACE_DOWN; break;
                    case GLFW_GAMEPAD_BUTTON_X: button = GAMEPAD_BUTTON_RIGHT_FACE_LEFT; break;

                    case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: button = GAMEPAD_BUTTON_LEFT_TRIGGER_1; break;
                    case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: button = GAMEPAD_BUTTON_RIGHT_TRIGGER_1; break;

                    case GLFW_GAMEPAD_BUTTON_BACK: button = GAMEPAD_BUTTON_MIDDLE_LEFT; break;
                    case GLFW_GAMEPAD_BUTTON_GUIDE: button = GAMEPAD_BUTTON_MIDDLE; break;
                    case GLFW_GAMEPAD_BUTTON_START: button = GAMEPAD_BUTTON_MIDDLE_RIGHT; break;

                    case GLFW_GAMEPAD_BUTTON_DPAD_UP: button = GAMEPAD_BUTTON_LEFT_FACE_UP; break;
                    case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: button = GAMEPAD_BUTTON_LEFT_FACE_RIGHT; break;
                    case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: button = GAMEPAD_BUTTON_LEFT_FACE_DOWN; break;
                    case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: button = GAMEPAD_BUTTON_LEFT_FACE_LEFT; break;

                    case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: button = GAMEPAD_BUTTON_LEFT_THUMB; break;
                    case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: button = GAMEPAD_BUTTON_RIGHT_THUMB; break;
                    default: break;
                }

                if (button != -1)   // Check for valid button
                {
                    if (buttons[k] == GLFW_PRESS)
                    {
                        CORE.Input.Gamepad.currentButtonState[i][button] = 1;
                        CORE.Input.Gamepad.lastButtonPressed = button;
                    }
                    else CORE.Input.Gamepad.currentButtonState[i][button] = 0;
                }
            }

            // Get current axis state
            const float *axes = state.axes;

            for (int k = 0; (axes != NULL) && (k < GLFW_GAMEPAD_AXIS_LAST + 1) && (k < MAX_GAMEPAD_AXIS); k++)
            {
                CORE.Input.Gamepad.axisState[i][k] = axes[k];
            }

            // Register buttons for 2nd triggers (because GLFW doesn't count these as buttons but rather axis)
            CORE.Input.Gamepad.currentButtonState[i][GAMEPAD_BUTTON_LEFT_TRIGGER_2] = (char)(CORE.Input.Gamepad.axisState[i][GAMEPAD_AXIS_LEFT_TRIGGER] > 0.1f);
            CORE.Input.Gamepad.currentButtonState[i][GAMEPAD_BUTTON_RIGHT_TRIGGER_2] = (char)(CORE.Input.Gamepad.axisState[i][GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.1f);

            CORE.Input.Gamepad.axisCount = GLFW_GAMEPAD_AXIS_LAST + 1;
        }
    }
*/
    CORE.Window.resizedLastFrame = false;

#if defined(SUPPORT_EVENTS_WAITING)
  //  glfwWaitEvents();
#else
   // glfwPollEvents();       // Register keyboard/mouse events (callbacks)... and window events!
#endif
#endif  // PLATFORM_DESKTOP

#if defined(PLATFORM_WEB)
    CORE.Window.resizedLastFrame = false;
#endif  // PLATFORM_WEB



}


// GLFW3 Error Callback, runs on GLFW3 error
static void ErrorCallback(int error, const char *description)
{
    TRACELOG(LOG_WARNING, "GLFW: Error: %i Description: %s", error, description);
}

// GLFW3 WindowSize Callback, runs when window is resizedLastFrame
// NOTE: Window resizing not allowed by default
static void WindowSizeCallback( int width, int height)
{
    // Reset viewport and projection matrix for new size
    SetupViewport(width, height);

    CORE.Window.currentFbo.width = width;
    CORE.Window.currentFbo.height = height;
    CORE.Window.resizedLastFrame = true;

    if (IsWindowFullscreen()) return;

    // Set current screen size
#if defined(__APPLE__)
    CORE.Window.screen.width = width;
    CORE.Window.screen.height = height;
#else
    if ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0)
    {
        Vector2 windowScaleDPI = GetWindowScaleDPI();

        CORE.Window.screen.width = (unsigned int)(width/windowScaleDPI.x);
        CORE.Window.screen.height = (unsigned int)(height/windowScaleDPI.y);
    }
    else
    {
        CORE.Window.screen.width = width;
        CORE.Window.screen.height = height;
    }
#endif

    // NOTE: Postprocessing texture is not scaled to new size
}

// GLFW3 WindowIconify Callback, runs when window is minimized/restored
static void WindowIconifyCallback( int iconified)
{
    if (iconified) CORE.Window.flags |= FLAG_WINDOW_MINIMIZED;  // The window was iconified
    else CORE.Window.flags &= ~FLAG_WINDOW_MINIMIZED;           // The window was restored
}

#if !defined(PLATFORM_WEB)
// GLFW3 WindowMaximize Callback, runs when window is maximized/restored
static void WindowMaximizeCallback( int maximized)
{
    if (maximized) CORE.Window.flags |= FLAG_WINDOW_MAXIMIZED;  // The window was maximized
    else CORE.Window.flags &= ~FLAG_WINDOW_MAXIMIZED;           // The window was restored
}
#endif

// GLFW3 WindowFocus Callback, runs when window get/lose focus
static void WindowFocusCallback( int focused)
{
    if (focused) CORE.Window.flags &= ~FLAG_WINDOW_UNFOCUSED;   // The window was focused
    else CORE.Window.flags |= FLAG_WINDOW_UNFOCUSED;            // The window lost focus
}

// GLFW3 Keyboard Callback, runs on key pressed
static void KeyCallback( int key, int scancode, int action, int mods)
{
    if (action == KEY_RELEASE) CORE.Input.Keyboard.currentKeyState[key] = 0;
    else CORE.Input.Keyboard.currentKeyState[key] = 1;

    // Check if there is space available in the key queue
    if ((CORE.Input.Keyboard.keyPressedQueueCount < MAX_KEY_PRESSED_QUEUE) && (action == KEY_PRESS))
    {
        // Add character to the queue
        CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount] = key;
        CORE.Input.Keyboard.keyPressedQueueCount++;
    }


    // Check the exit key to set close window
    if ((key == CORE.Input.Keyboard.exitKey) && (action == KEY_PRESS))
    {
        CORE.Window.shouldClose=true;
    }
    //glfwSetWindowShouldClose(CORE.Window.handle, GLFW_TRUE);

 //   SDL_Log(" %d %d %d",key,scancode,mods);

#if defined(SUPPORT_SCREEN_CAPTURE)
    if ((key == KEY_F12) && (action == KEY_PRESS))
    {
#if defined(SUPPORT_GIF_RECORDING)
        if (mods == KMOD_LCTRL)
        {
            if (gifRecording)
            {
                gifRecording = false;

                MsfGifResult result = msf_gif_end(&gifState);

                SaveFileData(TextFormat("%s/screenrec%03i.gif", CORE.Storage.basePath, screenshotCounter), result.data, (unsigned int)result.dataSize);
                msf_gif_free(result);

            #if defined(PLATFORM_WEB)
                // Download file from MEMFS (emscripten memory filesystem)
                // saveFileFromMEMFSToDisk() function is defined in raylib/templates/web_shel/shell.html
                emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", TextFormat("screenrec%03i.gif", screenshotCounter - 1), TextFormat("screenrec%03i.gif", screenshotCounter - 1)));
            #endif

                TRACELOG(LOG_INFO, "SYSTEM: Finish animated GIF recording");
            }
            else
            {
                gifRecording = true;
                gifFrameCounter = 0;

                msf_gif_begin(&gifState, CORE.Window.screen.width, CORE.Window.screen.height);
                screenshotCounter++;

                TRACELOG(LOG_INFO, "SYSTEM: Start animated GIF recording: %s", TextFormat("screenrec%03i.gif", screenshotCounter));
            }
        }
        else
#endif  // SUPPORT_GIF_RECORDING
        {
            TakeScreenshot(TextFormat("screenshot%03i.png", screenshotCounter));
            screenshotCounter++;
        }
    }
#endif  // SUPPORT_SCREEN_CAPTURE

#if defined(SUPPORT_EVENTS_AUTOMATION)
    if ((key == KEY_F11) && (action == 1))
    {
        eventsRecording = !eventsRecording;

        // On finish recording, we export events into a file
        if (!eventsRecording) ExportAutomationEvents("eventsrec.rep");
    }
    else if ((key == KEY_F9) && (action == 1))
    {
        LoadAutomationEvents("eventsrec.rep");
        eventsPlaying = true;

        TRACELOG(LOG_WARNING, "eventsPlaying enabled!");
    }
#endif*/
}

// GLFW3 Char Key Callback, runs on key down (gets equivalent unicode char value)
static void CharCallback( unsigned int key)
{
    //TRACELOG(LOG_DEBUG, "Char Callback: KEY:%i(%c)", key, key);

    // NOTE: Registers any key down considering OS keyboard layout but
    // do not detects action events, those should be managed by user...
    // Ref: https://github.com/glfw/glfw/issues/668#issuecomment-166794907
    // Ref: https://www.glfw.org/docs/latest/input_guide.html#input_char

    // Check if there is space available in the queue
    if (CORE.Input.Keyboard.charPressedQueueCount < MAX_KEY_PRESSED_QUEUE)
    {
        // Add character to the queue
        CORE.Input.Keyboard.charPressedQueue[CORE.Input.Keyboard.charPressedQueueCount] = key;
        CORE.Input.Keyboard.charPressedQueueCount++;
    }
}

// GLFW3 Mouse Button Callback, runs on mouse button pressed
static void MouseButtonCallback( int button, int action, int mods)
{
    // WARNING: GLFW could only return GLFW_PRESS (1) or GLFW_RELEASE (0) for now,
    // but future releases may add more actions (i.e. GLFW_REPEAT)
    CORE.Input.Mouse.currentButtonState[button] = action;

#if defined(SUPPORT_GESTURES_SYSTEM) && defined(SUPPORT_MOUSE_GESTURES)         // PLATFORM_DESKTOP
    // Process mouse events as touches to be able to use mouse-gestures
    GestureEvent gestureEvent = { 0 };

    // Register touch actions
    if ((CORE.Input.Mouse.currentButtonState[button] == 1) && (CORE.Input.Mouse.previousButtonState[button] == 0)) gestureEvent.touchAction = TOUCH_ACTION_DOWN;
    else if ((CORE.Input.Mouse.currentButtonState[button] == 0) && (CORE.Input.Mouse.previousButtonState[button] == 1)) gestureEvent.touchAction = TOUCH_ACTION_UP;

    // NOTE: TOUCH_ACTION_MOVE event is registered in MouseCursorPosCallback()

    // Assign a pointer ID
    gestureEvent.pointId[0] = 0;

    // Register touch points count
    gestureEvent.pointCount = 1;

    // Register touch points position, only one point registered
    gestureEvent.position[0] = GetMousePosition();

    // Normalize gestureEvent.position[0] for CORE.Window.screen.width and CORE.Window.screen.height
    gestureEvent.position[0].x /= (float)GetScreenWidth();
    gestureEvent.position[0].y /= (float)GetScreenHeight();

    // Gesture data is sent to gestures system for processing
    ProcessGestureEvent(gestureEvent);
#endif
}

// GLFW3 Cursor Position Callback, runs on mouse move
static void MouseCursorPosCallback( double x, double y)
{
    CORE.Input.Mouse.currentPosition.x = (float)x;
    CORE.Input.Mouse.currentPosition.y = (float)y;
    CORE.Input.Touch.position[0] = CORE.Input.Mouse.currentPosition;

#if defined(SUPPORT_GESTURES_SYSTEM) && defined(SUPPORT_MOUSE_GESTURES)         // PLATFORM_DESKTOP
    // Process mouse events as touches to be able to use mouse-gestures
    GestureEvent gestureEvent = { 0 };

    gestureEvent.touchAction = TOUCH_ACTION_MOVE;

    // Assign a pointer ID
    gestureEvent.pointId[0] = 0;

    // Register touch points count
    gestureEvent.pointCount = 1;

    // Register touch points position, only one point registered
    gestureEvent.position[0] = CORE.Input.Touch.position[0];

    // Normalize gestureEvent.position[0] for CORE.Window.screen.width and CORE.Window.screen.height
    gestureEvent.position[0].x /= (float)GetScreenWidth();
    gestureEvent.position[0].y /= (float)GetScreenHeight();

    // Gesture data is sent to gestures system for processing
    ProcessGestureEvent(gestureEvent);
#endif
}

// GLFW3 Srolling Callback, runs on mouse wheel
static void MouseScrollCallback( double xoffset, double yoffset)
{
    if (xoffset != 0.0) CORE.Input.Mouse.currentWheelMove = (float)xoffset;
    else CORE.Input.Mouse.currentWheelMove = (float)yoffset;
}

// GLFW3 CursorEnter Callback, when cursor enters the window
static void CursorEnterCallback( int enter)
{
 /*   if (enter == true) CORE.Input.Mouse.cursorOnScreen = true;
    else CORE.Input.Mouse.cursorOnScreen = false;*/
}

// GLFW3 Window Drop Callback, runs when drop files into window
// NOTE: Paths are stored in dynamic memory for further retrieval
// Everytime new files are dropped, old ones are discarded
static void WindowDropCallback( int count, const char **paths)
{
  /*  ClearDroppedFiles();

    CORE.Window.dropFilesPath = (char **)RL_MALLOC(count*sizeof(char *));

    for (int i = 0; i < count; i++)
    {
        CORE.Window.dropFilesPath[i] = (char *)RL_MALLOC(MAX_FILEPATH_LENGTH*sizeof(char));
        strcpy(CORE.Window.dropFilesPath[i], paths[i]);
    }

    CORE.Window.dropFileCount = count;*/
}

int get_sdl_to_glfw(int key)
{

switch(key)
	{
	    case  SDL_SCANCODE_SPACE:              return KEY_SPACE;
        case  SDL_SCANCODE_APOSTROPHE:         return KEY_APOSTROPHE;  /* ' */
        case  SDL_SCANCODE_COMMA :             return KEY_COMMA;  /* , */
        case  SDL_SCANCODE_MINUS  :            return KEY_MINUS;  /* - */
        case  SDL_SCANCODE_PERIOD:             return KEY_PERIOD;  /* . */
        case  SDL_SCANCODE_SLASH:              return KEY_SLASH;  /* / */
        case  SDL_SCANCODE_0   :               return KEY_ZERO;
        case  SDL_SCANCODE_1  :                return KEY_ONE;
        case  SDL_SCANCODE_2 :                 return KEY_TWO;
        case  SDL_SCANCODE_3 :                 return KEY_THREE;
        case  SDL_SCANCODE_4 :                 return KEY_FOUR;
        case  SDL_SCANCODE_5 :                 return KEY_FIVE;
        case  SDL_SCANCODE_6 :                 return KEY_SIX;
        case  SDL_SCANCODE_7 :                 return KEY_SEVEN;
        case  SDL_SCANCODE_8  :                return KEY_EIGHT;
        case  SDL_SCANCODE_9  :                return KEY_NINE;
        case  SDL_SCANCODE_SEMICOLON :         return KEY_SEMICOLON;  /* ; */
        case  SDL_SCANCODE_EQUALS:             return KEY_EQUAL;  /* = */
        case SDL_SCANCODE_A:	  return KEY_A;
        case SDL_SCANCODE_B:	  return KEY_B;
        case SDL_SCANCODE_C:	  return KEY_C;
        case SDL_SCANCODE_D:	  return KEY_D;
        case SDL_SCANCODE_E:	  return KEY_E;
        case SDL_SCANCODE_F:	  return KEY_F;
        case SDL_SCANCODE_G:	  return KEY_G;
        case SDL_SCANCODE_H:	  return KEY_H;
        case SDL_SCANCODE_I:	  return KEY_I;
        case SDL_SCANCODE_J:	  return KEY_J;
        case SDL_SCANCODE_K:	  return KEY_K;
        case SDL_SCANCODE_L:	  return KEY_L;
        case SDL_SCANCODE_M:	  return KEY_M;
        case SDL_SCANCODE_N:	  return KEY_N;
        case SDL_SCANCODE_O:	  return KEY_O;
        case SDL_SCANCODE_P:	  return KEY_P;
        case SDL_SCANCODE_Q:	  return KEY_Q;
        case SDL_SCANCODE_R:	  return KEY_R;
        case SDL_SCANCODE_S:	  return KEY_S;
        case SDL_SCANCODE_T:	  return KEY_T;
        case SDL_SCANCODE_U:	  return KEY_U;
        case SDL_SCANCODE_V:	  return KEY_V;
        case SDL_SCANCODE_W:	  return KEY_W;
        case SDL_SCANCODE_X:	  return KEY_X;
        case SDL_SCANCODE_Y:	  return KEY_Y;
        case SDL_SCANCODE_Z:	  return KEY_Z;
        case SDL_SCANCODE_LEFTBRACKET :      return KEY_LEFT_BRACKET;  /* [ */
        case SDL_SCANCODE_BACKSLASH :         return KEY_BACKSLASH;  /* \ */
        case SDL_SCANCODE_RIGHTBRACKET :     return KEY_RIGHT_BRACKET;  /* ] */
//        case SDL_SCANCODE_GRAVE :      return KEY_GRAVE_ACCENT;  /* ` */
	    case SDL_SCANCODE_ESCAPE:	  return KEY_ESCAPE;
		case SDL_SCANCODE_RETURN:	  return KEY_ENTER;
		case SDL_SCANCODE_TAB:	      return KEY_TAB;
		case SDL_SCANCODE_BACKSPACE :return KEY_BACKSPACE ;
        case SDL_SCANCODE_INSERT :return KEY_INSERT;
        case SDL_SCANCODE_DELETE :return KEY_DELETE;
        case SDL_SCANCODE_RIGHT           :return KEY_RIGHT;
        case SDL_SCANCODE_LEFT:return KEY_LEFT;
        case SDL_SCANCODE_DOWN:return KEY_DOWN;
        case SDL_SCANCODE_UP:return KEY_UP;
        case SDL_SCANCODE_PAGEUP :return KEY_PAGE_UP;
        case SDL_SCANCODE_PAGEDOWN  :return KEY_PAGE_DOWN;
        case SDL_SCANCODE_HOME:return KEY_HOME;
        case SDL_SCANCODE_END  :return KEY_END;
        case SDL_SCANCODE_CAPSLOCK   :return KEY_CAPS_LOCK;
        case SDL_SCANCODE_SCROLLLOCK :return KEY_SCROLL_LOCK;
        case SDL_SCANCODE_NUMLOCKCLEAR    :return KEY_NUM_LOCK;
        case SDL_SCANCODE_PRINTSCREEN  :return KEY_PRINT_SCREEN;
        case SDL_SCANCODE_PAUSE             :return KEY_PAUSE;
        case SDL_SCANCODE_F1                :return KEY_F1 ;
        case SDL_SCANCODE_F2                :return KEY_F2 ;
        case SDL_SCANCODE_F3                :return KEY_F3 ;
        case SDL_SCANCODE_F4                :return KEY_F4 ;
        case SDL_SCANCODE_F5                 :return KEY_F5 ;
        case SDL_SCANCODE_F6                :return KEY_F6 ;
        case SDL_SCANCODE_F7                 :return KEY_F7 ;
        case SDL_SCANCODE_F8                 :return KEY_F8 ;
        case SDL_SCANCODE_F9                 :return KEY_F9 ;
        case SDL_SCANCODE_F10                :return KEY_F10 ;
        case SDL_SCANCODE_F11                :return KEY_F11 ;
        case SDL_SCANCODE_F12                :return KEY_F12 ;
        case  SDL_SCANCODE_KP_0              :return KEY_KP_0 ;
        case  SDL_SCANCODE_KP_1               :return KEY_KP_1 ;
        case  SDL_SCANCODE_KP_2               :return KEY_KP_2 ;
        case  SDL_SCANCODE_KP_3               :return KEY_KP_3 ;
        case  SDL_SCANCODE_KP_4               :return KEY_KP_4 ;
        case  SDL_SCANCODE_KP_5               :return KEY_KP_5 ;
        case  SDL_SCANCODE_KP_6               :return KEY_KP_6 ;
        case  SDL_SCANCODE_KP_7               :return KEY_KP_7 ;
        case  SDL_SCANCODE_KP_8               :return KEY_KP_8 ;
        case  SDL_SCANCODE_KP_9               :return KEY_KP_9 ;
        case  SDL_SCANCODE_KP_DECIMAL         :return KEY_KP_DECIMAL ;
        case  SDL_SCANCODE_KP_DIVIDE          :return KEY_KP_DIVIDE ;
        case  SDL_SCANCODE_KP_MULTIPLY        :return KEY_KP_MULTIPLY ;
        case  SDL_SCANCODE_KP_MINUS        :return KEY_KP_SUBTRACT ;
        case  SDL_SCANCODE_KP_PLUS             :return KEY_KP_ADD ;
        case  SDL_SCANCODE_RETURN2           :return KEY_KP_ENTER ;
        case  SDL_SCANCODE_KP_EQUALS           :return KEY_KP_EQUAL ;
        case  SDL_SCANCODE_LSHIFT         :return KEY_LEFT_SHIFT ;
        case  SDL_SCANCODE_LCTRL       :return KEY_LEFT_CONTROL;
        case  SDL_SCANCODE_LALT           :return KEY_LEFT_ALT;
        case  SDL_SCANCODE_RSHIFT        :return KEY_RIGHT_SHIFT;
        case  SDL_SCANCODE_RCTRL      :return KEY_RIGHT_CONTROL;
        case  SDL_SCANCODE_RALT          :return KEY_RIGHT_ALT;
        case  SDL_SCANCODE_MENU               :return KEY_MENU;

		  default:                return 0;
	}


}

void ProcessInputEvents(const SDL_Event* e)
{




                    switch(e->type)
                    {
                      case SDL_QUIT:
                      {
                         CORE.Window.shouldClose=true;
                         break;
                       }
                        case SDL_WINDOWEVENT:
                        {
                           if (e->window.event == SDL_WINDOWEVENT_RESIZED)
                            {

                               // Input.size.width=e->window.data1;
                               // Input.size.height=e->window.data2;
                             break;
                            }

                            if ( e->window.event == SDL_WINDOWEVENT_CLOSE)
                            {
                            CORE.Window.shouldClose=true;
                             break;
                            }


                        }
                        break;
                       case SDL_KEYDOWN:
                       {
                            KeyCallback(get_sdl_to_glfw(e->key.keysym.scancode),e->key.keysym.sym,1,e->key.keysym.mod);

                       }
                       break;
                       case SDL_KEYUP:
                       {
                       KeyCallback(get_sdl_to_glfw(e->key.keysym.scancode),e->key.keysym.sym,0,e->key.keysym.mod);
                       }
                       break;
                       case SDL_TEXTINPUT:
                       {
                       }
                       break;
                        case SDL_MOUSEWHEEL:
                        {

                            MouseScrollCallback( e->wheel.y,e->wheel.x);
                        }
                        break;
                        case SDL_MOUSEBUTTONDOWN:
                        {


                         MouseButtonCallback(e->button.button,1,0);
                         MouseCursorPosCallback(e->button.x,e->button.y);


                        }
                        break;
                        case SDL_MOUSEBUTTONUP:
                        {
                         MouseButtonCallback(e->button.button,0,0);
                         MouseCursorPosCallback(e->button.x,e->button.y);


                        }
                        break;
                        case SDL_MOUSEMOTION:
                        {
                         MouseCursorPosCallback(e->button.x,e->button.y);
                         }
                        break;

                        case SDL_MULTIGESTURE:
                            {
                            }
                        break;
                        case SDL_FINGERDOWN:
                            {



                            }
                        break;
                        case SDL_FINGERUP:
                            {



                            }
                        break;
                        case SDL_FINGERMOTION:
                            {


                            }
                        break;
                   }



}


#if defined(SUPPORT_EVENTS_AUTOMATION)
// NOTE: Loading happens over AutomationEvent *events
// TODO: This system should probably be redesigned
static void LoadAutomationEvents(const char *fileName)
{
    //unsigned char fileId[4] = { 0 };

    // Load binary
    /*
    FILE *repFile = fopen(fileName, "rb");
    fread(fileId, 4, 1, repFile);

    if ((fileId[0] == 'r') && (fileId[1] == 'E') && (fileId[2] == 'P') && (fileId[1] == ' '))
    {
        fread(&eventCount, sizeof(int), 1, repFile);
        TraceLog(LOG_WARNING, "Events loaded: %i\n", eventCount);
        fread(events, sizeof(AutomationEvent), eventCount, repFile);
    }

    fclose(repFile);
    */

    // Load events (text file)
    FILE *repFile = fopen(fileName, "rt");

    if (repFile != NULL)
    {
        unsigned int count = 0;
        char buffer[256] = { 0 };

        fgets(buffer, 256, repFile);

        while (!feof(repFile))
        {
            if (buffer[0] == 'c') sscanf(buffer, "c %i", &eventCount);
            else if (buffer[0] == 'e')
            {
                sscanf(buffer, "e %d %d %d %d %d", &events[count].frame, &events[count].type,
                       &events[count].params[0], &events[count].params[1], &events[count].params[2]);

                count++;
            }

            fgets(buffer, 256, repFile);
        }

        if (count != eventCount) TRACELOG(LOG_WARNING, "Events count provided is different than count");

        fclose(repFile);
    }

    TRACELOG(LOG_WARNING, "Events loaded: %i", eventCount);
}

// Export recorded events into a file
static void ExportAutomationEvents(const char *fileName)
{
    unsigned char fileId[4] = "rEP ";

    // Save as binary
    /*
    FILE *repFile = fopen(fileName, "wb");
    fwrite(fileId, 4, 1, repFile);
    fwrite(&eventCount, sizeof(int), 1, repFile);
    fwrite(events, sizeof(AutomationEvent), eventCount, repFile);
    fclose(repFile);
    */

    // Export events as text
    FILE *repFile = fopen(fileName, "wt");

    if (repFile != NULL)
    {
        fprintf(repFile, "# Automation events list\n");
        fprintf(repFile, "#    c <events_count>\n");
        fprintf(repFile, "#    e <frame> <event_type> <param0> <param1> <param2> // <event_type_name>\n");

        fprintf(repFile, "c %i\n", eventCount);
        for (int i = 0; i < eventCount; i++)
        {
            fprintf(repFile, "e %i %i %i %i %i // %s\n", events[i].frame, events[i].type,
                    events[i].params[0], events[i].params[1], events[i].params[2], autoEventTypeName[events[i].type]);
        }

        fclose(repFile);
    }
}

// EndDrawing() -> After PollInputEvents()
// Check event in current frame and save into the events[i] array
static void RecordAutomationEvent(unsigned int frame)
{
    for (int key = 0; key < MAX_KEYBOARD_KEYS; key++)
    {
        // INPUT_KEY_UP (only saved once)
        if (CORE.Input.Keyboard.previousKeyState[key] && !CORE.Input.Keyboard.currentKeyState[key])
        {
            events[eventCount].frame = frame;
            events[eventCount].type = INPUT_KEY_UP;
            events[eventCount].params[0] = key;
            events[eventCount].params[1] = 0;
            events[eventCount].params[2] = 0;

            TRACELOG(LOG_INFO, "[%i] INPUT_KEY_UP: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
            eventCount++;
        }

        // INPUT_KEY_DOWN
        if (CORE.Input.Keyboard.currentKeyState[key])
        {
            events[eventCount].frame = frame;
            events[eventCount].type = INPUT_KEY_DOWN;
            events[eventCount].params[0] = key;
            events[eventCount].params[1] = 0;
            events[eventCount].params[2] = 0;

            TRACELOG(LOG_INFO, "[%i] INPUT_KEY_DOWN: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
            eventCount++;
        }
    }

    for (int button = 0; button < MAX_MOUSE_BUTTONS; button++)
    {
        // INPUT_MOUSE_BUTTON_UP
        if (CORE.Input.Mouse.previousButtonState[button] && !CORE.Input.Mouse.currentButtonState[button])
        {
            events[eventCount].frame = frame;
            events[eventCount].type = INPUT_MOUSE_BUTTON_UP;
            events[eventCount].params[0] = button;
            events[eventCount].params[1] = 0;
            events[eventCount].params[2] = 0;

            TRACELOG(LOG_INFO, "[%i] INPUT_MOUSE_BUTTON_UP: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
            eventCount++;
        }

        // INPUT_MOUSE_BUTTON_DOWN
        if (CORE.Input.Mouse.currentButtonState[button])
        {
            events[eventCount].frame = frame;
            events[eventCount].type = INPUT_MOUSE_BUTTON_DOWN;
            events[eventCount].params[0] = button;
            events[eventCount].params[1] = 0;
            events[eventCount].params[2] = 0;

            TRACELOG(LOG_INFO, "[%i] INPUT_MOUSE_BUTTON_DOWN: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
            eventCount++;
        }
    }

    // INPUT_MOUSE_POSITION (only saved if changed)
    if (((int)CORE.Input.Mouse.currentPosition.x != (int)CORE.Input.Mouse.previousPosition.x) ||
        ((int)CORE.Input.Mouse.currentPosition.y != (int)CORE.Input.Mouse.previousPosition.y))
    {
        events[eventCount].frame = frame;
        events[eventCount].type = INPUT_MOUSE_POSITION;
        events[eventCount].params[0] = (int)CORE.Input.Mouse.currentPosition.x;
        events[eventCount].params[1] = (int)CORE.Input.Mouse.currentPosition.y;
        events[eventCount].params[2] = 0;

        TRACELOG(LOG_INFO, "[%i] INPUT_MOUSE_POSITION: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
        eventCount++;
    }

    // INPUT_MOUSE_WHEEL_MOTION
    if ((int)CORE.Input.Mouse.currentWheelMove != (int)CORE.Input.Mouse.previousWheelMove)
    {
        events[eventCount].frame = frame;
        events[eventCount].type = INPUT_MOUSE_WHEEL_MOTION;
        events[eventCount].params[0] = (int)CORE.Input.Mouse.currentWheelMove;
        events[eventCount].params[1] = 0;
        events[eventCount].params[2] = 0;

        TRACELOG(LOG_INFO, "[%i] INPUT_MOUSE_WHEEL_MOTION: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
        eventCount++;
    }

    for (int id = 0; id < MAX_TOUCH_POINTS; id++)
    {
        // INPUT_TOUCH_UP
        if (CORE.Input.Touch.previousTouchState[id] && !CORE.Input.Touch.currentTouchState[id])
        {
            events[eventCount].frame = frame;
            events[eventCount].type = INPUT_TOUCH_UP;
            events[eventCount].params[0] = id;
            events[eventCount].params[1] = 0;
            events[eventCount].params[2] = 0;

            TRACELOG(LOG_INFO, "[%i] INPUT_TOUCH_UP: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
            eventCount++;
        }

        // INPUT_TOUCH_DOWN
        if (CORE.Input.Touch.currentTouchState[id])
        {
            events[eventCount].frame = frame;
            events[eventCount].type = INPUT_TOUCH_DOWN;
            events[eventCount].params[0] = id;
            events[eventCount].params[1] = 0;
            events[eventCount].params[2] = 0;

            TRACELOG(LOG_INFO, "[%i] INPUT_TOUCH_DOWN: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
            eventCount++;
        }

        // INPUT_TOUCH_POSITION
        // TODO: It requires the id!
        /*
        if (((int)CORE.Input.Touch.currentPosition[id].x != (int)CORE.Input.Touch.previousPosition[id].x) ||
            ((int)CORE.Input.Touch.currentPosition[id].y != (int)CORE.Input.Touch.previousPosition[id].y))
        {
            events[eventCount].frame = frame;
            events[eventCount].type = INPUT_TOUCH_POSITION;
            events[eventCount].params[0] = id;
            events[eventCount].params[1] = (int)CORE.Input.Touch.currentPosition[id].x;
            events[eventCount].params[2] = (int)CORE.Input.Touch.currentPosition[id].y;

            TRACELOG(LOG_INFO, "[%i] INPUT_TOUCH_POSITION: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
            eventCount++;
        }
        */
    }

    for (int gamepad = 0; gamepad < MAX_GAMEPADS; gamepad++)
    {
        // INPUT_GAMEPAD_CONNECT
        /*
        if ((CORE.Input.Gamepad.currentState[gamepad] != CORE.Input.Gamepad.previousState[gamepad]) &&
            (CORE.Input.Gamepad.currentState[gamepad] == true)) // Check if changed to ready
        {
            // TODO: Save gamepad connect event
        }
        */

        // INPUT_GAMEPAD_DISCONNECT
        /*
        if ((CORE.Input.Gamepad.currentState[gamepad] != CORE.Input.Gamepad.previousState[gamepad]) &&
            (CORE.Input.Gamepad.currentState[gamepad] == false)) // Check if changed to not-ready
        {
            // TODO: Save gamepad disconnect event
        }
        */

        for (int button = 0; button < MAX_GAMEPAD_BUTTONS; button++)
        {
            // INPUT_GAMEPAD_BUTTON_UP
            if (CORE.Input.Gamepad.previousButtonState[gamepad][button] && !CORE.Input.Gamepad.currentButtonState[gamepad][button])
            {
                events[eventCount].frame = frame;
                events[eventCount].type = INPUT_GAMEPAD_BUTTON_UP;
                events[eventCount].params[0] = gamepad;
                events[eventCount].params[1] = button;
                events[eventCount].params[2] = 0;

                TRACELOG(LOG_INFO, "[%i] INPUT_GAMEPAD_BUTTON_UP: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
                eventCount++;
            }

            // INPUT_GAMEPAD_BUTTON_DOWN
            if (CORE.Input.Gamepad.currentButtonState[gamepad][button])
            {
                events[eventCount].frame = frame;
                events[eventCount].type = INPUT_GAMEPAD_BUTTON_DOWN;
                events[eventCount].params[0] = gamepad;
                events[eventCount].params[1] = button;
                events[eventCount].params[2] = 0;

                TRACELOG(LOG_INFO, "[%i] INPUT_GAMEPAD_BUTTON_DOWN: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
                eventCount++;
            }
        }

        for (int axis = 0; axis < MAX_GAMEPAD_AXIS; axis++)
        {
            // INPUT_GAMEPAD_AXIS_MOTION
            if (CORE.Input.Gamepad.axisState[gamepad][axis] > 0.1f)
            {
                events[eventCount].frame = frame;
                events[eventCount].type = INPUT_GAMEPAD_AXIS_MOTION;
                events[eventCount].params[0] = gamepad;
                events[eventCount].params[1] = axis;
                events[eventCount].params[2] = (int)(CORE.Input.Gamepad.axisState[gamepad][axis]*32768.0f);

                TRACELOG(LOG_INFO, "[%i] INPUT_GAMEPAD_AXIS_MOTION: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
                eventCount++;
            }
        }
    }

    // INPUT_GESTURE
    if (GESTURES.current != GESTURE_NONE)
    {
        events[eventCount].frame = frame;
        events[eventCount].type = INPUT_GESTURE;
        events[eventCount].params[0] = GESTURES.current;
        events[eventCount].params[1] = 0;
        events[eventCount].params[2] = 0;

        TRACELOG(LOG_INFO, "[%i] INPUT_GESTURE: %i, %i, %i", events[eventCount].frame, events[eventCount].params[0], events[eventCount].params[1], events[eventCount].params[2]);
        eventCount++;
    }
}

// Play automation event
static void PlayAutomationEvent(unsigned int frame)
{
    for (unsigned int i = 0; i < eventCount; i++)
    {
        if (events[i].frame == frame)
        {
            switch (events[i].type)
            {
                // Input events
                case INPUT_KEY_UP: CORE.Input.Keyboard.currentKeyState[events[i].params[0]] = false; break;             // param[0]: key
                case INPUT_KEY_DOWN: CORE.Input.Keyboard.currentKeyState[events[i].params[0]] = true; break;            // param[0]: key
                case INPUT_MOUSE_BUTTON_UP: CORE.Input.Mouse.currentButtonState[events[i].params[0]] = false; break;    // param[0]: key
                case INPUT_MOUSE_BUTTON_DOWN: CORE.Input.Mouse.currentButtonState[events[i].params[0]] = true; break;   // param[0]: key
                case INPUT_MOUSE_POSITION:      // param[0]: x, param[1]: y
                {
                    CORE.Input.Mouse.currentPosition.x = (float)events[i].params[0];
                    CORE.Input.Mouse.currentPosition.y = (float)events[i].params[1];
                } break;
                case INPUT_MOUSE_WHEEL_MOTION: CORE.Input.Mouse.currentWheelMove = (float)events[i].params[0]; break;   // param[0]: delta
                case INPUT_TOUCH_UP: CORE.Input.Touch.currentTouchState[events[i].params[0]] = false; break;            // param[0]: id
                case INPUT_TOUCH_DOWN: CORE.Input.Touch.currentTouchState[events[i].params[0]] = true; break;           // param[0]: id
                case INPUT_TOUCH_POSITION:      // param[0]: id, param[1]: x, param[2]: y
                {
                    CORE.Input.Touch.position[events[i].params[0]].x = (float)events[i].params[1];
                    CORE.Input.Touch.position[events[i].params[0]].y = (float)events[i].params[2];
                } break;
                case INPUT_GAMEPAD_CONNECT: CORE.Input.Gamepad.ready[events[i].params[0]] = true; break;                // param[0]: gamepad
                case INPUT_GAMEPAD_DISCONNECT: CORE.Input.Gamepad.ready[events[i].params[0]] = false; break;            // param[0]: gamepad
                case INPUT_GAMEPAD_BUTTON_UP: CORE.Input.Gamepad.currentButtonState[events[i].params[0]][events[i].params[1]] = false; break;    // param[0]: gamepad, param[1]: button
                case INPUT_GAMEPAD_BUTTON_DOWN: CORE.Input.Gamepad.currentButtonState[events[i].params[0]][events[i].params[1]] = true; break;   // param[0]: gamepad, param[1]: button
                case INPUT_GAMEPAD_AXIS_MOTION: // param[0]: gamepad, param[1]: axis, param[2]: delta
                {
                    CORE.Input.Gamepad.axisState[events[i].params[0]][events[i].params[1]] = ((float)events[i].params[2]/32768.0f);
                } break;
                case INPUT_GESTURE: GESTURES.current = events[i].params[0]; break;     // param[0]: gesture (enum Gesture) -> rgestures.h: GESTURES.current

                // Window events
                case WINDOW_CLOSE: CORE.Window.shouldClose = true; break;
                case WINDOW_MAXIMIZE: MaximizeWindow(); break;
                case WINDOW_MINIMIZE: MinimizeWindow(); break;
                case WINDOW_RESIZE: SetWindowSize(events[i].params[0], events[i].params[1]); break;

                // Custom events
                case ACTION_TAKE_SCREENSHOT:
                {
                    TakeScreenshot(TextFormat("screenshot%03i.png", screenshotCounter));
                    screenshotCounter++;
                } break;
                case ACTION_SETTARGETFPS: SetTargetFPS(events[i].params[0]); break;
                default: break;
            }
        }
    }
}
#endif



//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------

#define MAX_TRACELOG_MSG_LENGTH     128     // Max length of one trace-log message

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static int logTypeLevel = LOG_INFO;                 // Minimum log type level

static TraceLogCallback traceLog = NULL;            // TraceLog callback function pointer
static LoadFileDataCallback loadFileData = NULL;    // LoadFileData callback funtion pointer
static SaveFileDataCallback saveFileData = NULL;    // SaveFileText callback funtion pointer
static LoadFileTextCallback loadFileText = NULL;    // LoadFileText callback funtion pointer
static SaveFileTextCallback saveFileText = NULL;    // SaveFileText callback funtion pointer

//----------------------------------------------------------------------------------
// Functions to set internal callbacks
//----------------------------------------------------------------------------------
void SetTraceLogCallback(TraceLogCallback callback) { traceLog = callback; }              // Set custom trace log
void SetLoadFileDataCallback(LoadFileDataCallback callback) { loadFileData = callback; }  // Set custom file data loader
void SetSaveFileDataCallback(SaveFileDataCallback callback) { saveFileData = callback; }  // Set custom file data saver
void SetLoadFileTextCallback(LoadFileTextCallback callback) { loadFileText = callback; }  // Set custom file text loader
void SetSaveFileTextCallback(SaveFileTextCallback callback) { saveFileText = callback; }  // Set custom file text saver



//----------------------------------------------------------------------------------
// Module Functions Definition - Utilities
//----------------------------------------------------------------------------------

// Set the current threshold (minimum) log level
void SetTraceLogLevel(int logType) { logTypeLevel = logType; }

// Show trace log messages (LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_DEBUG)
void TraceLog(int logType, const char *text, ...)
{
#if defined(SUPPORT_TRACELOG)
    // Message has level below current threshold, don't emit
    if (logType < logTypeLevel) return;

    va_list args;
    va_start(args, text);

    if (traceLog)
    {
        traceLog(logType, text, args);
        va_end(args);
        return;
    }

    char buffer[MAX_TRACELOG_MSG_LENGTH] = { 0 };

    switch (logType)
    {
        case LOG_TRACE: strcpy(buffer, "TRACE: "); break;
        case LOG_DEBUG: strcpy(buffer, "DEBUG: "); break;
        case LOG_INFO: strcpy(buffer, "INFO: "); break;
        case LOG_WARNING: strcpy(buffer, "WARNING: "); break;
        case LOG_ERROR: strcpy(buffer, "ERROR: "); break;
        case LOG_FATAL: strcpy(buffer, "FATAL: "); break;
        default: break;
    }

    strcat(buffer, text);
    strcat(buffer, "\n");
    vprintf(buffer, args);


    va_end(args);

    if (logType == LOG_FATAL) exit(EXIT_FAILURE);  // If fatal logging, exit program

#endif  // SUPPORT_TRACELOG
}

// Internal memory allocator
// NOTE: Initializes to zero by default
void *MemAlloc(int size)
{
    void *ptr = RL_CALLOC(size, 1);
    return ptr;
}

// Internal memory reallocator
void *MemRealloc(void *ptr, int size)
{
    void *ret = RL_REALLOC(ptr, size);
    return ret;
}

// Internal memory free
void MemFree(void *ptr)
{
    RL_FREE(ptr);
}

// Load data from file into a buffer
unsigned char *LoadFileData(const char *fileName, unsigned int *bytesRead)
{
    unsigned char *data = NULL;
    *bytesRead = 0;

    if (fileName != NULL)
    {
        if (loadFileData)
        {
            data = loadFileData(fileName, bytesRead);
            return data;
        }
#if defined(SUPPORT_STANDARD_FILEIO)
        FILE *file = fopen(fileName, "rb");

        if (file != NULL)
        {
            // WARNING: On binary streams SEEK_END could not be found,
            // using fseek() and ftell() could not work in some (rare) cases
            fseek(file, 0, SEEK_END);
            int size = ftell(file);
            fseek(file, 0, SEEK_SET);

            if (size > 0)
            {
                data = (unsigned char *)RL_MALLOC(size*sizeof(unsigned char));

                // NOTE: fread() returns number of read elements instead of bytes, so we read [1 byte, size elements]
                unsigned int count = (unsigned int)fread(data, sizeof(unsigned char), size, file);
                *bytesRead = count;

                if (count != size) TRACELOG(LOG_WARNING, "FILEIO: [%s] File partially loaded", fileName);
                else TRACELOG(LOG_INFO, "FILEIO: [%s] File loaded successfully", fileName);
            }
            else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to read file", fileName);

            fclose(file);
        }
        else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to open file", fileName);
#else
    TRACELOG(LOG_WARNING, "FILEIO: Standard file io not supported, use custom file callback");
#endif
    }
    else TRACELOG(LOG_WARNING, "FILEIO: File name provided is not valid");

    return data;
}

// Unload file data allocated by LoadFileData()
void UnloadFileData(unsigned char *data)
{
    RL_FREE(data);
}

// Save data to file from buffer
bool SaveFileData(const char *fileName, void *data, unsigned int bytesToWrite)
{
    bool success = false;

    if (fileName != NULL)
    {
        if (saveFileData)
        {
            return saveFileData(fileName, data, bytesToWrite);
        }
#if defined(SUPPORT_STANDARD_FILEIO)
        FILE *file = fopen(fileName, "wb");

        if (file != NULL)
        {
            unsigned int count = (unsigned int)fwrite(data, sizeof(unsigned char), bytesToWrite, file);

            if (count == 0) TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to write file", fileName);
            else if (count != bytesToWrite) TRACELOG(LOG_WARNING, "FILEIO: [%s] File partially written", fileName);
            else TRACELOG(LOG_INFO, "FILEIO: [%s] File saved successfully", fileName);

            int result = fclose(file);
            if (result == 0) success = true;
        }
        else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to open file", fileName);
#else
    TRACELOG(LOG_WARNING, "FILEIO: Standard file io not supported, use custom file callback");
#endif
    }
    else TRACELOG(LOG_WARNING, "FILEIO: File name provided is not valid");

    return success;
}

// Load text data from file, returns a '\0' terminated string
// NOTE: text chars array should be freed manually
char *LoadFileText(const char *fileName)
{
    char *text = NULL;

    if (fileName != NULL)
    {
        if (loadFileText)
        {
            text = loadFileText(fileName);
            return text;
        }
#if defined(SUPPORT_STANDARD_FILEIO)
        FILE *file = fopen(fileName, "rt");

        if (file != NULL)
        {
            // WARNING: When reading a file as 'text' file,
            // text mode causes carriage return-linefeed translation...
            // ...but using fseek() should return correct byte-offset
            fseek(file, 0, SEEK_END);
            unsigned int size = (unsigned int)ftell(file);
            fseek(file, 0, SEEK_SET);

            if (size > 0)
            {
                text = (char *)RL_MALLOC((size + 1)*sizeof(char));
                unsigned int count = (unsigned int)fread(text, sizeof(char), size, file);

                // WARNING: \r\n is converted to \n on reading, so,
                // read bytes count gets reduced by the number of lines
                if (count < size) text = RL_REALLOC(text, count + 1);

                // Zero-terminate the string
                text[count] = '\0';

                TRACELOG(LOG_INFO, "FILEIO: [%s] Text file loaded successfully", fileName);
            }
            else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to read text file", fileName);

            fclose(file);
        }
        else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to open text file", fileName);
#else
    TRACELOG(LOG_WARNING, "FILEIO: Standard file io not supported, use custom file callback");
#endif
    }
    else TRACELOG(LOG_WARNING, "FILEIO: File name provided is not valid");

    return text;
}

// Unload file text data allocated by LoadFileText()
void UnloadFileText(char *text)
{
    RL_FREE(text);
}

// Save text data to file (write), string must be '\0' terminated
bool SaveFileText(const char *fileName, char *text)
{
    bool success = false;

    if (fileName != NULL)
    {
        if (saveFileText)
        {
            return saveFileText(fileName, text);
        }
#if defined(SUPPORT_STANDARD_FILEIO)
        FILE *file = fopen(fileName, "wt");

        if (file != NULL)
        {
            int count = fprintf(file, "%s", text);

            if (count < 0) TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to write text file", fileName);
            else TRACELOG(LOG_INFO, "FILEIO: [%s] Text file saved successfully", fileName);

            int result = fclose(file);
            if (result == 0) success = true;
        }
        else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to open text file", fileName);
#else
    TRACELOG(LOG_WARNING, "FILEIO: Standard file io not supported, use custom file callback");
#endif
    }
    else TRACELOG(LOG_WARNING, "FILEIO: File name provided is not valid");

    return success;
}

//*****************************************************************************************************************************************************************************************

#if defined(SUPPORT_FILEFORMAT_TTF)
    #define STB_RECT_PACK_IMPLEMENTATION
    #include "external/stb_rect_pack.h"     // Required for: ttf font rectangles packaging

    #define STBTT_STATIC
    #define STB_TRUETYPE_IMPLEMENTATION
    #include "external/stb_truetype.h"      // Required for: ttf font data reading
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef MAX_TEXT_BUFFER_LENGTH
    #define MAX_TEXT_BUFFER_LENGTH              1024        // Size of internal static buffers used on some functions:
                                                            // TextFormat(), TextSubtext(), TextToUpper(), TextToLower(), TextToPascal(), TextSplit()
#endif
#ifndef MAX_TEXT_UNICODE_CHARS
    #define MAX_TEXT_UNICODE_CHARS               512        // Maximum number of unicode codepoints: GetCodepoints()
#endif
#ifndef MAX_TEXTSPLIT_COUNT
    #define MAX_TEXTSPLIT_COUNT                  128        // Maximum number of substrings to split: TextSplit()
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Global variables
//----------------------------------------------------------------------------------
#if defined(SUPPORT_DEFAULT_FONT)
// Default font provided by raylib
// NOTE: Default font is loaded on InitWindow() and disposed on CloseWindow() [module: core]
static Font defaultFont = { 0 };
#endif

//----------------------------------------------------------------------------------
// Other Modules Functions Declaration (required by text)
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Module specific Functions Declaration
//----------------------------------------------------------------------------------
#if defined(SUPPORT_FILEFORMAT_FNT)
static Font LoadBMFont(const char *fileName);     // Load a BMFont file (AngelCode font file)
#endif

#if defined(SUPPORT_DEFAULT_FONT)
extern void LoadFontDefault(void);
extern void UnloadFontDefault(void);
#endif

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
#if defined(SUPPORT_DEFAULT_FONT)

// Load raylib default font
extern void LoadFontDefault(void)
{
    #define BIT_CHECK(a,b) ((a) & (1u << (b)))

    // NOTE: Using UTF-8 encoding table for Unicode U+0000..U+00FF Basic Latin + Latin-1 Supplement
    // Ref: http://www.utf8-chartable.de/unicode-utf8-table.pl

    defaultFont.glyphCount = 224;   // Number of chars included in our default font
    defaultFont.glyphPadding = 0;   // Characters padding

    // Default font is directly defined here (data generated from a sprite font image)
    // This way, we reconstruct Font without creating large global variables
    // This data is automatically allocated to Stack and automatically deallocated at the end of this function
    unsigned int defaultFontData[512] = {
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00200020, 0x0001b000, 0x00000000, 0x00000000, 0x8ef92520, 0x00020a00, 0x7dbe8000, 0x1f7df45f,
        0x4a2bf2a0, 0x0852091e, 0x41224000, 0x10041450, 0x2e292020, 0x08220812, 0x41222000, 0x10041450, 0x10f92020, 0x3efa084c, 0x7d22103c, 0x107df7de,
        0xe8a12020, 0x08220832, 0x05220800, 0x10450410, 0xa4a3f000, 0x08520832, 0x05220400, 0x10450410, 0xe2f92020, 0x0002085e, 0x7d3e0281, 0x107df41f,
        0x00200000, 0x8001b000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xc0000fbe, 0xfbf7e00f, 0x5fbf7e7d, 0x0050bee8, 0x440808a2, 0x0a142fe8, 0x50810285, 0x0050a048,
        0x49e428a2, 0x0a142828, 0x40810284, 0x0048a048, 0x10020fbe, 0x09f7ebaf, 0xd89f3e84, 0x0047a04f, 0x09e48822, 0x0a142aa1, 0x50810284, 0x0048a048,
        0x04082822, 0x0a142fa0, 0x50810285, 0x0050a248, 0x00008fbe, 0xfbf42021, 0x5f817e7d, 0x07d09ce8, 0x00008000, 0x00000fe0, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000c0180,
        0xdfbf4282, 0x0bfbf7ef, 0x42850505, 0x004804bf, 0x50a142c6, 0x08401428, 0x42852505, 0x00a808a0, 0x50a146aa, 0x08401428, 0x42852505, 0x00081090,
        0x5fa14a92, 0x0843f7e8, 0x7e792505, 0x00082088, 0x40a15282, 0x08420128, 0x40852489, 0x00084084, 0x40a16282, 0x0842022a, 0x40852451, 0x00088082,
        0xc0bf4282, 0xf843f42f, 0x7e85fc21, 0x3e0900bf, 0x00000000, 0x00000004, 0x00000000, 0x000c0180, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04000402, 0x41482000, 0x00000000, 0x00000800,
        0x04000404, 0x4100203c, 0x00000000, 0x00000800, 0xf7df7df0, 0x514bef85, 0xbefbefbe, 0x04513bef, 0x14414500, 0x494a2885, 0xa28a28aa, 0x04510820,
        0xf44145f0, 0x474a289d, 0xa28a28aa, 0x04510be0, 0x14414510, 0x494a2884, 0xa28a28aa, 0x02910a00, 0xf7df7df0, 0xd14a2f85, 0xbefbe8aa, 0x011f7be0,
        0x00000000, 0x00400804, 0x20080000, 0x00000000, 0x00000000, 0x00600f84, 0x20080000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0xac000000, 0x00000f01, 0x00000000, 0x00000000, 0x24000000, 0x00000f01, 0x00000000, 0x06000000, 0x24000000, 0x00000f01, 0x00000000, 0x09108000,
        0x24fa28a2, 0x00000f01, 0x00000000, 0x013e0000, 0x2242252a, 0x00000f52, 0x00000000, 0x038a8000, 0x2422222a, 0x00000f29, 0x00000000, 0x010a8000,
        0x2412252a, 0x00000f01, 0x00000000, 0x010a8000, 0x24fbe8be, 0x00000f01, 0x00000000, 0x0ebe8000, 0xac020000, 0x00000f01, 0x00000000, 0x00048000,
        0x0003e000, 0x00000f00, 0x00000000, 0x00008000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000038, 0x8443b80e, 0x00203a03,
        0x02bea080, 0xf0000020, 0xc452208a, 0x04202b02, 0xf8029122, 0x07f0003b, 0xe44b388e, 0x02203a02, 0x081e8a1c, 0x0411e92a, 0xf4420be0, 0x01248202,
        0xe8140414, 0x05d104ba, 0xe7c3b880, 0x00893a0a, 0x283c0e1c, 0x04500902, 0xc4400080, 0x00448002, 0xe8208422, 0x04500002, 0x80400000, 0x05200002,
        0x083e8e00, 0x04100002, 0x804003e0, 0x07000042, 0xf8008400, 0x07f00003, 0x80400000, 0x04000022, 0x00000000, 0x00000000, 0x80400000, 0x04000002,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00800702, 0x1848a0c2, 0x84010000, 0x02920921, 0x01042642, 0x00005121, 0x42023f7f, 0x00291002,
        0xefc01422, 0x7efdfbf7, 0xefdfa109, 0x03bbbbf7, 0x28440f12, 0x42850a14, 0x20408109, 0x01111010, 0x28440408, 0x42850a14, 0x2040817f, 0x01111010,
        0xefc78204, 0x7efdfbf7, 0xe7cf8109, 0x011111f3, 0x2850a932, 0x42850a14, 0x2040a109, 0x01111010, 0x2850b840, 0x42850a14, 0xefdfbf79, 0x03bbbbf7,
        0x001fa020, 0x00000000, 0x00001000, 0x00000000, 0x00002070, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x08022800, 0x00012283, 0x02430802, 0x01010001, 0x8404147c, 0x20000144, 0x80048404, 0x00823f08, 0xdfbf4284, 0x7e03f7ef, 0x142850a1, 0x0000210a,
        0x50a14684, 0x528a1428, 0x142850a1, 0x03efa17a, 0x50a14a9e, 0x52521428, 0x142850a1, 0x02081f4a, 0x50a15284, 0x4a221428, 0xf42850a1, 0x03efa14b,
        0x50a16284, 0x4a521428, 0x042850a1, 0x0228a17a, 0xdfbf427c, 0x7e8bf7ef, 0xf7efdfbf, 0x03efbd0b, 0x00000000, 0x04000000, 0x00000000, 0x00000008,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00200508, 0x00840400, 0x11458122, 0x00014210,
        0x00514294, 0x51420800, 0x20a22a94, 0x0050a508, 0x00200000, 0x00000000, 0x00050000, 0x08000000, 0xfefbefbe, 0xfbefbefb, 0xfbeb9114, 0x00fbefbe,
        0x20820820, 0x8a28a20a, 0x8a289114, 0x3e8a28a2, 0xfefbefbe, 0xfbefbe0b, 0x8a289114, 0x008a28a2, 0x228a28a2, 0x08208208, 0x8a289114, 0x088a28a2,
        0xfefbefbe, 0xfbefbefb, 0xfa2f9114, 0x00fbefbe, 0x00000000, 0x00000040, 0x00000000, 0x00000000, 0x00000000, 0x00000020, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00210100, 0x00000004, 0x00000000, 0x00000000, 0x14508200, 0x00001402, 0x00000000, 0x00000000,
        0x00000010, 0x00000020, 0x00000000, 0x00000000, 0xa28a28be, 0x00002228, 0x00000000, 0x00000000, 0xa28a28aa, 0x000022e8, 0x00000000, 0x00000000,
        0xa28a28aa, 0x000022a8, 0x00000000, 0x00000000, 0xa28a28aa, 0x000022e8, 0x00000000, 0x00000000, 0xbefbefbe, 0x00003e2f, 0x00000000, 0x00000000,
        0x00000004, 0x00002028, 0x00000000, 0x00000000, 0x80000000, 0x00003e0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 };

    int charsHeight = 10;
    int charsDivisor = 1;    // Every char is separated from the consecutive by a 1 pixel divisor, horizontally and vertically

    int charsWidth[224] = { 3, 1, 4, 6, 5, 7, 6, 2, 3, 3, 5, 5, 2, 4, 1, 7, 5, 2, 5, 5, 5, 5, 5, 5, 5, 5, 1, 1, 3, 4, 3, 6,
                            7, 6, 6, 6, 6, 6, 6, 6, 6, 3, 5, 6, 5, 7, 6, 6, 6, 6, 6, 6, 7, 6, 7, 7, 6, 6, 6, 2, 7, 2, 3, 5,
                            2, 5, 5, 5, 5, 5, 4, 5, 5, 1, 2, 5, 2, 5, 5, 5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 5, 3, 1, 3, 4, 4,
                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                            1, 1, 5, 5, 5, 7, 1, 5, 3, 7, 3, 5, 4, 1, 7, 4, 3, 5, 3, 3, 2, 5, 6, 1, 2, 2, 3, 5, 6, 6, 6, 6,
                            6, 6, 6, 6, 6, 6, 7, 6, 6, 6, 6, 6, 3, 3, 3, 3, 7, 6, 6, 6, 6, 6, 6, 5, 6, 6, 6, 6, 6, 6, 4, 6,
                            5, 5, 5, 5, 5, 5, 9, 5, 5, 5, 5, 5, 2, 2, 3, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 5 };

    // Re-construct image from defaultFontData and generate OpenGL texture
    //----------------------------------------------------------------------
    Image imFont = {
        .data = calloc(128*128, 2),  // 2 bytes per pixel (gray + alpha)
        .width = 128,
        .height = 128,
        .format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA,
        .mipmaps = 1
    };

    // Fill image.data with defaultFontData (convert from bit to pixel!)
    for (int i = 0, counter = 0; i < imFont.width*imFont.height; i += 32)
    {
        for (int j = 31; j >= 0; j--)
        {
            if (BIT_CHECK(defaultFontData[counter], j))
            {
                // NOTE: We are unreferencing data as short, so,
                // we must consider data as little-endian order (alpha + gray)
                ((unsigned short *)imFont.data)[i + j] = 0xffff;
            }
            else ((unsigned short *)imFont.data)[i + j] = 0x00ff;
        }

        counter++;
    }

    defaultFont.texture = LoadTextureFromImage(imFont);

    // Reconstruct charSet using charsWidth[], charsHeight, charsDivisor, glyphCount
    //------------------------------------------------------------------------------

    // Allocate space for our characters info data
    // NOTE: This memory should be freed at end! --> CloseWindow()
    defaultFont.glyphs = (GlyphInfo *)RL_MALLOC(defaultFont.glyphCount*sizeof(GlyphInfo));
    defaultFont.recs = (Rectangle *)RL_MALLOC(defaultFont.glyphCount*sizeof(Rectangle));

    int currentLine = 0;
    int currentPosX = charsDivisor;
    int testPosX = charsDivisor;

    for (int i = 0; i < defaultFont.glyphCount; i++)
    {
        defaultFont.glyphs[i].value = 32 + i;  // First char is 32

        defaultFont.recs[i].x = (float)currentPosX;
        defaultFont.recs[i].y = (float)(charsDivisor + currentLine*(charsHeight + charsDivisor));
        defaultFont.recs[i].width = (float)charsWidth[i];
        defaultFont.recs[i].height = (float)charsHeight;

        testPosX += (int)(defaultFont.recs[i].width + (float)charsDivisor);

        if (testPosX >= defaultFont.texture.width)
        {
            currentLine++;
            currentPosX = 2*charsDivisor + charsWidth[i];
            testPosX = currentPosX;

            defaultFont.recs[i].x = (float)charsDivisor;
            defaultFont.recs[i].y = (float)(charsDivisor + currentLine*(charsHeight + charsDivisor));
        }
        else currentPosX = testPosX;

        // NOTE: On default font character offsets and xAdvance are not required
        defaultFont.glyphs[i].offsetX = 0;
        defaultFont.glyphs[i].offsetY = 0;
        defaultFont.glyphs[i].advanceX = 0;

        // Fill character image data from fontClear data
        defaultFont.glyphs[i].image = ImageFromImage(imFont, defaultFont.recs[i]);
    }

    UnloadImage(imFont);

    defaultFont.baseSize = (int)defaultFont.recs[0].height;

    TRACELOG(LOG_INFO, "FONT: Default font loaded successfully (%i glyphs)", defaultFont.glyphCount);
}

// Unload raylib default font
extern void UnloadFontDefault(void)
{
    for (int i = 0; i < defaultFont.glyphCount; i++) UnloadImage(defaultFont.glyphs[i].image);
    UnloadTexture(defaultFont.texture);
    RL_FREE(defaultFont.glyphs);
    RL_FREE(defaultFont.recs);
}
#endif      // SUPPORT_DEFAULT_FONT

// Get the default font, useful to be used with extended parameters
Font GetFontDefault()
{
#if defined(SUPPORT_DEFAULT_FONT)
    return defaultFont;
#else
    Font font = { 0 };
    return font;
#endif
}

// Load Font from file into GPU memory (VRAM)
Font LoadFont(const char *fileName)
{
    // Default values for ttf font generation
#ifndef FONT_TTF_DEFAULT_SIZE
    #define FONT_TTF_DEFAULT_SIZE           32      // TTF font generation default char size (char-height)
#endif
#ifndef FONT_TTF_DEFAULT_NUMCHARS
    #define FONT_TTF_DEFAULT_NUMCHARS       95      // TTF font generation default charset: 95 glyphs (ASCII 32..126)
#endif
#ifndef FONT_TTF_DEFAULT_FIRST_CHAR
    #define FONT_TTF_DEFAULT_FIRST_CHAR     32      // TTF font generation default first char for image sprite font (32-Space)
#endif
#ifndef FONT_TTF_DEFAULT_CHARS_PADDING
    #define FONT_TTF_DEFAULT_CHARS_PADDING   4      // TTF font generation default chars padding
#endif

    Font font = { 0 };

#if defined(SUPPORT_FILEFORMAT_TTF)
    if (IsFileExtension(fileName, ".ttf") || IsFileExtension(fileName, ".otf")) font = LoadFontEx(fileName, FONT_TTF_DEFAULT_SIZE, NULL, FONT_TTF_DEFAULT_NUMCHARS);
    else
#endif
#if defined(SUPPORT_FILEFORMAT_FNT)
    if (IsFileExtension(fileName, ".fnt")) font = LoadBMFont(fileName);
    else
#endif
    {
        Image image = LoadImage(fileName);
        if (image.data != NULL) font = LoadFontFromImage(image, MAGENTA, FONT_TTF_DEFAULT_FIRST_CHAR);
        UnloadImage(image);
    }

    if (font.texture.id == 0)
    {
        TRACELOG(LOG_WARNING, "FONT: [%s] Failed to load font texture -> Using default font", fileName);
        font = GetFontDefault();
    }
    else SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);    // By default we set point filter (best performance)

    return font;
}

// Load Font from TTF font file with generation parameters
// NOTE: You can pass an array with desired characters, those characters should be available in the font
// if array is NULL, default char set is selected 32..126
Font LoadFontEx(const char *fileName, int fontSize, int *fontChars, int glyphCount)
{
    Font font = { 0 };

    // Loading file to memory
    unsigned int fileSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &fileSize);

    if (fileData != NULL)
    {
        // Loading font from memory data
        font = LoadFontFromMemory(GetFileExtension(fileName), fileData, fileSize, fontSize, fontChars, glyphCount);

        RL_FREE(fileData);
    }
    else font = GetFontDefault();

    return font;
}

// Load an Image font file (XNA style)
Font LoadFontFromImage(Image image, Color key, int firstChar)
{
#ifndef MAX_GLYPHS_FROM_IMAGE
    #define MAX_GLYPHS_FROM_IMAGE   256     // Maximum number of glyphs supported on image scan
#endif

    #define COLOR_EQUAL(col1, col2) ((col1.r == col2.r) && (col1.g == col2.g) && (col1.b == col2.b) && (col1.a == col2.a))

    Font font = GetFontDefault();

    int charSpacing = 0;
    int lineSpacing = 0;

    int x = 0;
    int y = 0;

    // We allocate a temporal arrays for chars data measures,
    // once we get the actual number of chars, we copy data to a sized arrays
    int tempCharValues[MAX_GLYPHS_FROM_IMAGE] = { 0 };
    Rectangle tempCharRecs[MAX_GLYPHS_FROM_IMAGE] = { 0 };

    Color *pixels = LoadImageColors(image);

    // Parse image data to get charSpacing and lineSpacing
    for (y = 0; y < image.height; y++)
    {
        for (x = 0; x < image.width; x++)
        {
            if (!COLOR_EQUAL(pixels[y*image.width + x], key)) break;
        }

        if (!COLOR_EQUAL(pixels[y*image.width + x], key)) break;
    }

    if ((x == 0) || (y == 0)) return font;

    charSpacing = x;
    lineSpacing = y;

    int charHeight = 0;
    int j = 0;

    while (!COLOR_EQUAL(pixels[(lineSpacing + j)*image.width + charSpacing], key)) j++;

    charHeight = j;

    // Check array values to get characters: value, x, y, w, h
    int index = 0;
    int lineToRead = 0;
    int xPosToRead = charSpacing;

    // Parse image data to get rectangle sizes
    while ((lineSpacing + lineToRead*(charHeight + lineSpacing)) < image.height)
    {
        while ((xPosToRead < image.width) &&
              !COLOR_EQUAL((pixels[(lineSpacing + (charHeight+lineSpacing)*lineToRead)*image.width + xPosToRead]), key))
        {
            tempCharValues[index] = firstChar + index;

            tempCharRecs[index].x = (float)xPosToRead;
            tempCharRecs[index].y = (float)(lineSpacing + lineToRead*(charHeight + lineSpacing));
            tempCharRecs[index].height = (float)charHeight;

            int charWidth = 0;

            while (!COLOR_EQUAL(pixels[(lineSpacing + (charHeight+lineSpacing)*lineToRead)*image.width + xPosToRead + charWidth], key)) charWidth++;

            tempCharRecs[index].width = (float)charWidth;

            index++;

            xPosToRead += (charWidth + charSpacing);
        }

        lineToRead++;
        xPosToRead = charSpacing;
    }

    // NOTE: We need to remove key color borders from image to avoid weird
    // artifacts on texture scaling when using TEXTURE_FILTER_BILINEAR or TEXTURE_FILTER_TRILINEAR
    for (int i = 0; i < image.height*image.width; i++) if (COLOR_EQUAL(pixels[i], key)) pixels[i] = BLANK;

    // Create a new image with the processed color data (key color replaced by BLANK)
    Image fontClear = {
        .data = pixels,
        .width = image.width,
        .height = image.height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };

    // Set font with all data parsed from image
    font.texture = LoadTextureFromImage(fontClear); // Convert processed image to OpenGL texture
    font.glyphCount = index;
    font.glyphPadding = 0;

    // We got tempCharValues and tempCharsRecs populated with chars data
    // Now we move temp data to sized charValues and charRecs arrays
    font.glyphs = (GlyphInfo *)RL_MALLOC(font.glyphCount*sizeof(GlyphInfo));
    font.recs = (Rectangle *)RL_MALLOC(font.glyphCount*sizeof(Rectangle));

    for (int i = 0; i < font.glyphCount; i++)
    {
        font.glyphs[i].value = tempCharValues[i];

        // Get character rectangle in the font atlas texture
        font.recs[i] = tempCharRecs[i];

        // NOTE: On image based fonts (XNA style), character offsets and xAdvance are not required (set to 0)
        font.glyphs[i].offsetX = 0;
        font.glyphs[i].offsetY = 0;
        font.glyphs[i].advanceX = 0;

        // Fill character image data from fontClear data
        font.glyphs[i].image = ImageFromImage(fontClear, tempCharRecs[i]);
    }

    UnloadImage(fontClear);     // Unload processed image once converted to texture

    font.baseSize = (int)font.recs[0].height;

    return font;
}

// Load font from memory buffer, fileType refers to extension: i.e. ".ttf"
Font LoadFontFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, int fontSize, int *fontChars, int glyphCount)
{
    Font font = { 0 };

    char fileExtLower[16] = { 0 };
    strcpy(fileExtLower, TextToLower(fileType));

#if defined(SUPPORT_FILEFORMAT_TTF)
    if (TextIsEqual(fileExtLower, ".ttf") ||
        TextIsEqual(fileExtLower, ".otf"))
    {
        font.baseSize = fontSize;
        font.glyphCount = (glyphCount > 0)? glyphCount : 95;
        font.glyphPadding = 0;
        font.glyphs = LoadFontData(fileData, dataSize, font.baseSize, fontChars, font.glyphCount, FONT_DEFAULT);

        if (font.glyphs != NULL)
        {
            font.glyphPadding = FONT_TTF_DEFAULT_CHARS_PADDING;

            Image atlas = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, font.baseSize, font.glyphPadding, 0);
            font.texture = LoadTextureFromImage(atlas);

            // Update glyphs[i].image to use alpha, required to be used on ImageDrawText()
            for (int i = 0; i < font.glyphCount; i++)
            {
                UnloadImage(font.glyphs[i].image);
                font.glyphs[i].image = ImageFromImage(atlas, font.recs[i]);
            }

            UnloadImage(atlas);

            // TRACELOG(LOG_INFO, "FONT: Font loaded successfully (%i glyphs)", font.glyphCount);
        }
        else font = GetFontDefault();
    }
#else
    font = GetFontDefault();
#endif

    return font;
}

// Load font data for further use
// NOTE: Requires TTF font memory data and can generate SDF data
GlyphInfo *LoadFontData(const unsigned char *fileData, int dataSize, int fontSize, int *fontChars, int glyphCount, int type)
{
    // NOTE: Using some SDF generation default values,
    // trades off precision with ability to handle *smaller* sizes
#ifndef FONT_SDF_CHAR_PADDING
    #define FONT_SDF_CHAR_PADDING            4      // SDF font generation char padding
#endif
#ifndef FONT_SDF_ON_EDGE_VALUE
    #define FONT_SDF_ON_EDGE_VALUE         128      // SDF font generation on edge value
#endif
#ifndef FONT_SDF_PIXEL_DIST_SCALE
    #define FONT_SDF_PIXEL_DIST_SCALE     64.0f     // SDF font generation pixel distance scale
#endif
#ifndef FONT_BITMAP_ALPHA_THRESHOLD
    #define FONT_BITMAP_ALPHA_THRESHOLD     80      // Bitmap (B&W) font generation alpha threshold
#endif

    GlyphInfo *chars = NULL;

#if defined(SUPPORT_FILEFORMAT_TTF)
    // Load font data (including pixel data) from TTF memory file
    // NOTE: Loaded information should be enough to generate font image atlas, using any packaging method
    if (fileData != NULL)
    {
        bool genFontChars = false;
        stbtt_fontinfo fontInfo = { 0 };

        if (stbtt_InitFont(&fontInfo, (unsigned char *)fileData, 0))     // Initialize font for data reading
        {
            // Calculate font scale factor
            float scaleFactor = stbtt_ScaleForPixelHeight(&fontInfo, (float)fontSize);

            // Calculate font basic metrics
            // NOTE: ascent is equivalent to font baseline
            int ascent, descent, lineGap;
            stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

            // In case no chars count provided, default to 95
            glyphCount = (glyphCount > 0)? glyphCount : 95;

            // Fill fontChars in case not provided externally
            // NOTE: By default we fill glyphCount consecutevely, starting at 32 (Space)

            if (fontChars == NULL)
            {
                fontChars = (int *)RL_MALLOC(glyphCount*sizeof(int));
                for (int i = 0; i < glyphCount; i++) fontChars[i] = i + 32;
                genFontChars = true;
            }

            chars = (GlyphInfo *)RL_MALLOC(glyphCount*sizeof(GlyphInfo));

            // NOTE: Using simple packaging, one char after another
            for (int i = 0; i < glyphCount; i++)
            {
                int chw = 0, chh = 0;   // Character width and height (on generation)
                int ch = fontChars[i];  // Character value to get info for
                chars[i].value = ch;

                //  Render a unicode codepoint to a bitmap
                //      stbtt_GetCodepointBitmap()           -- allocates and returns a bitmap
                //      stbtt_GetCodepointBitmapBox()        -- how big the bitmap must be
                //      stbtt_MakeCodepointBitmap()          -- renders into bitmap you provide

                if (type != FONT_SDF) chars[i].image.data = stbtt_GetCodepointBitmap(&fontInfo, scaleFactor, scaleFactor, ch, &chw, &chh, &chars[i].offsetX, &chars[i].offsetY);
                else if (ch != 32) chars[i].image.data = stbtt_GetCodepointSDF(&fontInfo, scaleFactor, ch, FONT_SDF_CHAR_PADDING, FONT_SDF_ON_EDGE_VALUE, FONT_SDF_PIXEL_DIST_SCALE, &chw, &chh, &chars[i].offsetX, &chars[i].offsetY);
                else chars[i].image.data = NULL;

                stbtt_GetCodepointHMetrics(&fontInfo, ch, &chars[i].advanceX, NULL);
                chars[i].advanceX = (int)((float)chars[i].advanceX*scaleFactor);

                // Load characters images
                chars[i].image.width = chw;
                chars[i].image.height = chh;
                chars[i].image.mipmaps = 1;
                chars[i].image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;

                chars[i].offsetY += (int)((float)ascent*scaleFactor);

                // NOTE: We create an empty image for space character, it could be further required for atlas packing
                if (ch == 32)
                {
                    Image imSpace = {
                        .data = calloc(chars[i].advanceX*fontSize, 2),
                        .width = chars[i].advanceX,
                        .height = fontSize,
                        .format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
                        .mipmaps = 1
                    };

                    chars[i].image = imSpace;
                }

                if (type == FONT_BITMAP)
                {
                    // Aliased bitmap (black & white) font generation, avoiding anti-aliasing
                    // NOTE: For optimum results, bitmap font should be generated at base pixel size
                    for (int p = 0; p < chw*chh; p++)
                    {
                        if (((unsigned char *)chars[i].image.data)[p] < FONT_BITMAP_ALPHA_THRESHOLD) ((unsigned char *)chars[i].image.data)[p] = 0;
                        else ((unsigned char *)chars[i].image.data)[p] = 255;
                    }
                }

                // Get bounding box for character (may be offset to account for chars that dip above or below the line)
                /*
                int chX1, chY1, chX2, chY2;
                stbtt_GetCodepointBitmapBox(&fontInfo, ch, scaleFactor, scaleFactor, &chX1, &chY1, &chX2, &chY2);

                TRACELOGD("FONT: Character box measures: %i, %i, %i, %i", chX1, chY1, chX2 - chX1, chY2 - chY1);
                TRACELOGD("FONT: Character offsetY: %i", (int)((float)ascent*scaleFactor) + chY1);
                */
            }
        }
        else TRACELOG(LOG_WARNING, "FONT: Failed to process TTF font data");

        if (genFontChars) RL_FREE(fontChars);
    }
#endif

    return chars;
}

// Generate image font atlas using chars info
// NOTE: Packing method: 0-Default, 1-Skyline
#if defined(SUPPORT_FILEFORMAT_TTF)
Image GenImageFontAtlas(const GlyphInfo *chars, Rectangle **charRecs, int glyphCount, int fontSize, int padding, int packMethod)
{
    Image atlas = { 0 };

    if (chars == NULL)
    {
        TraceLog(LOG_WARNING, "FONT: Provided chars info not valid, returning empty image atlas");
        return atlas;
    }

    *charRecs = NULL;

    // In case no chars count provided we suppose default of 95
    glyphCount = (glyphCount > 0)? glyphCount : 95;

    // NOTE: Rectangles memory is loaded here!
    Rectangle *recs = (Rectangle *)RL_MALLOC(glyphCount*sizeof(Rectangle));

    // Calculate image size based on required pixel area
    // NOTE 1: Image is forced to be squared and POT... very conservative!
    // NOTE 2: SDF font characters already contain an internal padding,
    // so image size would result bigger than default font type
    float requiredArea = 0;
    for (int i = 0; i < glyphCount; i++) requiredArea += ((chars[i].image.width + 2*padding)*(chars[i].image.height + 2*padding));
    float guessSize = sqrtf(requiredArea)*1.4f;
    int imageSize = (int)powf(2, ceilf(logf((float)guessSize)/logf(2)));  // Calculate next POT

    atlas.width = imageSize;   // Atlas bitmap width
    atlas.height = imageSize;  // Atlas bitmap height
    atlas.data = (unsigned char *)RL_CALLOC(1, atlas.width*atlas.height);      // Create a bitmap to store characters (8 bpp)
    atlas.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
    atlas.mipmaps = 1;

    // DEBUG: We can see padding in the generated image setting a gray background...
    //for (int i = 0; i < atlas.width*atlas.height; i++) ((unsigned char *)atlas.data)[i] = 100;

    if (packMethod == 0)   // Use basic packing algorythm
    {
        int offsetX = padding;
        int offsetY = padding;

        // NOTE: Using simple packaging, one char after another
        for (int i = 0; i < glyphCount; i++)
        {
            // Copy pixel data from fc.data to atlas
            for (int y = 0; y < chars[i].image.height; y++)
            {
                for (int x = 0; x < chars[i].image.width; x++)
                {
                    ((unsigned char *)atlas.data)[(offsetY + y)*atlas.width + (offsetX + x)] = ((unsigned char *)chars[i].image.data)[y*chars[i].image.width + x];
                }
            }

            // Fill chars rectangles in atlas info
            recs[i].x = (float)offsetX;
            recs[i].y = (float)offsetY;
            recs[i].width = (float)chars[i].image.width;
            recs[i].height = (float)chars[i].image.height;

            // Move atlas position X for next character drawing
            offsetX += (chars[i].image.width + 2*padding);

            if (offsetX >= (atlas.width - chars[i].image.width - 2*padding))
            {
                offsetX = padding;

                // NOTE: Be careful on offsetY for SDF fonts, by default SDF
                // use an internal padding of 4 pixels, it means char rectangle
                // height is bigger than fontSize, it could be up to (fontSize + 8)
                offsetY += (fontSize + 2*padding);

                if (offsetY > (atlas.height - fontSize - padding))
                {
                    for(int j = i + 1; j < glyphCount; j++)
                    {
                        TRACELOG(LOG_WARNING, "FONT: Failed to package character (%i)", j);
                        // make sure remaining recs contain valid data
                        recs[j].x = 0;
                        recs[j].y = 0;
                        recs[j].width = 0;
                        recs[j].height = 0;
                    }
                    break;
                }
            }
        }
    }
    else if (packMethod == 1)  // Use Skyline rect packing algorythm (stb_pack_rect)
    {
        stbrp_context *context = (stbrp_context *)RL_MALLOC(sizeof(*context));
        stbrp_node *nodes = (stbrp_node *)RL_MALLOC(glyphCount*sizeof(*nodes));

        stbrp_init_target(context, atlas.width, atlas.height, nodes, glyphCount);
        stbrp_rect *rects = (stbrp_rect *)RL_MALLOC(glyphCount*sizeof(stbrp_rect));

        // Fill rectangles for packaging
        for (int i = 0; i < glyphCount; i++)
        {
            rects[i].id = i;
            rects[i].w = chars[i].image.width + 2*padding;
            rects[i].h = chars[i].image.height + 2*padding;
        }

        // Package rectangles into atlas
        stbrp_pack_rects(context, rects, glyphCount);

        for (int i = 0; i < glyphCount; i++)
        {
            // It return char rectangles in atlas
            recs[i].x = rects[i].x + (float)padding;
            recs[i].y = rects[i].y + (float)padding;
            recs[i].width = (float)chars[i].image.width;
            recs[i].height = (float)chars[i].image.height;

            if (rects[i].was_packed)
            {
                // Copy pixel data from fc.data to atlas
                for (int y = 0; y < chars[i].image.height; y++)
                {
                    for (int x = 0; x < chars[i].image.width; x++)
                    {
                        ((unsigned char *)atlas.data)[(rects[i].y + padding + y)*atlas.width + (rects[i].x + padding + x)] = ((unsigned char *)chars[i].image.data)[y*chars[i].image.width + x];
                    }
                }
            }
            else TRACELOG(LOG_WARNING, "FONT: Failed to package character (%i)", i);
        }

        RL_FREE(rects);
        RL_FREE(nodes);
        RL_FREE(context);
    }

    // Convert image data from GRAYSCALE to GRAY_ALPHA
    unsigned char *dataGrayAlpha = (unsigned char *)RL_MALLOC(atlas.width*atlas.height*sizeof(unsigned char)*2); // Two channels

    for (int i = 0, k = 0; i < atlas.width*atlas.height; i++, k += 2)
    {
        dataGrayAlpha[k] = 255;
        dataGrayAlpha[k + 1] = ((unsigned char *)atlas.data)[i];
    }

    RL_FREE(atlas.data);
    atlas.data = dataGrayAlpha;
    atlas.format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;

    *charRecs = recs;

    return atlas;
}
#endif

// Unload font glyphs info data (RAM)
void UnloadFontData(GlyphInfo *glyphs, int glyphCount)
{
    for (int i = 0; i < glyphCount; i++) UnloadImage(glyphs[i].image);

    RL_FREE(glyphs);
}

// Unload Font from GPU memory (VRAM)
void UnloadFont(Font font)
{
    // NOTE: Make sure font is not default font (fallback)
    if (font.texture.id != GetFontDefault().texture.id)
    {
        UnloadFontData(font.glyphs, font.glyphCount);
        UnloadTexture(font.texture);
        RL_FREE(font.recs);

        TRACELOGD("FONT: Unloaded font data from RAM and VRAM");
    }
}

// Export font as code file, returns true on success
bool ExportFontAsCode(Font font, const char *fileName)
{
    bool success = false;

#ifndef TEXT_BYTES_PER_LINE
    #define TEXT_BYTES_PER_LINE     20
#endif

    #define MAX_FONT_DATA_SIZE      1024*1024       // 1 MB

    // Get file name from path
    char fileNamePascal[256] = { 0 };
    strcpy(fileNamePascal, TextToPascal(GetFileNameWithoutExt(fileName)));

    // NOTE: Text data buffer size is estimated considering image data size in bytes
    // and requiring 6 char bytes for every byte: "0x00, "
    char *txtData = (char *)RL_CALLOC(MAX_FONT_DATA_SIZE, sizeof(char));

    int byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// FontAsCode exporter v1.0 - Font data exported as an array of bytes                 //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// more info and bugs-report:  github.com/raysan5/raylib                              //\n");
    byteCount += sprintf(txtData + byteCount, "// feedback and support:       ray[at]raylib.com                                      //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Copyright (c) 2018-2022 Ramon Santamaria (@raysan5)                                //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// ---------------------------------------------------------------------------------- //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// TODO: Fill the information and license of the exported font here:                  //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Font name:    ....                                                                 //\n");
    byteCount += sprintf(txtData + byteCount, "// Font creator: ....                                                                 //\n");
    byteCount += sprintf(txtData + byteCount, "// Font LICENSE: ....                                                                 //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n\n");

    // Support font export and initialization
    // NOTE: This mechanism is highly coupled to raylib
    Image image = LoadImageFromTexture(font.texture);
    if (image.format != PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) TRACELOG(LOG_WARNING, "Font export as code: Font image format is not GRAY+ALPHA!");
    int imageDataSize = GetPixelDataSize(image.width, image.height, image.format);

    // Image data is usually GRAYSCALE + ALPHA and can be reduced to GRAYSCALE
    //ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);

#define SUPPORT_COMPRESSED_FONT_ATLAS
#if defined(SUPPORT_COMPRESSED_FONT_ATLAS)
    // WARNING: Data is compressed using raylib CompressData() DEFLATE,
    // it requires to be decompressed with raylib DecompressData(), that requires
    // compiling raylib with SUPPORT_COMPRESSION_API config flag enabled

    // Compress font image data
    int compDataSize = 0;
    unsigned char *compData = CompressData(image.data, imageDataSize, &compDataSize);

    // Save font image data (compressed)
    byteCount += sprintf(txtData + byteCount, "#define COMPRESSED_DATA_SIZE_FONT_%s %i\n\n", TextToUpper(fileNamePascal), compDataSize);
    byteCount += sprintf(txtData + byteCount, "// Font image pixels data compressed (DEFLATE)\n");
    byteCount += sprintf(txtData + byteCount, "// NOTE: Original pixel data simplified to GRAYSCALE\n");
    byteCount += sprintf(txtData + byteCount, "static unsigned char fontData_%s[COMPRESSED_DATA_SIZE_FONT_%s] = { ", fileNamePascal, TextToUpper(fileNamePascal));
    for (int i = 0; i < compDataSize - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%02x,\n    " : "0x%02x, "), compData[i]);
    byteCount += sprintf(txtData + byteCount, "0x%02x };\n\n", compData[compDataSize - 1]);
    MemFree(compData);
#else
    // Save font image data (uncompressed)
    byteCount += sprintf(txtData + byteCount, "// Font image pixels data\n");
    byteCount += sprintf(txtData + byteCount, "// NOTE: 2 bytes per pixel, GRAY + ALPHA channels\n");
    byteCount += sprintf(txtData + byteCount, "static unsigned char fontImageData_%s[%i] = { ", fileNamePascal, imageDataSize);
    for (int i = 0; i < imageDataSize - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%02x,\n    " : "0x%02x, "), ((unsigned char *)imFont.data)[i]);
    byteCount += sprintf(txtData + byteCount, "0x%02x };\n\n", ((unsigned char *)imFont.data)[imageDataSize - 1]);
#endif

    // Save font recs data
    byteCount += sprintf(txtData + byteCount, "// Font characters rectangles data\n");
    byteCount += sprintf(txtData + byteCount, "static const Rectangle fontRecs_%s[%i] = {\n", fileNamePascal, font.glyphCount);
    for (int i = 0; i < font.glyphCount; i++)
    {
        byteCount += sprintf(txtData + byteCount, "    { %1.0f, %1.0f, %1.0f , %1.0f },\n", font.recs[i].x, font.recs[i].y, font.recs[i].width, font.recs[i].height);
    }
    byteCount += sprintf(txtData + byteCount, "};\n\n");

    // Save font glyphs data
    // NOTE: Glyphs image data not saved (grayscale pixels),
    // it could be generated from image and recs
    byteCount += sprintf(txtData + byteCount, "// Font glyphs info data\n");
    byteCount += sprintf(txtData + byteCount, "// NOTE: No glyphs.image data provided\n");
    byteCount += sprintf(txtData + byteCount, "static const GlyphInfo fontGlyphs_%s[%i] = {\n", fileNamePascal, font.glyphCount);
    for (int i = 0; i < font.glyphCount; i++)
    {
        byteCount += sprintf(txtData + byteCount, "    { %i, %i, %i, %i, { 0 }},\n", font.glyphs[i].value, font.glyphs[i].offsetX, font.glyphs[i].offsetY, font.glyphs[i].advanceX);
    }
    byteCount += sprintf(txtData + byteCount, "};\n\n");

    // Custom font loading function
    byteCount += sprintf(txtData + byteCount, "// Font loading function: %s\n", fileNamePascal);
    byteCount += sprintf(txtData + byteCount, "static Font LoadFont_%s(void)\n{\n", fileNamePascal);
    byteCount += sprintf(txtData + byteCount, "    Font font = { 0 };\n\n");
    byteCount += sprintf(txtData + byteCount, "    font.baseSize = %i;\n", font.baseSize);
    byteCount += sprintf(txtData + byteCount, "    font.glyphCount = %i;\n", font.glyphCount);
    byteCount += sprintf(txtData + byteCount, "    font.glyphPadding = %i;\n\n", FONT_TTF_DEFAULT_CHARS_PADDING);
    byteCount += sprintf(txtData + byteCount, "    // Custom font loading\n");
#if defined(SUPPORT_COMPRESSED_FONT_ATLAS)
    byteCount += sprintf(txtData + byteCount, "    // NOTE: Compressed font image data (DEFLATE), it requires DecompressData() function\n");
    byteCount += sprintf(txtData + byteCount, "    int fontDataSize_%s = 0;\n", fileNamePascal);
    byteCount += sprintf(txtData + byteCount, "    unsigned char *data = DecompressData(fontData_%s, COMPRESSED_DATA_SIZE_FONT_%s, &fontDataSize_%s);\n", fileNamePascal, TextToUpper(fileNamePascal), fileNamePascal);
    byteCount += sprintf(txtData + byteCount, "    Image imFont = { data, %i, %i, 1, %i };\n\n", image.width, image.height, image.format);
#else
    byteCount += sprintf(txtData + byteCount, "    Image imFont = { fontImageData_%s, %i, %i, 1, %i };\n\n", styleName, image.width, image.height, image.format);
#endif
    byteCount += sprintf(txtData + byteCount, "    // Load texture from image\n");
    byteCount += sprintf(txtData + byteCount, "    font.texture = LoadTextureFromImage(imFont);\n");
#if defined(SUPPORT_COMPRESSED_FONT_ATLAS)
    byteCount += sprintf(txtData + byteCount, "    UnloadImage(imFont);  // Uncompressed data can be unloaded from memory\n\n");
#endif
    // We have two possible mechanisms to assign font.recs and font.glyphs data,
    // that data is already available as global arrays, we two options to assign that data:
    //  - 1. Data copy. This option consumes more memory and Font MUST be unloaded by user, requiring additional code.
    //  - 2. Data assignment. This option consumes less memory and Font MUST NOT be unloaded by user because data is on protected DATA segment
//#define SUPPORT_FONT_DATA_COPY
#if defined(SUPPORT_FONT_DATA_COPY)
    byteCount += sprintf(txtData + byteCount, "    // Copy glyph recs data from global fontRecs\n");
    byteCount += sprintf(txtData + byteCount, "    // NOTE: Required to avoid issues if trying to free font\n");
    byteCount += sprintf(txtData + byteCount, "    font.recs = (Rectangle *)malloc(font.glyphCount*sizeof(Rectangle));\n");
    byteCount += sprintf(txtData + byteCount, "    memcpy(font.recs, fontRecs_%s, font.glyphCount*sizeof(Rectangle));\n\n", fileNamePascal);

    byteCount += sprintf(txtData + byteCount, "    // Copy font glyph info data from global fontChars\n");
    byteCount += sprintf(txtData + byteCount, "    // NOTE: Required to avoid issues if trying to free font\n");
    byteCount += sprintf(txtData + byteCount, "    font.glyphs = (GlyphInfo *)malloc(font.glyphCount*sizeof(GlyphInfo));\n");
    byteCount += sprintf(txtData + byteCount, "    memcpy(font.glyphs, fontGlyphs_%s, font.glyphCount*sizeof(GlyphInfo));\n\n", fileNamePascal);
#else
    byteCount += sprintf(txtData + byteCount, "    // Assign glyph recs and info data directly\n");
    byteCount += sprintf(txtData + byteCount, "    // WARNING: This font data must not be unloaded\n");
    byteCount += sprintf(txtData + byteCount, "    font.recs = fontRecs_%s;\n", fileNamePascal);
    byteCount += sprintf(txtData + byteCount, "    font.glyphs = fontGlyphs_%s;\n\n", fileNamePascal);
#endif
    byteCount += sprintf(txtData + byteCount, "    return font;\n");
    byteCount += sprintf(txtData + byteCount, "}\n");

    UnloadImage(image);

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    success = SaveFileText(fileName, txtData);

    RL_FREE(txtData);

    if (success != 0) TRACELOG(LOG_INFO, "FILEIO: [%s] Font as code exported successfully", fileName);
    else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to export font as code", fileName);

    return success;
}


// Draw current FPS
// NOTE: Uses default font
void DrawFPS(int posX, int posY)
{
    Color color = LIME;                         // Good FPS
    int fps = GetFPS();

    if ((fps < 30) && (fps >= 15)) color = ORANGE;  // Warning FPS
    else if (fps < 15) color = RED;             // Low FPS

    DrawText(TextFormat("%2i FPS", GetFPS()), posX, posY, 20, color);
}

// Draw text (using default font)
// NOTE: fontSize work like in any drawing program but if fontSize is lower than font-base-size, then font-base-size is used
// NOTE: chars spacing is proportional to fontSize
void DrawText(const char *text, int posX, int posY, int fontSize, Color color)
{
    // Check if default font has been loaded
    if (GetFontDefault().texture.id != 0)
    {
        Vector2 position = { (float)posX, (float)posY };

        int defaultFontSize = 10;   // Default Font chars height in pixel
        if (fontSize < defaultFontSize) fontSize = defaultFontSize;
        int spacing = fontSize/defaultFontSize;

        DrawTextEx(GetFontDefault(), text, position, (float)fontSize, (float)spacing, color);
    }
}

// Draw text using Font
// NOTE: chars spacing is NOT proportional to fontSize
void DrawTextEx(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint)
{
    if (font.texture.id == 0) font = GetFontDefault();  // Security check in case of not valid font

    int size = TextLength(text);    // Total size in bytes of the text, scanned by codepoints in loop

    int textOffsetY = 0;            // Offset between lines (on line break '\n')
    float textOffsetX = 0.0f;       // Offset X to next character to draw

    float scaleFactor = fontSize/font.baseSize;         // Character quad scaling factor

    for (int i = 0; i < size;)
    {
        // Get next codepoint from byte string and glyph index in font
        int codepointByteCount = 0;
        int codepoint = GetCodepoint(&text[i], &codepointByteCount);
        int index = GetGlyphIndex(font, codepoint);

        // NOTE: Normally we exit the decoding sequence as soon as a bad byte is found (and return 0x3f)
        // but we need to draw all of the bad bytes using the '?' symbol moving one byte
        if (codepoint == 0x3f) codepointByteCount = 1;

        if (codepoint == '\n')
        {
            // NOTE: Fixed line spacing of 1.5 line-height
            // TODO: Support custom line spacing defined by user
            textOffsetY += (int)((font.baseSize + font.baseSize/2)*scaleFactor);
            textOffsetX = 0.0f;
        }
        else
        {
            if ((codepoint != ' ') && (codepoint != '\t'))
            {
                DrawTextCodepoint(font, codepoint, (Vector2){ position.x + textOffsetX, position.y + textOffsetY }, fontSize, tint);
            }

            if (font.glyphs[index].advanceX == 0) textOffsetX += ((float)font.recs[index].width*scaleFactor + spacing);
            else textOffsetX += ((float)font.glyphs[index].advanceX*scaleFactor + spacing);
        }

        i += codepointByteCount;   // Move text bytes counter to next codepoint
    }
}

// Draw text using Font and pro parameters (rotation)
void DrawTextPro(Font font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float spacing, Color tint)
{
    rlPushMatrix();

        rlTranslatef(position.x, position.y, 0.0f);
        rlRotatef(rotation, 0.0f, 0.0f, 1.0f);
        rlTranslatef(-origin.x, -origin.y, 0.0f);

        DrawTextEx(font, text, (Vector2){ 0.0f, 0.0f }, fontSize, spacing, tint);

    rlPopMatrix();
}

// Draw one character (codepoint)
void DrawTextCodepoint(Font font, int codepoint, Vector2 position, float fontSize, Color tint)
{
    // Character index position in sprite font
    // NOTE: In case a codepoint is not available in the font, index returned points to '?'
    int index = GetGlyphIndex(font, codepoint);
    float scaleFactor = fontSize/font.baseSize;     // Character quad scaling factor

    // Character destination rectangle on screen
    // NOTE: We consider glyphPadding on drawing
    Rectangle dstRec = { (int)position.x + font.glyphs[index].offsetX*scaleFactor - (float)font.glyphPadding*scaleFactor,
                      (int)position.y + font.glyphs[index].offsetY*scaleFactor - (float)font.glyphPadding*scaleFactor,
                      (font.recs[index].width + 2.0f*font.glyphPadding)*scaleFactor,
                      (font.recs[index].height + 2.0f*font.glyphPadding)*scaleFactor };

    // Character source rectangle from font texture atlas
    // NOTE: We consider chars padding when drawing, it could be required for outline/glow shader effects
    Rectangle srcRec = { font.recs[index].x - (float)font.glyphPadding, font.recs[index].y - (float)font.glyphPadding,
                         font.recs[index].width + 2.0f*font.glyphPadding, font.recs[index].height + 2.0f*font.glyphPadding };

    // Draw the character texture on the screen
    DrawTexturePro(font.texture, srcRec, dstRec, (Vector2){ 0, 0 }, 0.0f, tint);
}

// Draw multiple character (codepoints)
void DrawTextCodepoints(Font font, const int *codepoints, int count, Vector2 position, float fontSize, float spacing, Color tint)
{
    int textOffsetY = 0;            // Offset between lines (on line break '\n')
    float textOffsetX = 0.0f;       // Offset X to next character to draw

    float scaleFactor = fontSize/font.baseSize;         // Character quad scaling factor

    for (int i = 0; i < count; i++)
    {
        int index = GetGlyphIndex(font, codepoints[i]);

        if (codepoints[i] == '\n')
        {
            // NOTE: Fixed line spacing of 1.5 line-height
            // TODO: Support custom line spacing defined by user
            textOffsetY += (int)((font.baseSize + font.baseSize/2)*scaleFactor);
            textOffsetX = 0.0f;
        }
        else
        {
            if ((codepoints[i] != ' ') && (codepoints[i] != '\t'))
            {
                DrawTextCodepoint(font, codepoints[i], (Vector2){ position.x + textOffsetX, position.y + textOffsetY }, fontSize, tint);
            }

            if (font.glyphs[index].advanceX == 0) textOffsetX += ((float)font.recs[index].width*scaleFactor + spacing);
            else textOffsetX += ((float)font.glyphs[index].advanceX*scaleFactor + spacing);
        }
    }
}

// Measure string width for default font
int MeasureText(const char *text, int fontSize)
{
    Vector2 vec = { 0.0f, 0.0f };

    // Check if default font has been loaded
    if (GetFontDefault().texture.id != 0)
    {
        int defaultFontSize = 10;   // Default Font chars height in pixel
        if (fontSize < defaultFontSize) fontSize = defaultFontSize;
        int spacing = fontSize/defaultFontSize;

        vec = MeasureTextEx(GetFontDefault(), text, (float)fontSize, (float)spacing);
    }

    return (int)vec.x;
}

// Measure string size for Font
Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing)
{
    int size = TextLength(text);    // Get size in bytes of text
    int tempByteCounter = 0;        // Used to count longer text line num chars
    int byteCounter = 0;

    float textWidth = 0.0f;
    float tempTextWidth = 0.0f;     // Used to count longer text line width

    float textHeight = (float)font.baseSize;
    float scaleFactor = fontSize/(float)font.baseSize;

    int letter = 0;                 // Current character
    int index = 0;                  // Index position in sprite font

    for (int i = 0; i < size; i++)
    {
        byteCounter++;

        int next = 0;
        letter = GetCodepoint(&text[i], &next);
        index = GetGlyphIndex(font, letter);

        // NOTE: normally we exit the decoding sequence as soon as a bad byte is found (and return 0x3f)
        // but we need to draw all of the bad bytes using the '?' symbol so to not skip any we set next = 1
        if (letter == 0x3f) next = 1;
        i += next - 1;

        if (letter != '\n')
        {
            if (font.glyphs[index].advanceX != 0) textWidth += font.glyphs[index].advanceX;
            else textWidth += (font.recs[index].width + font.glyphs[index].offsetX);
        }
        else
        {
            if (tempTextWidth < textWidth) tempTextWidth = textWidth;
            byteCounter = 0;
            textWidth = 0;
            textHeight += ((float)font.baseSize*1.5f); // NOTE: Fixed line spacing of 1.5 lines
        }

        if (tempByteCounter < byteCounter) tempByteCounter = byteCounter;
    }

    if (tempTextWidth < textWidth) tempTextWidth = textWidth;

    Vector2 vec = { 0 };
    vec.x = tempTextWidth*scaleFactor + (float)((tempByteCounter - 1)*spacing); // Adds chars spacing to measure
    vec.y = textHeight*scaleFactor;

    return vec;
}

// Get index position for a unicode character on font
// NOTE: If codepoint is not found in the font it fallbacks to '?'
int GetGlyphIndex(Font font, int codepoint)
{
#ifndef GLYPH_NOTFOUND_CHAR_FALLBACK
    #define GLYPH_NOTFOUND_CHAR_FALLBACK     63      // Character used if requested codepoint is not found: '?'
#endif

// Support charsets with any characters order
#define SUPPORT_UNORDERED_CHARSET
#if defined(SUPPORT_UNORDERED_CHARSET)
    int index = GLYPH_NOTFOUND_CHAR_FALLBACK;

    for (int i = 0; i < font.glyphCount; i++)
    {
        if (font.glyphs[i].value == codepoint)
        {
            index = i;
            break;
        }
    }

    return index;
#else
    return (codepoint - 32);
#endif
}

// Get glyph font info data for a codepoint (unicode character)
// NOTE: If codepoint is not found in the font it fallbacks to '?'
GlyphInfo GetGlyphInfo(Font font, int codepoint)
{
    GlyphInfo info = { 0 };

    info = font.glyphs[GetGlyphIndex(font, codepoint)];

    return info;
}

// Get glyph rectangle in font atlas for a codepoint (unicode character)
// NOTE: If codepoint is not found in the font it fallbacks to '?'
Rectangle GetGlyphAtlasRec(Font font, int codepoint)
{
    Rectangle rec = { 0 };

    rec = font.recs[GetGlyphIndex(font, codepoint)];

    return rec;
}

//----------------------------------------------------------------------------------
// Text strings management functions
//----------------------------------------------------------------------------------
// Get text length in bytes, check for \0 character
unsigned int TextLength(const char *text)
{
    unsigned int length = 0; //strlen(text)

    if (text != NULL)
    {
        while (*text++) length++;
    }

    return length;
}

// Formatting of text with variables to 'embed'
// WARNING: String returned will expire after this function is called MAX_TEXTFORMAT_BUFFERS times
const char *TextFormat(const char *text, ...)
{
#ifndef MAX_TEXTFORMAT_BUFFERS
    #define MAX_TEXTFORMAT_BUFFERS 4        // Maximum number of static buffers for text formatting
#endif

    // We create an array of buffers so strings don't expire until MAX_TEXTFORMAT_BUFFERS invocations
    static char buffers[MAX_TEXTFORMAT_BUFFERS][MAX_TEXT_BUFFER_LENGTH] = { 0 };
    static int index = 0;

    char *currentBuffer = buffers[index];
    memset(currentBuffer, 0, MAX_TEXT_BUFFER_LENGTH);   // Clear buffer before using

    va_list args;
    va_start(args, text);
    vsnprintf(currentBuffer, MAX_TEXT_BUFFER_LENGTH, text, args);
    va_end(args);

    index += 1;     // Move to next buffer for next function call
    if (index >= MAX_TEXTFORMAT_BUFFERS) index = 0;

    return currentBuffer;
}

// Get integer value from text
// NOTE: This function replaces atoi() [stdlib.h]
int TextToInteger(const char *text)
{
    int value = 0;
    int sign = 1;

    if ((text[0] == '+') || (text[0] == '-'))
    {
        if (text[0] == '-') sign = -1;
        text++;
    }

    for (int i = 0; ((text[i] >= '0') && (text[i] <= '9')); ++i) value = value*10 + (int)(text[i] - '0');

    return value*sign;
}

#if defined(SUPPORT_TEXT_MANIPULATION)
// Copy one string to another, returns bytes copied
int TextCopy(char *dst, const char *src)
{
    int bytes = 0;

    if (dst != NULL)
    {
        while (*src != '\0')
        {
            *dst = *src;
            dst++;
            src++;

            bytes++;
        }

        *dst = '\0';
    }

    return bytes;
}

// Check if two text string are equal
// REQUIRES: strcmp()
bool TextIsEqual(const char *text1, const char *text2)
{
    bool result = false;

    if ((text1 != NULL) && (text2 != NULL))
    {
        if (strcmp(text1, text2) == 0) result = true;
    }

    return result;
}

// Get a piece of a text string
const char *TextSubtext(const char *text, int position, int length)
{
    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    int textLength = TextLength(text);

    if (position >= textLength)
    {
        position = textLength - 1;
        length = 0;
    }

    if (length >= textLength) length = textLength;

    for (int c = 0 ; c < length ; c++)
    {
        *(buffer + c) = *(text + position);
        text++;
    }

    *(buffer + length) = '\0';

    return buffer;
}

// Replace text string
// REQUIRES: strlen(), strstr(), strncpy(), strcpy()
// WARNING: Allocated memory should be manually freed
char *TextReplace(char *text, const char *replace, const char *by)
{
    // Sanity checks and initialization
    if (!text || !replace || !by) return NULL;

    char *result = NULL;

    char *insertPoint = NULL;   // Next insert point
    char *temp = NULL;          // Temp pointer
    int replaceLen = 0;         // Replace string length of (the string to remove)
    int byLen = 0;              // Replacement length (the string to replace replace by)
    int lastReplacePos = 0;     // Distance between replace and end of last replace
    int count = 0;              // Number of replacements

    replaceLen = TextLength(replace);
    if (replaceLen == 0) return NULL;  // Empty replace causes infinite loop during count

    byLen = TextLength(by);

    // Count the number of replacements needed
    insertPoint = text;
    for (count = 0; (temp = strstr(insertPoint, replace)); count++) insertPoint = temp + replaceLen;

    // Allocate returning string and point temp to it
    temp = result = (char *)RL_MALLOC(TextLength(text) + (byLen - replaceLen)*count + 1);

    if (!result) return NULL;   // Memory could not be allocated

    // First time through the loop, all the variable are set correctly from here on,
    //  - 'temp' points to the end of the result string
    //  - 'insertPoint' points to the next occurrence of replace in text
    //  - 'text' points to the remainder of text after "end of replace"
    while (count--)
    {
        insertPoint = strstr(text, replace);
        lastReplacePos = (int)(insertPoint - text);
        temp = strncpy(temp, text, lastReplacePos) + lastReplacePos;
        temp = strcpy(temp, by) + byLen;
        text += lastReplacePos + replaceLen; // Move to next "end of replace"
    }

    // Copy remaind text part after replacement to result (pointed by moving temp)
    strcpy(temp, text);

    return result;
}

// Insert text in a specific position, moves all text forward
// WARNING: Allocated memory should be manually freed
char *TextInsert(const char *text, const char *insert, int position)
{
    int textLen = TextLength(text);
    int insertLen = TextLength(insert);

    char *result = (char *)RL_MALLOC(textLen + insertLen + 1);

    for (int i = 0; i < position; i++) result[i] = text[i];
    for (int i = position; i < insertLen + position; i++) result[i] = insert[i];
    for (int i = (insertLen + position); i < (textLen + insertLen); i++) result[i] = text[i];

    result[textLen + insertLen] = '\0';     // Make sure text string is valid!

    return result;
}

// Join text strings with delimiter
// REQUIRES: memset(), memcpy()
const char *TextJoin(const char **textList, int count, const char *delimiter)
{
    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);
    char *textPtr = buffer;

    int totalLength = 0;
    int delimiterLen = TextLength(delimiter);

    for (int i = 0; i < count; i++)
    {
        int textLength = TextLength(textList[i]);

        // Make sure joined text could fit inside MAX_TEXT_BUFFER_LENGTH
        if ((totalLength + textLength) < MAX_TEXT_BUFFER_LENGTH)
        {
            memcpy(textPtr, textList[i], textLength);
            totalLength += textLength;
            textPtr += textLength;

            if ((delimiterLen > 0) && (i < (count - 1)))
            {
                memcpy(textPtr, delimiter, delimiterLen);
                totalLength += delimiterLen;
                textPtr += delimiterLen;
            }
        }
    }

    return buffer;
}

// Split string into multiple strings
// REQUIRES: memset()
const char **TextSplit(const char *text, char delimiter, int *count)
{
    // NOTE: Current implementation returns a copy of the provided string with '\0' (string end delimiter)
    // inserted between strings defined by "delimiter" parameter. No memory is dynamically allocated,
    // all used memory is static... it has some limitations:
    //      1. Maximum number of possible split strings is set by MAX_TEXTSPLIT_COUNT
    //      2. Maximum size of text to split is MAX_TEXT_BUFFER_LENGTH

    static const char *result[MAX_TEXTSPLIT_COUNT] = { NULL };
    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    result[0] = buffer;
    int counter = 0;

    if (text != NULL)
    {
        counter = 1;

        // Count how many substrings we have on text and point to every one
        for (int i = 0; i < MAX_TEXT_BUFFER_LENGTH; i++)
        {
            buffer[i] = text[i];
            if (buffer[i] == '\0') break;
            else if (buffer[i] == delimiter)
            {
                buffer[i] = '\0';   // Set an end of string at this point
                result[counter] = buffer + i + 1;
                counter++;

                if (counter == MAX_TEXTSPLIT_COUNT) break;
            }
        }
    }

    *count = counter;
    return result;
}

// Append text at specific position and move cursor!
// REQUIRES: strcpy()
void TextAppend(char *text, const char *append, int *position)
{
    strcpy(text + *position, append);
    *position += TextLength(append);
}

// Find first text occurrence within a string
// REQUIRES: strstr()
int TextFindIndex(const char *text, const char *find)
{
    int position = -1;

    char *ptr = strstr(text, find);

    if (ptr != NULL) position = (int)(ptr - text);

    return position;
}

// Get upper case version of provided string
// REQUIRES: toupper()
const char *TextToUpper(const char *text)
{
    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    for (int i = 0; i < MAX_TEXT_BUFFER_LENGTH; i++)
    {
        if (text[i] != '\0')
        {
            buffer[i] = (char)toupper(text[i]);
            //if ((text[i] >= 'a') && (text[i] <= 'z')) buffer[i] = text[i] - 32;

            // TODO: Support UTF-8 diacritics to upper-case
            //if ((text[i] >= 'à') && (text[i] <= 'ý')) buffer[i] = text[i] - 32;
        }
        else { buffer[i] = '\0'; break; }
    }

    return buffer;
}

// Get lower case version of provided string
// REQUIRES: tolower()
const char *TextToLower(const char *text)
{
    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    for (int i = 0; i < MAX_TEXT_BUFFER_LENGTH; i++)
    {
        if (text[i] != '\0')
        {
            buffer[i] = (char)tolower(text[i]);
            //if ((text[i] >= 'A') && (text[i] <= 'Z')) buffer[i] = text[i] + 32;
        }
        else { buffer[i] = '\0'; break; }
    }

    return buffer;
}

// Get Pascal case notation version of provided string
// REQUIRES: toupper()
const char *TextToPascal(const char *text)
{
    static char buffer[MAX_TEXT_BUFFER_LENGTH] = { 0 };
    memset(buffer, 0, MAX_TEXT_BUFFER_LENGTH);

    buffer[0] = (char)toupper(text[0]);

    for (int i = 1, j = 1; i < MAX_TEXT_BUFFER_LENGTH; i++, j++)
    {
        if (text[j] != '\0')
        {
            if (text[j] != '_') buffer[i] = text[j];
            else
            {
                j++;
                buffer[i] = (char)toupper(text[j]);
            }
        }
        else { buffer[i] = '\0'; break; }
    }

    return buffer;
}

// Encode text codepoint into UTF-8 text
// REQUIRES: memcpy()
// WARNING: Allocated memory should be manually freed
char *TextCodepointsToUTF8(const int *codepoints, int length)
{
    // We allocate enough memory fo fit all possible codepoints
    // NOTE: 5 bytes for every codepoint should be enough
    char *text = (char *)RL_CALLOC(length*5, 1);
    const char *utf8 = NULL;
    int size = 0;

    for (int i = 0, bytes = 0; i < length; i++)
    {
        utf8 = CodepointToUTF8(codepoints[i], &bytes);
        memcpy(text + size, utf8, bytes);
        size += bytes;
    }

    // Resize memory to text length + string NULL terminator
    void *ptr = RL_REALLOC(text, size + 1);

    if (ptr != NULL) text = (char *)ptr;

    return text;
}

// Encode codepoint into utf8 text (char array length returned as parameter)
// NOTE: It uses a static array to store UTF-8 bytes
RLAPI const char *CodepointToUTF8(int codepoint, int *byteSize)
{
    static char utf8[6] = { 0 };
    int size = 0;   // Byte size of codepoint

    if (codepoint <= 0x7f)
    {
        utf8[0] = (char)codepoint;
        size = 1;
    }
    else if (codepoint <= 0x7ff)
    {
        utf8[0] = (char)(((codepoint >> 6) & 0x1f) | 0xc0);
        utf8[1] = (char)((codepoint & 0x3f) | 0x80);
        size = 2;
    }
    else if (codepoint <= 0xffff)
    {
        utf8[0] = (char)(((codepoint >> 12) & 0x0f) | 0xe0);
        utf8[1] = (char)(((codepoint >>  6) & 0x3f) | 0x80);
        utf8[2] = (char)((codepoint & 0x3f) | 0x80);
        size = 3;
    }
    else if (codepoint <= 0x10ffff)
    {
        utf8[0] = (char)(((codepoint >> 18) & 0x07) | 0xf0);
        utf8[1] = (char)(((codepoint >> 12) & 0x3f) | 0x80);
        utf8[2] = (char)(((codepoint >>  6) & 0x3f) | 0x80);
        utf8[3] = (char)((codepoint & 0x3f) | 0x80);
        size = 4;
    }

    *byteSize = size;

    return utf8;
}

// Load all codepoints from a UTF-8 text string, codepoints count returned by parameter
int *LoadCodepoints(const char *text, int *count)
{
    int textLength = TextLength(text);

    int bytesProcessed = 0;
    int codepointCount = 0;

    // Allocate a big enough buffer to store as many codepoints as text bytes
    int *codepoints = RL_CALLOC(textLength, sizeof(int));

    for (int i = 0; i < textLength; codepointCount++)
    {
        codepoints[codepointCount] = GetCodepoint(text + i, &bytesProcessed);
        i += bytesProcessed;
    }

    // Re-allocate buffer to the actual number of codepoints loaded
    void *temp = RL_REALLOC(codepoints, codepointCount*sizeof(int));
    if (temp != NULL) codepoints = temp;

    *count = codepointCount;

    return codepoints;
}

// Unload codepoints data from memory
void UnloadCodepoints(int *codepoints)
{
    RL_FREE(codepoints);
}

// Get total number of characters(codepoints) in a UTF-8 encoded text, until '\0' is found
// NOTE: If an invalid UTF-8 sequence is encountered a '?'(0x3f) codepoint is counted instead
int GetCodepointCount(const char *text)
{
    unsigned int length = 0;
    char *ptr = (char *)&text[0];

    while (*ptr != '\0')
    {
        int next = 0;
        int letter = GetCodepoint(ptr, &next);

        if (letter == 0x3f) ptr += 1;
        else ptr += next;

        length++;
    }

    return length;
}
#endif      // SUPPORT_TEXT_MANIPULATION

// Get next codepoint in a UTF-8 encoded text, scanning until '\0' is found
// When a invalid UTF-8 byte is encountered we exit as soon as possible and a '?'(0x3f) codepoint is returned
// Total number of bytes processed are returned as a parameter
// NOTE: The standard says U+FFFD should be returned in case of errors
// but that character is not supported by the default font in raylib
int GetCodepoint(const char *text, int *bytesProcessed)
{
/*
    UTF-8 specs from https://www.ietf.org/rfc/rfc3629.txt

    Char. number range  |        UTF-8 octet sequence
      (hexadecimal)    |              (binary)
    --------------------+---------------------------------------------
    0000 0000-0000 007F | 0xxxxxxx
    0000 0080-0000 07FF | 110xxxxx 10xxxxxx
    0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
    0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
*/
    // NOTE: on decode errors we return as soon as possible

    int code = 0x3f;   // Codepoint (defaults to '?')
    int octet = (unsigned char)(text[0]); // The first UTF8 octet
    *bytesProcessed = 1;

    if (octet <= 0x7f)
    {
        // Only one octet (ASCII range x00-7F)
        code = text[0];
    }
    else if ((octet & 0xe0) == 0xc0)
    {
        // Two octets

        // [0]xC2-DF    [1]UTF8-tail(x80-BF)
        unsigned char octet1 = text[1];

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) { *bytesProcessed = 2; return code; } // Unexpected sequence

        if ((octet >= 0xc2) && (octet <= 0xdf))
        {
            code = ((octet & 0x1f) << 6) | (octet1 & 0x3f);
            *bytesProcessed = 2;
        }
    }
    else if ((octet & 0xf0) == 0xe0)
    {
        // Three octets
        unsigned char octet1 = text[1];
        unsigned char octet2 = '\0';

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) { *bytesProcessed = 2; return code; } // Unexpected sequence

        octet2 = text[2];

        if ((octet2 == '\0') || ((octet2 >> 6) != 2)) { *bytesProcessed = 3; return code; } // Unexpected sequence

        // [0]xE0    [1]xA0-BF       [2]UTF8-tail(x80-BF)
        // [0]xE1-EC [1]UTF8-tail    [2]UTF8-tail(x80-BF)
        // [0]xED    [1]x80-9F       [2]UTF8-tail(x80-BF)
        // [0]xEE-EF [1]UTF8-tail    [2]UTF8-tail(x80-BF)

        if (((octet == 0xe0) && !((octet1 >= 0xa0) && (octet1 <= 0xbf))) ||
            ((octet == 0xed) && !((octet1 >= 0x80) && (octet1 <= 0x9f)))) { *bytesProcessed = 2; return code; }

        if ((octet >= 0xe0) && (octet <= 0xef))
        {
            code = ((octet & 0xf) << 12) | ((octet1 & 0x3f) << 6) | (octet2 & 0x3f);
            *bytesProcessed = 3;
        }
    }
    else if ((octet & 0xf8) == 0xf0)
    {
        // Four octets
        if (octet > 0xf4) return code;

        unsigned char octet1 = text[1];
        unsigned char octet2 = '\0';
        unsigned char octet3 = '\0';

        if ((octet1 == '\0') || ((octet1 >> 6) != 2)) { *bytesProcessed = 2; return code; }  // Unexpected sequence

        octet2 = text[2];

        if ((octet2 == '\0') || ((octet2 >> 6) != 2)) { *bytesProcessed = 3; return code; }  // Unexpected sequence

        octet3 = text[3];

        if ((octet3 == '\0') || ((octet3 >> 6) != 2)) { *bytesProcessed = 4; return code; }  // Unexpected sequence

        // [0]xF0       [1]x90-BF       [2]UTF8-tail  [3]UTF8-tail
        // [0]xF1-F3    [1]UTF8-tail    [2]UTF8-tail  [3]UTF8-tail
        // [0]xF4       [1]x80-8F       [2]UTF8-tail  [3]UTF8-tail

        if (((octet == 0xf0) && !((octet1 >= 0x90) && (octet1 <= 0xbf))) ||
            ((octet == 0xf4) && !((octet1 >= 0x80) && (octet1 <= 0x8f)))) { *bytesProcessed = 2; return code; } // Unexpected sequence

        if (octet >= 0xf0)
        {
            code = ((octet & 0x7) << 18) | ((octet1 & 0x3f) << 12) | ((octet2 & 0x3f) << 6) | (octet3 & 0x3f);
            *bytesProcessed = 4;
        }
    }

    if (code > 0x10ffff) code = 0x3f;     // Codepoints after U+10ffff are invalid

    return code;
}

//----------------------------------------------------------------------------------
// Module specific Functions Definition
//----------------------------------------------------------------------------------
#if defined(SUPPORT_FILEFORMAT_FNT)

// Read a line from memory
// REQUIRES: memcpy()
// NOTE: Returns the number of bytes read
static int GetLine(const char *origin, char *buffer, int maxLength)
{
    int count = 0;
    for (; count < maxLength; count++) if (origin[count] == '\n') break;
    memcpy(buffer, origin, count);
    return count;
}

// Load a BMFont file (AngelCode font file)
// REQUIRES: strstr(), sscanf(), strrchr(), memcpy()
static Font LoadBMFont(const char *fileName)
{
    #define MAX_BUFFER_SIZE     256

    Font font = { 0 };

    char buffer[MAX_BUFFER_SIZE] = { 0 };
    char *searchPoint = NULL;

    int fontSize = 0;
    int glyphCount = 0;

    int imWidth = 0;
    int imHeight = 0;
    char imFileName[129] = { 0 };

    int base = 0;   // Useless data

    char *fileText = LoadFileText(fileName);

    if (fileText == NULL) return font;

    char *fileTextPtr = fileText;

    // NOTE: We skip first line, it contains no useful information
    int lineBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
    fileTextPtr += (lineBytes + 1);

    // Read line data
    lineBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
    searchPoint = strstr(buffer, "lineHeight");
    sscanf(searchPoint, "lineHeight=%i base=%i scaleW=%i scaleH=%i", &fontSize, &base, &imWidth, &imHeight);
    fileTextPtr += (lineBytes + 1);

    TRACELOGD("FONT: [%s] Loaded font info:", fileName);
    TRACELOGD("    > Base size: %i", fontSize);
    TRACELOGD("    > Texture scale: %ix%i", imWidth, imHeight);

    lineBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
    searchPoint = strstr(buffer, "file");
    sscanf(searchPoint, "file=\"%128[^\"]\"", imFileName);
    fileTextPtr += (lineBytes + 1);

    TRACELOGD("    > Texture filename: %s", imFileName);

    lineBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
    searchPoint = strstr(buffer, "count");
    sscanf(searchPoint, "count=%i", &glyphCount);
    fileTextPtr += (lineBytes + 1);

    TRACELOGD("    > Chars count: %i", glyphCount);

    // Compose correct path using route of .fnt file (fileName) and imFileName
    char *imPath = NULL;
    char *lastSlash = NULL;

    lastSlash = strrchr(fileName, '/');
    if (lastSlash == NULL) lastSlash = strrchr(fileName, '\\');

    if (lastSlash != NULL)
    {
        // NOTE: We need some extra space to avoid memory corruption on next allocations!
        imPath = RL_CALLOC(TextLength(fileName) - TextLength(lastSlash) + TextLength(imFileName) + 4, 1);
        memcpy(imPath, fileName, TextLength(fileName) - TextLength(lastSlash) + 1);
        memcpy(imPath + TextLength(fileName) - TextLength(lastSlash) + 1, imFileName, TextLength(imFileName));
    }
    else imPath = imFileName;

    TRACELOGD("    > Image loading path: %s", imPath);

    Image imFont = LoadImage(imPath);

    if (imFont.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
    {
        // Convert image to GRAYSCALE + ALPHA, using the mask as the alpha channel
        Image imFontAlpha = {
            .data = calloc(imFont.width*imFont.height, 2),
            .width = imFont.width,
            .height = imFont.height,
            .format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA,
            .mipmaps = 1
        };

        for (int p = 0, i = 0; p < (imFont.width*imFont.height*2); p += 2, i++)
        {
            ((unsigned char *)(imFontAlpha.data))[p] = 0xff;
            ((unsigned char *)(imFontAlpha.data))[p + 1] = ((unsigned char *)imFont.data)[i];
        }

        UnloadImage(imFont);
        imFont = imFontAlpha;
    }

    font.texture = LoadTextureFromImage(imFont);

    if (lastSlash != NULL) RL_FREE(imPath);

    // Fill font characters info data
    font.baseSize = fontSize;
    font.glyphCount = glyphCount;
    font.glyphPadding = 0;
    font.glyphs = (GlyphInfo *)RL_MALLOC(glyphCount*sizeof(GlyphInfo));
    font.recs = (Rectangle *)RL_MALLOC(glyphCount*sizeof(Rectangle));

    int charId, charX, charY, charWidth, charHeight, charOffsetX, charOffsetY, charAdvanceX;

    for (int i = 0; i < glyphCount; i++)
    {
        lineBytes = GetLine(fileTextPtr, buffer, MAX_BUFFER_SIZE);
        sscanf(buffer, "char id=%i x=%i y=%i width=%i height=%i xoffset=%i yoffset=%i xadvance=%i",
                       &charId, &charX, &charY, &charWidth, &charHeight, &charOffsetX, &charOffsetY, &charAdvanceX);
        fileTextPtr += (lineBytes + 1);

        // Get character rectangle in the font atlas texture
        font.recs[i] = (Rectangle){ (float)charX, (float)charY, (float)charWidth, (float)charHeight };

        // Save data properly in sprite font
        font.glyphs[i].value = charId;
        font.glyphs[i].offsetX = charOffsetX;
        font.glyphs[i].offsetY = charOffsetY;
        font.glyphs[i].advanceX = charAdvanceX;

        // Fill character image data from imFont data
        font.glyphs[i].image = ImageFromImage(imFont, font.recs[i]);
    }

    UnloadImage(imFont);
    UnloadFileText(fileText);

    if (font.texture.id == 0)
    {
        UnloadFont(font);
        font = GetFontDefault();
        TRACELOG(LOG_WARNING, "FONT: [%s] Failed to load texture, reverted to default font", fileName);
    }
    else TRACELOG(LOG_INFO, "FONT: [%s] Font loaded successfully (%i glyphs)", fileName, font.glyphCount);

    return font;
}
#endif

//*****************************************************************************************************************************************************************************************


#include <math.h>       // Required for: sinf(), asinf(), cosf(), acosf(), sqrtf(), fabsf()
#include <float.h>      // Required for: FLT_EPSILON

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
// Error rate to calculate how many segments we need to draw a smooth circle,
// taken from https://stackoverflow.com/a/2244088
#ifndef SMOOTH_CIRCLE_ERROR_RATE
    #define SMOOTH_CIRCLE_ERROR_RATE    0.5f    // Circle error rate
#endif
#ifndef BEZIER_LINE_DIVISIONS
    #define BEZIER_LINE_DIVISIONS       24      // Bezier line divisions
#endif


//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Not here...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
Texture2D texShapes = { 1, 1, 1, 1, 7 };                // Texture used on shapes drawing (usually a white pixel)
Rectangle texShapesRec = { 0.0f, 0.0f, 1.0f, 1.0f };    // Texture source rectangle used on shapes drawing

//----------------------------------------------------------------------------------
// Module specific Functions Declaration
//----------------------------------------------------------------------------------
static float EaseCubicInOut(float t, float b, float c, float d);    // Cubic easing

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Set texture and rectangle to be used on shapes drawing
// NOTE: It can be useful when using basic shapes and one single font,
// defining a font char white rectangle would allow drawing everything in a single draw call
void SetShapesTexture(Texture2D texture, Rectangle source)
{
    texShapes = texture;
    texShapesRec = source;
}

// Draw a pixel
void DrawPixel(int posX, int posY, Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2i(posX, posY);
        rlVertex2i(posX + 1, posY + 1);
    rlEnd();
}

// Draw a pixel (Vector version)
void DrawPixelV(Vector2 position, Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(position.x, position.y);
        rlVertex2f(position.x + 1.0f, position.y + 1.0f);
    rlEnd();
}

// Draw a line
void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2i(startPosX, startPosY);
        rlVertex2i(endPosX, endPosY);
    rlEnd();
}

// Draw a line  (Vector version)
void DrawLineV(Vector2 startPos, Vector2 endPos, Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(startPos.x, startPos.y);
        rlVertex2f(endPos.x, endPos.y);
    rlEnd();
}

// Draw a line defining thickness
void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color)
{
    Vector2 delta = { endPos.x - startPos.x, endPos.y - startPos.y };
    float length = sqrtf(delta.x*delta.x + delta.y*delta.y);

    if ((length > 0) && (thick > 0))
    {
        float scale = thick/(2*length);
        Vector2 radius = { -scale*delta.y, scale*delta.x };
        Vector2 strip[4] = {
            { startPos.x - radius.x, startPos.y - radius.y },
            { startPos.x + radius.x, startPos.y + radius.y },
            { endPos.x - radius.x, endPos.y - radius.y },
            { endPos.x + radius.x, endPos.y + radius.y }
        };

        DrawTriangleStrip(strip, 4, color);
    }
}

// Draw line using cubic-bezier curves in-out
void DrawLineBezier(Vector2 startPos, Vector2 endPos, float thick, Color color)
{
    Vector2 previous = startPos;
    Vector2 current = { 0 };

    for (int i = 1; i <= BEZIER_LINE_DIVISIONS; i++)
    {
        // Cubic easing in-out
        // NOTE: Easing is calculated only for y position value
        current.y = EaseCubicInOut((float)i, startPos.y, endPos.y - startPos.y, (float)BEZIER_LINE_DIVISIONS);
        current.x = previous.x + (endPos.x - startPos.x)/ (float)BEZIER_LINE_DIVISIONS;

        DrawLineEx(previous, current, thick, color);

        previous = current;
    }
}

// Draw line using quadratic bezier curves with a control point
void DrawLineBezierQuad(Vector2 startPos, Vector2 endPos, Vector2 controlPos, float thick, Color color)
{
    const float step = 1.0f/BEZIER_LINE_DIVISIONS;

    Vector2 previous = startPos;
    Vector2 current = { 0 };
    float t = 0.0f;

    for (int i = 0; i <= BEZIER_LINE_DIVISIONS; i++)
    {
        t = step*i;
        float a = powf(1 - t, 2);
        float b = 2*(1 - t)*t;
        float c = powf(t, 2);

        // NOTE: The easing functions aren't suitable here because they don't take a control point
        current.y = a*startPos.y + b*controlPos.y + c*endPos.y;
        current.x = a*startPos.x + b*controlPos.x + c*endPos.x;

        DrawLineEx(previous, current, thick, color);

        previous = current;
    }
}

// Draw line using cubic bezier curves with 2 control points
void DrawLineBezierCubic(Vector2 startPos, Vector2 endPos, Vector2 startControlPos, Vector2 endControlPos, float thick, Color color)
{
    const float step = 1.0f/BEZIER_LINE_DIVISIONS;

    Vector2 previous = startPos;
    Vector2 current = { 0 };
    float t = 0.0f;

    for (int i = 0; i <= BEZIER_LINE_DIVISIONS; i++)
    {
        t = step*i;
        float a = powf(1 - t, 3);
        float b = 3*powf(1 - t, 2)*t;
        float c = 3*(1-t)*powf(t, 2);
        float d = powf(t, 3);

        current.y = a*startPos.y + b*startControlPos.y + c*endControlPos.y + d*endPos.y;
        current.x = a*startPos.x + b*startControlPos.x + c*endControlPos.x + d*endPos.x;

        DrawLineEx(previous, current, thick, color);

        previous = current;
    }
}

// Draw lines sequence
void DrawLineStrip(Vector2 *points, int pointCount, Color color)
{
    if (pointCount >= 2)
    {
        rlCheckRenderBatchLimit(pointCount);

        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 0; i < pointCount - 1; i++)
            {
                rlVertex2f(points[i].x, points[i].y);
                rlVertex2f(points[i + 1].x, points[i + 1].y);
            }
        rlEnd();
    }
}

// Draw a color-filled circle
void DrawCircle(int centerX, int centerY, float radius, Color color)
{
    DrawCircleV((Vector2){ (float)centerX, (float)centerY }, radius, color);
}

// Draw a piece of a circle
void DrawCircleSector(Vector2 center, float radius, float startAngle, float endAngle, int segments, Color color)
{
    if (radius <= 0.0f) radius = 0.1f;  // Avoid div by zero

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        float tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    int minSegments = (int)ceilf((endAngle - startAngle)/90);

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        float th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/radius, 2) - 1);
        segments = (int)((endAngle - startAngle)*ceilf(2*PI/th)/360);

        if (segments <= 0) segments = minSegments;
    }

    float stepLength = (endAngle - startAngle)/(float)segments;
    float angle = startAngle;

#if defined(SUPPORT_QUADS_DRAW_MODE)
    rlCheckRenderBatchLimit(4*segments/2);

    rlSetTexture(texShapes.id);

    rlBegin(RL_QUADS);
        // NOTE: Every QUAD actually represents two segments
        for (int i = 0; i < segments/2; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(center.x, center.y);

            rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*radius, center.y + cosf(DEG2RAD*angle)*radius);

            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*radius, center.y + cosf(DEG2RAD*(angle + stepLength))*radius);

            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength*2))*radius, center.y + cosf(DEG2RAD*(angle + stepLength*2))*radius);

            angle += (stepLength*2);
        }

        // NOTE: In case number of segments is odd, we add one last piece to the cake
        if (segments%2)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(center.x, center.y);

            rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*radius, center.y + cosf(DEG2RAD*angle)*radius);

            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*radius, center.y + cosf(DEG2RAD*(angle + stepLength))*radius);

            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(center.x, center.y);
        }
    rlEnd();

    rlSetTexture(0);
#else
    rlCheckRenderBatchLimit(3*segments);

    rlBegin(RL_TRIANGLES);
        for (int i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x, center.y);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*radius, center.y + cosf(DEG2RAD*angle)*radius);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*radius, center.y + cosf(DEG2RAD*(angle + stepLength))*radius);

            angle += stepLength;
        }
    rlEnd();
#endif
}

// Draw a piece of a circle outlines
void DrawCircleSectorLines(Vector2 center, float radius, float startAngle, float endAngle, int segments, Color color)
{
    if (radius <= 0.0f) radius = 0.1f;  // Avoid div by zero issue

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        float tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    int minSegments = (int)ceilf((endAngle - startAngle)/90);

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        float th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/radius, 2) - 1);
        segments = (int)((endAngle - startAngle)*ceilf(2*PI/th)/360);

        if (segments <= 0) segments = minSegments;
    }

    float stepLength = (endAngle - startAngle)/(float)segments;
    float angle = startAngle;

    // Hide the cap lines when the circle is full
    bool showCapLines = true;
    int limit = 2*(segments + 2);
    if ((int)(endAngle - startAngle)%360 == 0) { limit = 2*segments; showCapLines = false; }

    rlCheckRenderBatchLimit(limit);

    rlBegin(RL_LINES);
        if (showCapLines)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x, center.y);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*radius, center.y + cosf(DEG2RAD*angle)*radius);
        }

        for (int i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x + sinf(DEG2RAD*angle)*radius, center.y + cosf(DEG2RAD*angle)*radius);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*radius, center.y + cosf(DEG2RAD*(angle + stepLength))*radius);

            angle += stepLength;
        }

        if (showCapLines)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x, center.y);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*radius, center.y + cosf(DEG2RAD*angle)*radius);
        }
    rlEnd();
}

// Draw a gradient-filled circle
// NOTE: Gradient goes from center (color1) to border (color2)
void DrawCircleGradient(int centerX, int centerY, float radius, Color color1, Color color2)
{
    rlCheckRenderBatchLimit(3*36);

    rlBegin(RL_TRIANGLES);
        for (int i = 0; i < 360; i += 10)
        {
            rlColor4ub(color1.r, color1.g, color1.b, color1.a);
            rlVertex2f((float)centerX, (float)centerY);
            rlColor4ub(color2.r, color2.g, color2.b, color2.a);
            rlVertex2f((float)centerX + sinf(DEG2RAD*i)*radius, (float)centerY + cosf(DEG2RAD*i)*radius);
            rlColor4ub(color2.r, color2.g, color2.b, color2.a);
            rlVertex2f((float)centerX + sinf(DEG2RAD*(i + 10))*radius, (float)centerY + cosf(DEG2RAD*(i + 10))*radius);
        }
    rlEnd();
}

// Draw a color-filled circle (Vector version)
// NOTE: On OpenGL 3.3 and ES2 we use QUADS to avoid drawing order issues
void DrawCircleV(Vector2 center, float radius, Color color)
{
    DrawCircleSector(center, radius, 0, 360, 36, color);
}

// Draw circle outline
void DrawCircleLines(int centerX, int centerY, float radius, Color color)
{
    rlCheckRenderBatchLimit(2*36);

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // NOTE: Circle outline is drawn pixel by pixel every degree (0 to 360)
        for (int i = 0; i < 360; i += 10)
        {
            rlVertex2f(centerX + sinf(DEG2RAD*i)*radius, centerY + cosf(DEG2RAD*i)*radius);
            rlVertex2f(centerX + sinf(DEG2RAD*(i + 10))*radius, centerY + cosf(DEG2RAD*(i + 10))*radius);
        }
    rlEnd();
}

// Draw ellipse
void DrawEllipse(int centerX, int centerY, float radiusH, float radiusV, Color color)
{
    rlCheckRenderBatchLimit(3*36);

    rlBegin(RL_TRIANGLES);
        for (int i = 0; i < 360; i += 10)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f((float)centerX, (float)centerY);
            rlVertex2f((float)centerX + sinf(DEG2RAD*i)*radiusH, (float)centerY + cosf(DEG2RAD*i)*radiusV);
            rlVertex2f((float)centerX + sinf(DEG2RAD*(i + 10))*radiusH, (float)centerY + cosf(DEG2RAD*(i + 10))*radiusV);
        }
    rlEnd();
}

// Draw ellipse outline
void DrawEllipseLines(int centerX, int centerY, float radiusH, float radiusV, Color color)
{
    rlCheckRenderBatchLimit(2*36);

    rlBegin(RL_LINES);
        for (int i = 0; i < 360; i += 10)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(centerX + sinf(DEG2RAD*i)*radiusH, centerY + cosf(DEG2RAD*i)*radiusV);
            rlVertex2f(centerX + sinf(DEG2RAD*(i + 10))*radiusH, centerY + cosf(DEG2RAD*(i + 10))*radiusV);
        }
    rlEnd();
}

// Draw ring
void DrawRing(Vector2 center, float innerRadius, float outerRadius, float startAngle, float endAngle, int segments, Color color)
{
    if (startAngle == endAngle) return;

    // Function expects (outerRadius > innerRadius)
    if (outerRadius < innerRadius)
    {
        float tmp = outerRadius;
        outerRadius = innerRadius;
        innerRadius = tmp;

        if (outerRadius <= 0.0f) outerRadius = 0.1f;
    }

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        float tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    int minSegments = (int)ceilf((endAngle - startAngle)/90);

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        float th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/outerRadius, 2) - 1);
        segments = (int)((endAngle - startAngle)*ceilf(2*PI/th)/360);

        if (segments <= 0) segments = minSegments;
    }

    // Not a ring
    if (innerRadius <= 0.0f)
    {
        DrawCircleSector(center, outerRadius, startAngle, endAngle, segments, color);
        return;
    }

    float stepLength = (endAngle - startAngle)/(float)segments;
    float angle = startAngle;

#if defined(SUPPORT_QUADS_DRAW_MODE)
    rlCheckRenderBatchLimit(4*segments);

    rlSetTexture(texShapes.id);

    rlBegin(RL_QUADS);
        for (int i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*innerRadius, center.y + cosf(DEG2RAD*angle)*innerRadius);

            rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*outerRadius, center.y + cosf(DEG2RAD*angle)*outerRadius);

            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*outerRadius);

            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*innerRadius);

            angle += stepLength;
        }
    rlEnd();

    rlSetTexture(0);
#else
    rlCheckRenderBatchLimit(6*segments);

    rlBegin(RL_TRIANGLES);
        for (int i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x + sinf(DEG2RAD*angle)*innerRadius, center.y + cosf(DEG2RAD*angle)*innerRadius);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*outerRadius, center.y + cosf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*innerRadius);

            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*innerRadius);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*outerRadius, center.y + cosf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*outerRadius);

            angle += stepLength;
        }
    rlEnd();
#endif
}

// Draw ring outline
void DrawRingLines(Vector2 center, float innerRadius, float outerRadius, float startAngle, float endAngle, int segments, Color color)
{
    if (startAngle == endAngle) return;

    // Function expects (outerRadius > innerRadius)
    if (outerRadius < innerRadius)
    {
        float tmp = outerRadius;
        outerRadius = innerRadius;
        innerRadius = tmp;

        if (outerRadius <= 0.0f) outerRadius = 0.1f;
    }

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        float tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    int minSegments = (int)ceilf((endAngle - startAngle)/90);

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        float th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/outerRadius, 2) - 1);
        segments = (int)((endAngle - startAngle)*ceilf(2*PI/th)/360);

        if (segments <= 0) segments = minSegments;
    }

    if (innerRadius <= 0.0f)
    {
        DrawCircleSectorLines(center, outerRadius, startAngle, endAngle, segments, color);
        return;
    }

    float stepLength = (endAngle - startAngle)/(float)segments;
    float angle = startAngle;

    bool showCapLines = true;
    int limit = 4*(segments + 1);
    if ((int)(endAngle - startAngle)%360 == 0) { limit = 4*segments; showCapLines = false; }

    rlCheckRenderBatchLimit(limit);

    rlBegin(RL_LINES);
        if (showCapLines)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*outerRadius, center.y + cosf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*innerRadius, center.y + cosf(DEG2RAD*angle)*innerRadius);
        }

        for (int i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x + sinf(DEG2RAD*angle)*outerRadius, center.y + cosf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*outerRadius);

            rlVertex2f(center.x + sinf(DEG2RAD*angle)*innerRadius, center.y + cosf(DEG2RAD*angle)*innerRadius);
            rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*innerRadius);

            angle += stepLength;
        }

        if (showCapLines)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*outerRadius, center.y + cosf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + sinf(DEG2RAD*angle)*innerRadius, center.y + cosf(DEG2RAD*angle)*innerRadius);
        }
    rlEnd();
}

// Draw a color-filled rectangle
void DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    DrawRectangleV((Vector2){ (float)posX, (float)posY }, (Vector2){ (float)width, (float)height }, color);
}

// Draw a color-filled rectangle (Vector version)
// NOTE: On OpenGL 3.3 and ES2 we use QUADS to avoid drawing order issues
void DrawRectangleV(Vector2 position, Vector2 size, Color color)
{
    DrawRectanglePro((Rectangle){ position.x, position.y, size.x, size.y }, (Vector2){ 0.0f, 0.0f }, 0.0f, color);
}

// Draw a color-filled rectangle
void DrawRectangleRec(Rectangle rec, Color color)
{
    DrawRectanglePro(rec, (Vector2){ 0.0f, 0.0f }, 0.0f, color);
}

// Draw a color-filled rectangle with pro parameters
void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation, Color color)
{
    Vector2 topLeft = { 0 };
    Vector2 topRight = { 0 };
    Vector2 bottomLeft = { 0 };
    Vector2 bottomRight = { 0 };

    // Only calculate rotation if needed
    if (rotation == 0.0f)
    {
        float x = rec.x - origin.x;
        float y = rec.y - origin.y;
        topLeft = (Vector2){ x, y };
        topRight = (Vector2){ x + rec.width, y };
        bottomLeft = (Vector2){ x, y + rec.height };
        bottomRight = (Vector2){ x + rec.width, y + rec.height };
    }
    else
    {
        float sinRotation = sinf(rotation*DEG2RAD);
        float cosRotation = cosf(rotation*DEG2RAD);
        float x = rec.x;
        float y = rec.y;
        float dx = -origin.x;
        float dy = -origin.y;

        topLeft.x = x + dx*cosRotation - dy*sinRotation;
        topLeft.y = y + dx*sinRotation + dy*cosRotation;

        topRight.x = x + (dx + rec.width)*cosRotation - dy*sinRotation;
        topRight.y = y + (dx + rec.width)*sinRotation + dy*cosRotation;

        bottomLeft.x = x + dx*cosRotation - (dy + rec.height)*sinRotation;
        bottomLeft.y = y + dx*sinRotation + (dy + rec.height)*cosRotation;

        bottomRight.x = x + (dx + rec.width)*cosRotation - (dy + rec.height)*sinRotation;
        bottomRight.y = y + (dx + rec.width)*sinRotation + (dy + rec.height)*cosRotation;
    }

#if defined(SUPPORT_QUADS_DRAW_MODE)
    rlCheckRenderBatchLimit(4);

    rlSetTexture(texShapes.id);

    rlBegin(RL_QUADS);

        rlNormal3f(0.0f, 0.0f, 1.0f);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(topLeft.x, topLeft.y);

        rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(bottomLeft.x, bottomLeft.y);

        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(bottomRight.x, bottomRight.y);

        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(topRight.x, topRight.y);

    rlEnd();

    rlSetTexture(0);
#else
    rlCheckRenderBatchLimit(6);

    rlBegin(RL_TRIANGLES);

        rlColor4ub(color.r, color.g, color.b, color.a);

        rlVertex2f(topLeft.x, topLeft.y);
        rlVertex2f(bottomLeft.x, bottomLeft.y);
        rlVertex2f(topRight.x, topRight.y);

        rlVertex2f(topRight.x, topRight.y);
        rlVertex2f(bottomLeft.x, bottomLeft.y);
        rlVertex2f(bottomRight.x, bottomRight.y);

    rlEnd();
#endif
}

// Draw a vertical-gradient-filled rectangle
// NOTE: Gradient goes from bottom (color1) to top (color2)
void DrawRectangleGradientV(int posX, int posY, int width, int height, Color color1, Color color2)
{
    DrawRectangleGradientEx((Rectangle){ (float)posX, (float)posY, (float)width, (float)height }, color1, color2, color2, color1);
}

// Draw a horizontal-gradient-filled rectangle
// NOTE: Gradient goes from bottom (color1) to top (color2)
void DrawRectangleGradientH(int posX, int posY, int width, int height, Color color1, Color color2)
{
    DrawRectangleGradientEx((Rectangle){ (float)posX, (float)posY, (float)width, (float)height }, color1, color1, color2, color2);
}

// Draw a gradient-filled rectangle
// NOTE: Colors refer to corners, starting at top-lef corner and counter-clockwise
void DrawRectangleGradientEx(Rectangle rec, Color col1, Color col2, Color col3, Color col4)
{
    rlCheckRenderBatchLimit(4);

    rlSetTexture(texShapes.id);

    rlPushMatrix();
        rlBegin(RL_QUADS);
            rlNormal3f(0.0f, 0.0f, 1.0f);

            // NOTE: Default raylib font character 95 is a white square
            rlColor4ub(col1.r, col1.g, col1.b, col1.a);
            rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(rec.x, rec.y);

            rlColor4ub(col2.r, col2.g, col2.b, col2.a);
            rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(rec.x, rec.y + rec.height);

            rlColor4ub(col3.r, col3.g, col3.b, col3.a);
            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(rec.x + rec.width, rec.y + rec.height);

            rlColor4ub(col4.r, col4.g, col4.b, col4.a);
            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(rec.x + rec.width, rec.y);
        rlEnd();
    rlPopMatrix();

    rlSetTexture(0);
}

// Draw rectangle outline
// NOTE: On OpenGL 3.3 and ES2 we use QUADS to avoid drawing order issues
void DrawRectangleLines(int posX, int posY, int width, int height, Color color)
{
#if defined(SUPPORT_QUADS_DRAW_MODE)
    DrawRectangle(posX, posY, width, 1, color);
    DrawRectangle(posX + width - 1, posY + 1, 1, height - 2, color);
    DrawRectangle(posX, posY + height - 1, width, 1, color);
    DrawRectangle(posX, posY + 1, 1, height - 2, color);
#else
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2i(posX + 1, posY + 1);
        rlVertex2i(posX + width, posY + 1);

        rlVertex2i(posX + width, posY + 1);
        rlVertex2i(posX + width, posY + height);

        rlVertex2i(posX + width, posY + height);
        rlVertex2i(posX + 1, posY + height);

        rlVertex2i(posX + 1, posY + height);
        rlVertex2i(posX + 1, posY + 1);
    rlEnd();
#endif
}

// Draw rectangle outline with extended parameters
void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color)
{
    if ((lineThick > rec.width) || (lineThick > rec.height))
    {
        if (rec.width > rec.height) lineThick = rec.height/2;
        else if (rec.width < rec.height) lineThick = rec.width/2;
    }

    // When rec = { x, y, 8.0f, 6.0f } and lineThick = 2, the following
    // four rectangles are drawn ([T]op, [B]ottom, [L]eft, [R]ight):
    //
    //   TTTTTTTT
    //   TTTTTTTT
    //   LL    RR
    //   LL    RR
    //   BBBBBBBB
    //   BBBBBBBB
    //

    Rectangle top = { rec.x, rec.y, rec.width, lineThick };
    Rectangle bottom = { rec.x, rec.y - lineThick + rec.height, rec.width, lineThick };
    Rectangle left = { rec.x, rec.y + lineThick, lineThick, rec.height - lineThick*2.0f };
    Rectangle right = { rec.x - lineThick + rec.width, rec.y + lineThick, lineThick, rec.height - lineThick*2.0f };

    DrawRectangleRec(top, color);
    DrawRectangleRec(bottom, color);
    DrawRectangleRec(left, color);
    DrawRectangleRec(right, color);
}

// Draw rectangle with rounded edges
void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color)
{
    // Not a rounded rectangle
    if ((roundness <= 0.0f) || (rec.width < 1) || (rec.height < 1 ))
    {
        DrawRectangleRec(rec, color);
        return;
    }

    if (roundness >= 1.0f) roundness = 1.0f;

    // Calculate corner radius
    float radius = (rec.width > rec.height)? (rec.height*roundness)/2 : (rec.width*roundness)/2;
    if (radius <= 0.0f) return;

    // Calculate number of segments to use for the corners
    if (segments < 4)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        float th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/radius, 2) - 1);
        segments = (int)(ceilf(2*PI/th)/4.0f);
        if (segments <= 0) segments = 4;
    }

    float stepLength = 90.0f/(float)segments;

    /*
    Quick sketch to make sense of all of this,
    there are 9 parts to draw, also mark the 12 points we'll use

          P0____________________P1
          /|                    |\
         /1|          2         |3\
     P7 /__|____________________|__\ P2
       |   |P8                P9|   |
       | 8 |          9         | 4 |
       | __|____________________|__ |
     P6 \  |P11              P10|  / P3
         \7|          6         |5/
          \|____________________|/
          P5                    P4
    */
    // Coordinates of the 12 points that define the rounded rect
    const Vector2 point[12] = {
        {(float)rec.x + radius, rec.y}, {(float)(rec.x + rec.width) - radius, rec.y}, { rec.x + rec.width, (float)rec.y + radius },     // PO, P1, P2
        {rec.x + rec.width, (float)(rec.y + rec.height) - radius}, {(float)(rec.x + rec.width) - radius, rec.y + rec.height},           // P3, P4
        {(float)rec.x + radius, rec.y + rec.height}, { rec.x, (float)(rec.y + rec.height) - radius}, {rec.x, (float)rec.y + radius},    // P5, P6, P7
        {(float)rec.x + radius, (float)rec.y + radius}, {(float)(rec.x + rec.width) - radius, (float)rec.y + radius},                   // P8, P9
        {(float)(rec.x + rec.width) - radius, (float)(rec.y + rec.height) - radius}, {(float)rec.x + radius, (float)(rec.y + rec.height) - radius} // P10, P11
    };

    const Vector2 centers[4] = { point[8], point[9], point[10], point[11] };
    const float angles[4] = { 180.0f, 90.0f, 0.0f, 270.0f };

#if defined(SUPPORT_QUADS_DRAW_MODE)
    rlCheckRenderBatchLimit(16*segments/2 + 5*4);

    rlSetTexture(texShapes.id);

    rlBegin(RL_QUADS);
        // Draw all of the 4 corners: [1] Upper Left Corner, [3] Upper Right Corner, [5] Lower Right Corner, [7] Lower Left Corner
        for (int k = 0; k < 4; ++k) // Hope the compiler is smart enough to unroll this loop
        {
            float angle = angles[k];
            const Vector2 center = centers[k];

            // NOTE: Every QUAD actually represents two segments
            for (int i = 0; i < segments/2; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
                rlVertex2f(center.x, center.y);
                rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                rlVertex2f(center.x + sinf(DEG2RAD*angle)*radius, center.y + cosf(DEG2RAD*angle)*radius);
                rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*radius, center.y + cosf(DEG2RAD*(angle + stepLength))*radius);
                rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
                rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength*2))*radius, center.y + cosf(DEG2RAD*(angle + stepLength*2))*radius);
                angle += (stepLength*2);
            }

            // NOTE: In case number of segments is odd, we add one last piece to the cake
            if (segments%2)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
                rlVertex2f(center.x, center.y);
                rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                rlVertex2f(center.x + sinf(DEG2RAD*angle)*radius, center.y + cosf(DEG2RAD*angle)*radius);
                rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*radius, center.y + cosf(DEG2RAD*(angle + stepLength))*radius);
                rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
                rlVertex2f(center.x, center.y);
            }
        }

        // [2] Upper Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(point[0].x, point[0].y);
        rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(point[8].x, point[8].y);
        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(point[9].x, point[9].y);
        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(point[1].x, point[1].y);

        // [4] Right Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(point[2].x, point[2].y);
        rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(point[9].x, point[9].y);
        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(point[10].x, point[10].y);
        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(point[3].x, point[3].y);

        // [6] Bottom Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(point[11].x, point[11].y);
        rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(point[5].x, point[5].y);
        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(point[4].x, point[4].y);
        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(point[10].x, point[10].y);

        // [8] Left Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(point[7].x, point[7].y);
        rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(point[6].x, point[6].y);
        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(point[11].x, point[11].y);
        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(point[8].x, point[8].y);

        // [9] Middle Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(point[8].x, point[8].y);
        rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(point[11].x, point[11].y);
        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(point[10].x, point[10].y);
        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(point[9].x, point[9].y);

    rlEnd();
    rlSetTexture(0);
#else
    rlCheckRenderBatchLimit(12*segments + 5*6); // 4 corners with 3 vertices per segment + 5 rectangles with 6 vertices each

    rlBegin(RL_TRIANGLES);

        // Draw all of the 4 corners: [1] Upper Left Corner, [3] Upper Right Corner, [5] Lower Right Corner, [7] Lower Left Corner
        for (int k = 0; k < 4; ++k) // Hope the compiler is smart enough to unroll this loop
        {
            float angle = angles[k];
            const Vector2 center = centers[k];
            for (int i = 0; i < segments; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlVertex2f(center.x, center.y);
                rlVertex2f(center.x + sinf(DEG2RAD*angle)*radius, center.y + cosf(DEG2RAD*angle)*radius);
                rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*radius, center.y + cosf(DEG2RAD*(angle + stepLength))*radius);
                angle += stepLength;
            }
        }

        // [2] Upper Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(point[0].x, point[0].y);
        rlVertex2f(point[8].x, point[8].y);
        rlVertex2f(point[9].x, point[9].y);
        rlVertex2f(point[1].x, point[1].y);
        rlVertex2f(point[0].x, point[0].y);
        rlVertex2f(point[9].x, point[9].y);

        // [4] Right Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(point[9].x, point[9].y);
        rlVertex2f(point[10].x, point[10].y);
        rlVertex2f(point[3].x, point[3].y);
        rlVertex2f(point[2].x, point[2].y);
        rlVertex2f(point[9].x, point[9].y);
        rlVertex2f(point[3].x, point[3].y);

        // [6] Bottom Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(point[11].x, point[11].y);
        rlVertex2f(point[5].x, point[5].y);
        rlVertex2f(point[4].x, point[4].y);
        rlVertex2f(point[10].x, point[10].y);
        rlVertex2f(point[11].x, point[11].y);
        rlVertex2f(point[4].x, point[4].y);

        // [8] Left Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(point[7].x, point[7].y);
        rlVertex2f(point[6].x, point[6].y);
        rlVertex2f(point[11].x, point[11].y);
        rlVertex2f(point[8].x, point[8].y);
        rlVertex2f(point[7].x, point[7].y);
        rlVertex2f(point[11].x, point[11].y);

        // [9] Middle Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(point[8].x, point[8].y);
        rlVertex2f(point[11].x, point[11].y);
        rlVertex2f(point[10].x, point[10].y);
        rlVertex2f(point[9].x, point[9].y);
        rlVertex2f(point[8].x, point[8].y);
        rlVertex2f(point[10].x, point[10].y);
    rlEnd();
#endif
}

// Draw rectangle with rounded edges outline
void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments, float lineThick, Color color)
{
    if (lineThick < 0) lineThick = 0;

    // Not a rounded rectangle
    if (roundness <= 0.0f)
    {
        DrawRectangleLinesEx((Rectangle){rec.x-lineThick, rec.y-lineThick, rec.width+2*lineThick, rec.height+2*lineThick}, lineThick, color);
        return;
    }

    if (roundness >= 1.0f) roundness = 1.0f;

    // Calculate corner radius
    float radius = (rec.width > rec.height)? (rec.height*roundness)/2 : (rec.width*roundness)/2;
    if (radius <= 0.0f) return;

    // Calculate number of segments to use for the corners
    if (segments < 4)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        float th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/radius, 2) - 1);
        segments = (int)(ceilf(2*PI/th)/2.0f);
        if (segments <= 0) segments = 4;
    }

    float stepLength = 90.0f/(float)segments;
    const float outerRadius = radius + lineThick, innerRadius = radius;

    /*
    Quick sketch to make sense of all of this,
    marks the 16 + 4(corner centers P16-19) points we'll use

           P0 ================== P1
          // P8                P9 \\
         //                        \\
     P7 // P15                  P10 \\ P2
       ||   *P16             P17*    ||
       ||                            ||
       || P14                   P11  ||
     P6 \\  *P19             P18*   // P3
         \\                        //
          \\ P13              P12 //
           P5 ================== P4
    */
    const Vector2 point[16] = {
        {(float)rec.x + innerRadius, rec.y - lineThick}, {(float)(rec.x + rec.width) - innerRadius, rec.y - lineThick}, { rec.x + rec.width + lineThick, (float)rec.y + innerRadius }, // PO, P1, P2
        {rec.x + rec.width + lineThick, (float)(rec.y + rec.height) - innerRadius}, {(float)(rec.x + rec.width) - innerRadius, rec.y + rec.height + lineThick}, // P3, P4
        {(float)rec.x + innerRadius, rec.y + rec.height + lineThick}, { rec.x - lineThick, (float)(rec.y + rec.height) - innerRadius}, {rec.x - lineThick, (float)rec.y + innerRadius}, // P5, P6, P7
        {(float)rec.x + innerRadius, rec.y}, {(float)(rec.x + rec.width) - innerRadius, rec.y}, // P8, P9
        { rec.x + rec.width, (float)rec.y + innerRadius }, {rec.x + rec.width, (float)(rec.y + rec.height) - innerRadius}, // P10, P11
        {(float)(rec.x + rec.width) - innerRadius, rec.y + rec.height}, {(float)rec.x + innerRadius, rec.y + rec.height}, // P12, P13
        { rec.x, (float)(rec.y + rec.height) - innerRadius}, {rec.x, (float)rec.y + innerRadius} // P14, P15
    };

    const Vector2 centers[4] = {
        {(float)rec.x + innerRadius, (float)rec.y + innerRadius}, {(float)(rec.x + rec.width) - innerRadius, (float)rec.y + innerRadius}, // P16, P17
        {(float)(rec.x + rec.width) - innerRadius, (float)(rec.y + rec.height) - innerRadius}, {(float)rec.x + innerRadius, (float)(rec.y + rec.height) - innerRadius} // P18, P19
    };

    const float angles[4] = { 180.0f, 90.0f, 0.0f, 270.0f };

    if (lineThick > 1)
    {
#if defined(SUPPORT_QUADS_DRAW_MODE)
        rlCheckRenderBatchLimit(4*4*segments + 4*4); // 4 corners with 4 vertices for each segment + 4 rectangles with 4 vertices each

        rlSetTexture(texShapes.id);

        rlBegin(RL_QUADS);

            // Draw all of the 4 corners first: Upper Left Corner, Upper Right Corner, Lower Right Corner, Lower Left Corner
            for (int k = 0; k < 4; ++k) // Hope the compiler is smart enough to unroll this loop
            {
                float angle = angles[k];
                const Vector2 center = centers[k];
                for (int i = 0; i < segments; i++)
                {
                    rlColor4ub(color.r, color.g, color.b, color.a);
                    rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
                    rlVertex2f(center.x + sinf(DEG2RAD*angle)*innerRadius, center.y + cosf(DEG2RAD*angle)*innerRadius);
                    rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                    rlVertex2f(center.x + sinf(DEG2RAD*angle)*outerRadius, center.y + cosf(DEG2RAD*angle)*outerRadius);
                    rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                    rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*outerRadius);
                    rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
                    rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*innerRadius);

                    angle += stepLength;
                }
            }

            // Upper rectangle
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(point[0].x, point[0].y);
            rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(point[8].x, point[8].y);
            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(point[9].x, point[9].y);
            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(point[1].x, point[1].y);

            // Right rectangle
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(point[2].x, point[2].y);
            rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(point[10].x, point[10].y);
            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(point[11].x, point[11].y);
            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(point[3].x, point[3].y);

            // Lower rectangle
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(point[13].x, point[13].y);
            rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(point[5].x, point[5].y);
            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(point[4].x, point[4].y);
            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(point[12].x, point[12].y);

            // Left rectangle
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(point[15].x, point[15].y);
            rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(point[7].x, point[7].y);
            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
            rlVertex2f(point[6].x, point[6].y);
            rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
            rlVertex2f(point[14].x, point[14].y);

        rlEnd();
        rlSetTexture(0);
#else
        rlCheckRenderBatchLimit(4*6*segments + 4*6); // 4 corners with 6(2*3) vertices for each segment + 4 rectangles with 6 vertices each

        rlBegin(RL_TRIANGLES);

            // Draw all of the 4 corners first: Upper Left Corner, Upper Right Corner, Lower Right Corner, Lower Left Corner
            for (int k = 0; k < 4; ++k) // Hope the compiler is smart enough to unroll this loop
            {
                float angle = angles[k];
                const Vector2 center = centers[k];

                for (int i = 0; i < segments; i++)
                {
                    rlColor4ub(color.r, color.g, color.b, color.a);

                    rlVertex2f(center.x + sinf(DEG2RAD*angle)*innerRadius, center.y + cosf(DEG2RAD*angle)*innerRadius);
                    rlVertex2f(center.x + sinf(DEG2RAD*angle)*outerRadius, center.y + cosf(DEG2RAD*angle)*outerRadius);
                    rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*innerRadius);

                    rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*innerRadius);
                    rlVertex2f(center.x + sinf(DEG2RAD*angle)*outerRadius, center.y + cosf(DEG2RAD*angle)*outerRadius);
                    rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*outerRadius);

                    angle += stepLength;
                }
            }

            // Upper rectangle
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(point[0].x, point[0].y);
            rlVertex2f(point[8].x, point[8].y);
            rlVertex2f(point[9].x, point[9].y);
            rlVertex2f(point[1].x, point[1].y);
            rlVertex2f(point[0].x, point[0].y);
            rlVertex2f(point[9].x, point[9].y);

            // Right rectangle
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(point[10].x, point[10].y);
            rlVertex2f(point[11].x, point[11].y);
            rlVertex2f(point[3].x, point[3].y);
            rlVertex2f(point[2].x, point[2].y);
            rlVertex2f(point[10].x, point[10].y);
            rlVertex2f(point[3].x, point[3].y);

            // Lower rectangle
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(point[13].x, point[13].y);
            rlVertex2f(point[5].x, point[5].y);
            rlVertex2f(point[4].x, point[4].y);
            rlVertex2f(point[12].x, point[12].y);
            rlVertex2f(point[13].x, point[13].y);
            rlVertex2f(point[4].x, point[4].y);

            // Left rectangle
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(point[7].x, point[7].y);
            rlVertex2f(point[6].x, point[6].y);
            rlVertex2f(point[14].x, point[14].y);
            rlVertex2f(point[15].x, point[15].y);
            rlVertex2f(point[7].x, point[7].y);
            rlVertex2f(point[14].x, point[14].y);
        rlEnd();
#endif
    }
    else
    {
        // Use LINES to draw the outline
        rlCheckRenderBatchLimit(8*segments + 4*2); // 4 corners with 2 vertices for each segment + 4 rectangles with 2 vertices each

        rlBegin(RL_LINES);

            // Draw all of the 4 corners first: Upper Left Corner, Upper Right Corner, Lower Right Corner, Lower Left Corner
            for (int k = 0; k < 4; ++k) // Hope the compiler is smart enough to unroll this loop
            {
                float angle = angles[k];
                const Vector2 center = centers[k];

                for (int i = 0; i < segments; i++)
                {
                    rlColor4ub(color.r, color.g, color.b, color.a);
                    rlVertex2f(center.x + sinf(DEG2RAD*angle)*outerRadius, center.y + cosf(DEG2RAD*angle)*outerRadius);
                    rlVertex2f(center.x + sinf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + cosf(DEG2RAD*(angle + stepLength))*outerRadius);
                    angle += stepLength;
                }
            }

            // And now the remaining 4 lines
            for (int i = 0; i < 8; i += 2)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlVertex2f(point[i].x, point[i].y);
                rlVertex2f(point[i + 1].x, point[i + 1].y);
            }

        rlEnd();
    }
}

// Draw a triangle
// NOTE: Vertex must be provided in counter-clockwise order
void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
#if defined(SUPPORT_QUADS_DRAW_MODE)
    rlCheckRenderBatchLimit(4);

    rlSetTexture(texShapes.id);

    rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(v1.x, v1.y);

        rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(v2.x, v2.y);

        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
        rlVertex2f(v2.x, v2.y);

        rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
        rlVertex2f(v3.x, v3.y);
    rlEnd();

    rlSetTexture(0);
#else
    rlCheckRenderBatchLimit(3);

    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(v1.x, v1.y);
        rlVertex2f(v2.x, v2.y);
        rlVertex2f(v3.x, v3.y);
    rlEnd();
#endif
}

// Draw a triangle using lines
// NOTE: Vertex must be provided in counter-clockwise order
void DrawTriangleLines(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    rlCheckRenderBatchLimit(6);

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(v1.x, v1.y);
        rlVertex2f(v2.x, v2.y);

        rlVertex2f(v2.x, v2.y);
        rlVertex2f(v3.x, v3.y);

        rlVertex2f(v3.x, v3.y);
        rlVertex2f(v1.x, v1.y);
    rlEnd();
}

// Draw a triangle fan defined by points
// NOTE: First vertex provided is the center, shared by all triangles
// By default, following vertex should be provided in counter-clockwise order
void DrawTriangleFan(Vector2 *points, int pointCount, Color color)
{
    if (pointCount >= 3)
    {
        rlCheckRenderBatchLimit((pointCount - 2)*4);

        rlSetTexture(texShapes.id);
        rlBegin(RL_QUADS);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 1; i < pointCount - 1; i++)
            {
                rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
                rlVertex2f(points[0].x, points[0].y);

                rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                rlVertex2f(points[i].x, points[i].y);

                rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                rlVertex2f(points[i + 1].x, points[i + 1].y);

                rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
                rlVertex2f(points[i + 1].x, points[i + 1].y);
            }
        rlEnd();
        rlSetTexture(0);
    }
}

// Draw a triangle strip defined by points
// NOTE: Every new vertex connects with previous two
void DrawTriangleStrip(Vector2 *points, int pointCount, Color color)
{
    if (pointCount >= 3)
    {
        rlCheckRenderBatchLimit(3*(pointCount - 2));

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 2; i < pointCount; i++)
            {
                if ((i%2) == 0)
                {
                    rlVertex2f(points[i].x, points[i].y);
                    rlVertex2f(points[i - 2].x, points[i - 2].y);
                    rlVertex2f(points[i - 1].x, points[i - 1].y);
                }
                else
                {
                    rlVertex2f(points[i].x, points[i].y);
                    rlVertex2f(points[i - 1].x, points[i - 1].y);
                    rlVertex2f(points[i - 2].x, points[i - 2].y);
                }
            }
        rlEnd();
    }
}

// Draw a regular polygon of n sides (Vector version)
void DrawPoly(Vector2 center, int sides, float radius, float rotation, Color color)
{
    if (sides < 3) sides = 3;
    float centralAngle = 0.0f;

#if defined(SUPPORT_QUADS_DRAW_MODE)
    rlCheckRenderBatchLimit(4*sides); // Each side is a quad
#else
    rlCheckRenderBatchLimit(3*sides);
#endif

    rlPushMatrix();
        rlTranslatef(center.x, center.y, 0.0f);
        rlRotatef(rotation, 0.0f, 0.0f, 1.0f);

#if defined(SUPPORT_QUADS_DRAW_MODE)
        rlSetTexture(texShapes.id);

        rlBegin(RL_QUADS);
            for (int i = 0; i < sides; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);

                rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
                rlVertex2f(0, 0);

                rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                rlVertex2f(sinf(DEG2RAD*centralAngle)*radius, cosf(DEG2RAD*centralAngle)*radius);

                rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                rlVertex2f(sinf(DEG2RAD*centralAngle)*radius, cosf(DEG2RAD*centralAngle)*radius);

                centralAngle += 360.0f/(float)sides;
                rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
                rlVertex2f(sinf(DEG2RAD*centralAngle)*radius, cosf(DEG2RAD*centralAngle)*radius);
            }
        rlEnd();
        rlSetTexture(0);
#else
        rlBegin(RL_TRIANGLES);
            for (int i = 0; i < sides; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);

                rlVertex2f(0, 0);
                rlVertex2f(sinf(DEG2RAD*centralAngle)*radius, cosf(DEG2RAD*centralAngle)*radius);

                centralAngle += 360.0f/(float)sides;
                rlVertex2f(sinf(DEG2RAD*centralAngle)*radius, cosf(DEG2RAD*centralAngle)*radius);
            }
        rlEnd();
#endif
    rlPopMatrix();
}

// Draw a polygon outline of n sides
void DrawPolyLines(Vector2 center, int sides, float radius, float rotation, Color color)
{
    if (sides < 3) sides = 3;
    float centralAngle = 0.0f;

    rlCheckRenderBatchLimit(2*sides);

    rlPushMatrix();
        rlTranslatef(center.x, center.y, 0.0f);
        rlRotatef(rotation, 0.0f, 0.0f, 1.0f);

        rlBegin(RL_LINES);
            for (int i = 0; i < sides; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);

                rlVertex2f(sinf(DEG2RAD*centralAngle)*radius, cosf(DEG2RAD*centralAngle)*radius);
                centralAngle += 360.0f/(float)sides;
                rlVertex2f(sinf(DEG2RAD*centralAngle)*radius, cosf(DEG2RAD*centralAngle)*radius);
            }
        rlEnd();
    rlPopMatrix();
}

void DrawPolyLinesEx(Vector2 center, int sides, float radius, float rotation, float lineThick, Color color)
{
    if (sides < 3) sides = 3;
    float centralAngle = 0.0f;
    float exteriorAngle = 360.0f/(float)sides;
    float innerRadius = radius - (lineThick*cosf(DEG2RAD*exteriorAngle/2.0f));

#if defined(SUPPORT_QUADS_DRAW_MODE)
    rlCheckRenderBatchLimit(4*sides);
#else
    rlCheckRenderBatchLimit(6*sides);
#endif

    rlPushMatrix();
        rlTranslatef(center.x, center.y, 0.0f);
        rlRotatef(rotation, 0.0f, 0.0f, 1.0f);

#if defined(SUPPORT_QUADS_DRAW_MODE)
        rlSetTexture(texShapes.id);

        rlBegin(RL_QUADS);
            for (int i = 0; i < sides; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);

                rlTexCoord2f(texShapesRec.x/texShapes.width, texShapesRec.y/texShapes.height);
                rlVertex2f(sinf(DEG2RAD*centralAngle)*innerRadius, cosf(DEG2RAD*centralAngle)*innerRadius);

                rlTexCoord2f(texShapesRec.x/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                rlVertex2f(sinf(DEG2RAD*centralAngle)*radius, cosf(DEG2RAD*centralAngle)*radius);

                centralAngle += exteriorAngle;
                rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, texShapesRec.y/texShapes.height);
                rlVertex2f(sinf(DEG2RAD*centralAngle)*radius, cosf(DEG2RAD*centralAngle)*radius);

                rlTexCoord2f((texShapesRec.x + texShapesRec.width)/texShapes.width, (texShapesRec.y + texShapesRec.height)/texShapes.height);
                rlVertex2f(sinf(DEG2RAD*centralAngle)*innerRadius, cosf(DEG2RAD*centralAngle)*innerRadius);
            }
        rlEnd();
        rlSetTexture(0);
#else
        rlBegin(RL_TRIANGLES);
            for (int i = 0; i < sides; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                float nextAngle = centralAngle + exteriorAngle;

                rlVertex2f(sinf(DEG2RAD*centralAngle)*radius, cosf(DEG2RAD*centralAngle)*radius);
                rlVertex2f(sinf(DEG2RAD*centralAngle)*innerRadius, cosf(DEG2RAD*centralAngle)*innerRadius);
                rlVertex2f(sinf(DEG2RAD*nextAngle)*radius, cosf(DEG2RAD*nextAngle)*radius);

                rlVertex2f(sinf(DEG2RAD*centralAngle)*innerRadius, cosf(DEG2RAD*centralAngle)*innerRadius);
                rlVertex2f(sinf(DEG2RAD*nextAngle)*radius, cosf(DEG2RAD*nextAngle)*radius);
                rlVertex2f(sinf(DEG2RAD*nextAngle)*innerRadius, cosf(DEG2RAD*nextAngle)*innerRadius);

                centralAngle = nextAngle;
            }
        rlEnd();
#endif
    rlPopMatrix();
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Collision Detection functions
//----------------------------------------------------------------------------------

// Check if point is inside rectangle
bool CheckCollisionPointRec(Vector2 point, Rectangle rec)
{
    bool collision = false;

    if ((point.x >= rec.x) && (point.x <= (rec.x + rec.width)) && (point.y >= rec.y) && (point.y <= (rec.y + rec.height))) collision = true;

    return collision;
}

// Check if point is inside circle
bool CheckCollisionPointCircle(Vector2 point, Vector2 center, float radius)
{
    bool collision = false;

    collision = CheckCollisionCircles(point, 0, center, radius);

    return collision;
}

// Check if point is inside a triangle defined by three points (p1, p2, p3)
bool CheckCollisionPointTriangle(Vector2 point, Vector2 p1, Vector2 p2, Vector2 p3)
{
    bool collision = false;

    float alpha = ((p2.y - p3.y)*(point.x - p3.x) + (p3.x - p2.x)*(point.y - p3.y)) /
                  ((p2.y - p3.y)*(p1.x - p3.x) + (p3.x - p2.x)*(p1.y - p3.y));

    float beta = ((p3.y - p1.y)*(point.x - p3.x) + (p1.x - p3.x)*(point.y - p3.y)) /
                 ((p2.y - p3.y)*(p1.x - p3.x) + (p3.x - p2.x)*(p1.y - p3.y));

    float gamma = 1.0f - alpha - beta;

    if ((alpha > 0) && (beta > 0) && (gamma > 0)) collision = true;

    return collision;
}

// Check collision between two rectangles
bool CheckCollisionRecs(Rectangle rec1, Rectangle rec2)
{
    bool collision = false;

    if ((rec1.x < (rec2.x + rec2.width) && (rec1.x + rec1.width) > rec2.x) &&
        (rec1.y < (rec2.y + rec2.height) && (rec1.y + rec1.height) > rec2.y)) collision = true;

    return collision;
}

// Check collision between two circles
bool CheckCollisionCircles(Vector2 center1, float radius1, Vector2 center2, float radius2)
{
    bool collision = false;

    float dx = center2.x - center1.x;      // X distance between centers
    float dy = center2.y - center1.y;      // Y distance between centers

    float distance = sqrtf(dx*dx + dy*dy); // Distance between centers

    if (distance <= (radius1 + radius2)) collision = true;

    return collision;
}

// Check collision between circle and rectangle
// NOTE: Reviewed version to take into account corner limit case
bool CheckCollisionCircleRec(Vector2 center, float radius, Rectangle rec)
{
    bool collision = false;

    int recCenterX = (int)(rec.x + rec.width/2.0f);
    int recCenterY = (int)(rec.y + rec.height/2.0f);

    float dx = fabsf(center.x - (float)recCenterX);
    float dy = fabsf(center.y - (float)recCenterY);

    if (dx > (rec.width/2.0f + radius)) { return false; }
    if (dy > (rec.height/2.0f + radius)) { return false; }

    if (dx <= (rec.width/2.0f)) { return true; }
    if (dy <= (rec.height/2.0f)) { return true; }

    float cornerDistanceSq = (dx - rec.width/2.0f)*(dx - rec.width/2.0f) +
                             (dy - rec.height/2.0f)*(dy - rec.height/2.0f);

    collision = (cornerDistanceSq <= (radius*radius));

    return collision;
}

// Check the collision between two lines defined by two points each, returns collision point by reference
bool CheckCollisionLines(Vector2 startPos1, Vector2 endPos1, Vector2 startPos2, Vector2 endPos2, Vector2 *collisionPoint)
{
    bool collision = false;

    float div = (endPos2.y - startPos2.y)*(endPos1.x - startPos1.x) - (endPos2.x - startPos2.x)*(endPos1.y - startPos1.y);

    if (fabsf(div) >= FLT_EPSILON)
    {
        collision = true;

        float xi = ((startPos2.x - endPos2.x)*(startPos1.x*endPos1.y - startPos1.y*endPos1.x) - (startPos1.x - endPos1.x)*(startPos2.x*endPos2.y - startPos2.y*endPos2.x))/div;
        float yi = ((startPos2.y - endPos2.y)*(startPos1.x*endPos1.y - startPos1.y*endPos1.x) - (startPos1.y - endPos1.y)*(startPos2.x*endPos2.y - startPos2.y*endPos2.x))/div;

        if (((fabsf(startPos1.x - endPos1.x) > FLT_EPSILON) && (xi < fminf(startPos1.x, endPos1.x) || (xi > fmaxf(startPos1.x, endPos1.x)))) ||
            ((fabsf(startPos2.x - endPos2.x) > FLT_EPSILON) && (xi < fminf(startPos2.x, endPos2.x) || (xi > fmaxf(startPos2.x, endPos2.x)))) ||
            ((fabsf(startPos1.y - endPos1.y) > FLT_EPSILON) && (yi < fminf(startPos1.y, endPos1.y) || (yi > fmaxf(startPos1.y, endPos1.y)))) ||
            ((fabsf(startPos2.y - endPos2.y) > FLT_EPSILON) && (yi < fminf(startPos2.y, endPos2.y) || (yi > fmaxf(startPos2.y, endPos2.y))))) collision = false;

        if (collision && (collisionPoint != 0))
        {
            collisionPoint->x = xi;
            collisionPoint->y = yi;
        }
    }

    return collision;
}

// Check if point belongs to line created between two points [p1] and [p2] with defined margin in pixels [threshold]
bool CheckCollisionPointLine(Vector2 point, Vector2 p1, Vector2 p2, int threshold)
{
    bool collision = false;

    float dxc = point.x - p1.x;
    float dyc = point.y - p1.y;
    float dxl = p2.x - p1.x;
    float dyl = p2.y - p1.y;
    float cross = dxc*dyl - dyc*dxl;

    if (fabsf(cross) < (threshold*fmaxf(fabsf(dxl), fabsf(dyl))))
    {
        if (fabsf(dxl) >= fabsf(dyl)) collision = (dxl > 0)? ((p1.x <= point.x) && (point.x <= p2.x)) : ((p2.x <= point.x) && (point.x <= p1.x));
        else collision = (dyl > 0)? ((p1.y <= point.y) && (point.y <= p2.y)) : ((p2.y <= point.y) && (point.y <= p1.y));
    }

    return collision;
}

// Get collision rectangle for two rectangles collision
Rectangle GetCollisionRec(Rectangle rec1, Rectangle rec2)
{
    Rectangle rec = { 0, 0, 0, 0 };

    if (CheckCollisionRecs(rec1, rec2))
    {
        float dxx = fabsf(rec1.x - rec2.x);
        float dyy = fabsf(rec1.y - rec2.y);

        if (rec1.x <= rec2.x)
        {
            if (rec1.y <= rec2.y)
            {
                rec.x = rec2.x;
                rec.y = rec2.y;
                rec.width = rec1.width - dxx;
                rec.height = rec1.height - dyy;
            }
            else
            {
                rec.x = rec2.x;
                rec.y = rec1.y;
                rec.width = rec1.width - dxx;
                rec.height = rec2.height - dyy;
            }
        }
        else
        {
            if (rec1.y <= rec2.y)
            {
                rec.x = rec1.x;
                rec.y = rec2.y;
                rec.width = rec2.width - dxx;
                rec.height = rec1.height - dyy;
            }
            else
            {
                rec.x = rec1.x;
                rec.y = rec1.y;
                rec.width = rec2.width - dxx;
                rec.height = rec2.height - dyy;
            }
        }

        if (rec1.width > rec2.width)
        {
            if (rec.width >= rec2.width) rec.width = rec2.width;
        }
        else
        {
            if (rec.width >= rec1.width) rec.width = rec1.width;
        }

        if (rec1.height > rec2.height)
        {
            if (rec.height >= rec2.height) rec.height = rec2.height;
        }
        else
        {
           if (rec.height >= rec1.height) rec.height = rec1.height;
        }
    }

    return rec;
}

//----------------------------------------------------------------------------------
// Module specific Functions Definition
//----------------------------------------------------------------------------------

// Cubic easing in-out
// NOTE: Used by DrawLineBezier() only
static float EaseCubicInOut(float t, float b, float c, float d)
{
    if ((t /= 0.5f*d) < 1) return 0.5f*c*t*t*t + b;

    t -= 2;

    return 0.5f*c*(t*t*t + 2.0f) + b;
}


//*****************************************************************************************************************************************************************************************

#include <stdlib.h>             // Required for: malloc(), free()
#include <string.h>             // Required for: strlen() [Used in ImageTextEx()], strcmp() [Used in LoadImageFromMemory()]
#include <math.h>               // Required for: fabsf()
#include <stdio.h>              // Required for: sprintf() [Used in ExportImageAsCode()]

// Support only desired texture formats on stb_image
#if !defined(SUPPORT_FILEFORMAT_BMP)
    #define STBI_NO_BMP
#endif
#if !defined(SUPPORT_FILEFORMAT_PNG)
    #define STBI_NO_PNG
#endif
#if !defined(SUPPORT_FILEFORMAT_TGA)
    #define STBI_NO_TGA
#endif
#if !defined(SUPPORT_FILEFORMAT_JPG)
    #define STBI_NO_JPEG        // Image format .jpg and .jpeg
#endif
#if !defined(SUPPORT_FILEFORMAT_PSD)
    #define STBI_NO_PSD
#endif
#if !defined(SUPPORT_FILEFORMAT_GIF)
    #define STBI_NO_GIF
#endif
#if !defined(SUPPORT_FILEFORMAT_PIC)
    #define STBI_NO_PIC
#endif
#if !defined(SUPPORT_FILEFORMAT_HDR)
    #define STBI_NO_HDR
#endif

// Image fileformats not supported by default
#define STBI_NO_PIC
#define STBI_NO_PNM             // Image format .ppm and .pgm

#if defined(__TINYC__)
    #define STBI_NO_SIMD
#endif

#if (defined(SUPPORT_FILEFORMAT_BMP) || \
     defined(SUPPORT_FILEFORMAT_PNG) || \
     defined(SUPPORT_FILEFORMAT_TGA) || \
     defined(SUPPORT_FILEFORMAT_JPG) || \
     defined(SUPPORT_FILEFORMAT_PSD) || \
     defined(SUPPORT_FILEFORMAT_GIF) || \
     defined(SUPPORT_FILEFORMAT_PIC) || \
     defined(SUPPORT_FILEFORMAT_HDR))

    #define STBI_MALLOC RL_MALLOC
    #define STBI_FREE RL_FREE
    #define STBI_REALLOC RL_REALLOC

    #define STB_IMAGE_IMPLEMENTATION
    #include "external/stb_image.h"         // Required for: stbi_load_from_file()
                                            // NOTE: Used to read image data (multiple formats support)
#endif

#if defined(SUPPORT_FILEFORMAT_QOI)
    #define QOI_MALLOC RL_MALLOC
    #define QOI_FREE RL_FREE

    #define QOI_IMPLEMENTATION
    #include "external/qoi.h"
#endif

#if defined(SUPPORT_IMAGE_EXPORT)
    #define STBIW_MALLOC RL_MALLOC
    #define STBIW_FREE RL_FREE
    #define STBIW_REALLOC RL_REALLOC

    #define STB_IMAGE_WRITE_IMPLEMENTATION
    #include "external/stb_image_write.h"   // Required for: stbi_write_*()
#endif

#if defined(SUPPORT_IMAGE_MANIPULATION)
    #define STBIR_MALLOC(size,c) ((void)(c), RL_MALLOC(size))
    #define STBIR_FREE(ptr,c) ((void)(c), RL_FREE(ptr))

    #define STB_IMAGE_RESIZE_IMPLEMENTATION
    #include "external/stb_image_resize.h"  // Required for: stbir_resize_uint8() [ImageResize()]
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD
    #define PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD  50    // Threshold over 255 to set alpha as 0
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
// It's lonely here...

//----------------------------------------------------------------------------------
// Other Modules Functions Declaration (required by text)
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Module specific Functions Declaration
//----------------------------------------------------------------------------------
#if defined(SUPPORT_FILEFORMAT_DDS)
static Image LoadDDS(const unsigned char *fileData, unsigned int fileSize);   // Load DDS file data
#endif
#if defined(SUPPORT_FILEFORMAT_PKM)
static Image LoadPKM(const unsigned char *fileData, unsigned int fileSize);   // Load PKM file data
#endif
#if defined(SUPPORT_FILEFORMAT_KTX)
static Image LoadKTX(const unsigned char *fileData, unsigned int fileSize);   // Load KTX file data
static int SaveKTX(Image image, const char *fileName);  // Save image data as KTX file
#endif
#if defined(SUPPORT_FILEFORMAT_PVR)
static Image LoadPVR(const unsigned char *fileData, unsigned int fileSize);   // Load PVR file data
#endif
#if defined(SUPPORT_FILEFORMAT_ASTC)
static Image LoadASTC(const unsigned char *fileData, unsigned int fileSize);  // Load ASTC file data
#endif

static Vector4 *LoadImageDataNormalized(Image image);       // Load pixel data from image as Vector4 array (float normalized)

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Load image from file into CPU memory (RAM)
Image LoadImage(const char *fileName)
{
    Image image = { 0 };

#if defined(SUPPORT_FILEFORMAT_PNG) || \
    defined(SUPPORT_FILEFORMAT_BMP) || \
    defined(SUPPORT_FILEFORMAT_TGA) || \
    defined(SUPPORT_FILEFORMAT_JPG) || \
    defined(SUPPORT_FILEFORMAT_GIF) || \
    defined(SUPPORT_FILEFORMAT_PIC) || \
    defined(SUPPORT_FILEFORMAT_HDR) || \
    defined(SUPPORT_FILEFORMAT_PSD)

    #define STBI_REQUIRED
#endif

    // Loading file to memory
    unsigned int fileSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &fileSize);

    // Loading image from memory data
    if (fileData != NULL) image = LoadImageFromMemory(GetFileExtension(fileName), fileData, fileSize);

    RL_FREE(fileData);

    return image;
}

// Load an image from RAW file data
Image LoadImageRaw(const char *fileName, int width, int height, int format, int headerSize)
{
    Image image = { 0 };

    unsigned int dataSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &dataSize);

    if (fileData != NULL)
    {
        unsigned char *dataPtr = fileData;
        unsigned int size = GetPixelDataSize(width, height, format);

        if (headerSize > 0) dataPtr += headerSize;

        image.data = RL_MALLOC(size);      // Allocate required memory in bytes
        memcpy(image.data, dataPtr, size); // Copy required data to image
        image.width = width;
        image.height = height;
        image.mipmaps = 1;
        image.format = format;

        RL_FREE(fileData);
    }

    return image;
}

// Load animated image data
//  - Image.data buffer includes all frames: [image#0][image#1][image#2][...]
//  - Number of frames is returned through 'frames' parameter
//  - All frames are returned in RGBA format
//  - Frames delay data is discarded
Image LoadImageAnim(const char *fileName, int *frames)
{
    Image image = { 0 };
    int frameCount = 1;

#if defined(SUPPORT_FILEFORMAT_GIF)
    if (IsFileExtension(fileName, ".gif"))
    {
        unsigned int dataSize = 0;
        unsigned char *fileData = LoadFileData(fileName, &dataSize);

        if (fileData != NULL)
        {
            int comp = 0;
            int *delays = NULL;
            image.data = stbi_load_gif_from_memory(fileData, dataSize, &delays, &image.width, &image.height, &frameCount, &comp, 4);

            image.mipmaps = 1;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

            RL_FREE(fileData);
            RL_FREE(delays);        // NOTE: Frames delays are discarded
        }
    }
#else
    if (false) { }
#endif
    else image = LoadImage(fileName);

    // TODO: Support APNG animated images

    *frames = frameCount;
    return image;
}

// Load image from memory buffer, fileType refers to extension: i.e. ".png"
// WARNING: File extension must be provided in lower-case
Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize)
{
    Image image = { 0 };

    if ((false)
#if defined(SUPPORT_FILEFORMAT_PNG)
        || (strcmp(fileType, ".png") == 0)
#endif
#if defined(SUPPORT_FILEFORMAT_BMP)
        || (strcmp(fileType, ".bmp") == 0)
#endif
#if defined(SUPPORT_FILEFORMAT_TGA)
        || (strcmp(fileType, ".tga") == 0)
#endif
#if defined(SUPPORT_FILEFORMAT_JPG)
        || ((strcmp(fileType, ".jpg") == 0) || (strcmp(fileType, ".jpeg") == 0))
#endif
#if defined(SUPPORT_FILEFORMAT_GIF)
        || (strcmp(fileType, ".gif") == 0)
#endif
#if defined(SUPPORT_FILEFORMAT_PIC)
        || (strcmp(fileType, ".pic") == 0)
#endif
#if defined(SUPPORT_FILEFORMAT_PSD)
        || (strcmp(fileType, ".psd") == 0)
#endif
       )
    {
#if defined(STBI_REQUIRED)
        // NOTE: Using stb_image to load images (Supports multiple image formats)

        if (fileData != NULL)
        {
            int comp = 0;
            image.data = stbi_load_from_memory(fileData, dataSize, &image.width, &image.height, &comp, 0);

            if (image.data != NULL)
            {
                image.mipmaps = 1;

                if (comp == 1) image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
                else if (comp == 2) image.format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
                else if (comp == 3) image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
                else if (comp == 4) image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            }
        }
#endif
    }
#if defined(SUPPORT_FILEFORMAT_HDR)
    else if (strcmp(fileType, ".hdr") == 0)
    {
#if defined(STBI_REQUIRED)
        if (fileData != NULL)
        {
            int comp = 0;
            image.data = stbi_loadf_from_memory(fileData, dataSize, &image.width, &image.height, &comp, 0);

            image.mipmaps = 1;

            if (comp == 1) image.format = PIXELFORMAT_UNCOMPRESSED_R32;
            else if (comp == 3) image.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32;
            else if (comp == 4) image.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
            else
            {
                TRACELOG(LOG_WARNING, "IMAGE: HDR file format not supported");
                UnloadImage(image);
            }
        }
#endif
    }
#endif
#if defined(SUPPORT_FILEFORMAT_QOI)
    else if (strcmp(fileType, ".qoi") == 0)
    {
        qoi_desc desc = { 0 };
        image.data = qoi_decode(fileData, dataSize, &desc, 4);
        image.width = desc.width;
        image.height = desc.height;
        image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        image.mipmaps = 1;
    }
#endif
#if defined(SUPPORT_FILEFORMAT_DDS)
    else if (strcmp(fileType, ".dds") == 0) image = LoadDDS(fileData, dataSize);
#endif
#if defined(SUPPORT_FILEFORMAT_PKM)
    else if (strcmp(fileType, ".pkm") == 0) image = LoadPKM(fileData, dataSize);
#endif
#if defined(SUPPORT_FILEFORMAT_KTX)
    else if (strcmp(fileType, ".ktx") == 0) image = LoadKTX(fileData, dataSize);
#endif
#if defined(SUPPORT_FILEFORMAT_PVR)
    else if (strcmp(fileType, ".pvr") == 0) image = LoadPVR(fileData, dataSize);
#endif
#if defined(SUPPORT_FILEFORMAT_ASTC)
    else if (strcmp(fileType, ".astc") == 0) image = LoadASTC(fileData, dataSize);
#endif
    else TRACELOG(LOG_WARNING, "IMAGE: Data format not supported");

    if (image.data != NULL) TRACELOG(LOG_INFO, "IMAGE: Data loaded successfully (%ix%i | %s | %i mipmaps)", image.width, image.height, rlGetPixelFormatName(image.format), image.mipmaps);
    else TRACELOG(LOG_WARNING, "IMAGE: Failed to load image data");

    return image;
}

// Load image from GPU texture data
// NOTE: Compressed texture formats not supported
Image LoadImageFromTexture(Texture2D texture)
{
    Image image = { 0 };

    if (texture.format < PIXELFORMAT_COMPRESSED_DXT1_RGB)
    {
        image.data = rlReadTexturePixels(texture.id, texture.width, texture.height, texture.format);

        if (image.data != NULL)
        {
            image.width = texture.width;
            image.height = texture.height;
            image.format = texture.format;
            image.mipmaps = 1;

#if defined(GRAPHICS_API_OPENGL_ES2)
            // NOTE: Data retrieved on OpenGL ES 2.0 should be RGBA,
            // coming from FBO color buffer attachment, but it seems
            // original texture format is retrieved on RPI...
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
#endif
            TRACELOG(LOG_INFO, "TEXTURE: [ID %i] Pixel data retrieved successfully", texture.id);
        }
        else TRACELOG(LOG_WARNING, "TEXTURE: [ID %i] Failed to retrieve pixel data", texture.id);
    }
    else TRACELOG(LOG_WARNING, "TEXTURE: [ID %i] Failed to retrieve compressed pixel data", texture.id);

    return image;
}

// Load image from screen buffer and (screenshot)
Image LoadImageFromScreen(void)
{
    Image image = { 0 };

    image.width = GetScreenWidth();
    image.height = GetScreenHeight();
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    image.data = rlReadScreenPixels(image.width, image.height);

    return image;
}

// Unload image from CPU memory (RAM)
void UnloadImage(Image image)
{
    RL_FREE(image.data);
}

// Export image data to file
// NOTE: File format depends on fileName extension
bool ExportImage(Image image, const char *fileName)
{
    int success = 0;

#if defined(SUPPORT_IMAGE_EXPORT)
    int channels = 4;
    bool allocatedData = false;
    unsigned char *imgData = (unsigned char *)image.data;

    if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) channels = 1;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) channels = 2;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) channels = 3;
    else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) channels = 4;
    else
    {
        // NOTE: Getting Color array as RGBA unsigned char values
        imgData = (unsigned char *)LoadImageColors(image);
        allocatedData = true;
    }

#if defined(SUPPORT_FILEFORMAT_PNG)
    if (IsFileExtension(fileName, ".png"))
    {
        int dataSize = 0;
        unsigned char *fileData = stbi_write_png_to_mem((const unsigned char *)imgData, image.width*channels, image.width, image.height, channels, &dataSize);
        success = SaveFileData(fileName, fileData, dataSize);
        RL_FREE(fileData);
    }
#else
    if (false) { }
#endif
#if defined(SUPPORT_FILEFORMAT_BMP)
    else if (IsFileExtension(fileName, ".bmp")) success = stbi_write_bmp(fileName, image.width, image.height, channels, imgData);
#endif
#if defined(SUPPORT_FILEFORMAT_TGA)
    else if (IsFileExtension(fileName, ".tga")) success = stbi_write_tga(fileName, image.width, image.height, channels, imgData);
#endif
#if defined(SUPPORT_FILEFORMAT_JPG)
    else if (IsFileExtension(fileName, ".jpg") ||
             IsFileExtension(fileName, ".jpeg")) success = stbi_write_jpg(fileName, image.width, image.height, channels, imgData, 90);  // JPG quality: between 1 and 100
#endif
#if defined(SUPPORT_FILEFORMAT_QOI)
    else if (IsFileExtension(fileName, ".qoi"))
    {
        channels = 0;
        if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) channels = 3;
        else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) channels = 4;
        else TRACELOG(LOG_WARNING, "IMAGE: Image pixel format must be R8G8B8 or R8G8B8A8");

        if ((channels == 3) || (channels == 4))
        {
            qoi_desc desc = { 0 };
            desc.width = image.width;
            desc.height = image.height;
            desc.channels = channels;
            desc.colorspace = QOI_SRGB;

            success = qoi_write(fileName, imgData, &desc);
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_KTX)
    else if (IsFileExtension(fileName, ".ktx")) success = SaveKTX(image, fileName);
#endif
    else if (IsFileExtension(fileName, ".raw"))
    {
        // Export raw pixel data (without header)
        // NOTE: It's up to the user to track image parameters
        success = SaveFileData(fileName, image.data, GetPixelDataSize(image.width, image.height, image.format));
    }

    if (allocatedData) RL_FREE(imgData);
#endif      // SUPPORT_IMAGE_EXPORT

    if (success != 0) TRACELOG(LOG_INFO, "FILEIO: [%s] Image exported successfully", fileName);
    else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to export image", fileName);

    return success;
}

// Export image as code file (.h) defining an array of bytes
bool ExportImageAsCode(Image image, const char *fileName)
{
    bool success = false;

#if defined(SUPPORT_IMAGE_EXPORT)

#ifndef TEXT_BYTES_PER_LINE
    #define TEXT_BYTES_PER_LINE     20
#endif

    int dataSize = GetPixelDataSize(image.width, image.height, image.format);

    // NOTE: Text data buffer size is estimated considering image data size in bytes
    // and requiring 6 char bytes for every byte: "0x00, "
    char *txtData = (char *)RL_CALLOC(dataSize*6 + 2000, sizeof(char));

    int byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// ImageAsCode exporter v1.0 - Image pixel data exported as an array of bytes         //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// more info and bugs-report:  github.com/raysan5/raylib                              //\n");
    byteCount += sprintf(txtData + byteCount, "// feedback and support:       ray[at]raylib.com                                      //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "// Copyright (c) 2018-2022 Ramon Santamaria (@raysan5)                                //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                                    //\n");
    byteCount += sprintf(txtData + byteCount, "////////////////////////////////////////////////////////////////////////////////////////\n\n");

    // Get file name from path and convert variable name to uppercase
    char varFileName[256] = { 0 };
    strcpy(varFileName, GetFileNameWithoutExt(fileName));
    for (int i = 0; varFileName[i] != '\0'; i++) if ((varFileName[i] >= 'a') && (varFileName[i] <= 'z')) { varFileName[i] = varFileName[i] - 32; }

    // Add image information
    byteCount += sprintf(txtData + byteCount, "// Image data information\n");
    byteCount += sprintf(txtData + byteCount, "#define %s_WIDTH    %i\n", varFileName, image.width);
    byteCount += sprintf(txtData + byteCount, "#define %s_HEIGHT   %i\n", varFileName, image.height);
    byteCount += sprintf(txtData + byteCount, "#define %s_FORMAT   %i          // raylib internal pixel format\n\n", varFileName, image.format);

    byteCount += sprintf(txtData + byteCount, "static unsigned char %s_DATA[%i] = { ", varFileName, dataSize);
    for (int i = 0; i < dataSize - 1; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%x,\n" : "0x%x, "), ((unsigned char *)image.data)[i]);
    byteCount += sprintf(txtData + byteCount, "0x%x };\n", ((unsigned char *)image.data)[dataSize - 1]);

    // NOTE: Text data size exported is determined by '\0' (NULL) character
    success = SaveFileText(fileName, txtData);

    RL_FREE(txtData);

#endif      // SUPPORT_IMAGE_EXPORT

    if (success != 0) TRACELOG(LOG_INFO, "FILEIO: [%s] Image as code exported successfully", fileName);
    else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to export image as code", fileName);

    return success;
}

//------------------------------------------------------------------------------------
// Image generation functions
//------------------------------------------------------------------------------------
// Generate image: plain color
Image GenImageColor(int width, int height, Color color)
{
    Color *pixels = (Color *)RL_CALLOC(width*height, sizeof(Color));

    for (int i = 0; i < width*height; i++) pixels[i] = color;

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };

    return image;
}

#if defined(SUPPORT_IMAGE_GENERATION)
// Generate image: vertical gradient
Image GenImageGradientV(int width, int height, Color top, Color bottom)
{
    Color *pixels = (Color *)RL_MALLOC(width*height*sizeof(Color));

    for (int j = 0; j < height; j++)
    {
        float factor = (float)j/(float)height;
        for (int i = 0; i < width; i++)
        {
            pixels[j*width + i].r = (int)((float)bottom.r*factor + (float)top.r*(1.f - factor));
            pixels[j*width + i].g = (int)((float)bottom.g*factor + (float)top.g*(1.f - factor));
            pixels[j*width + i].b = (int)((float)bottom.b*factor + (float)top.b*(1.f - factor));
            pixels[j*width + i].a = (int)((float)bottom.a*factor + (float)top.a*(1.f - factor));
        }
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };

    return image;
}

// Generate image: horizontal gradient
Image GenImageGradientH(int width, int height, Color left, Color right)
{
    Color *pixels = (Color *)RL_MALLOC(width*height*sizeof(Color));

    for (int i = 0; i < width; i++)
    {
        float factor = (float)i/(float)width;
        for (int j = 0; j < height; j++)
        {
            pixels[j*width + i].r = (int)((float)right.r*factor + (float)left.r*(1.f - factor));
            pixels[j*width + i].g = (int)((float)right.g*factor + (float)left.g*(1.f - factor));
            pixels[j*width + i].b = (int)((float)right.b*factor + (float)left.b*(1.f - factor));
            pixels[j*width + i].a = (int)((float)right.a*factor + (float)left.a*(1.f - factor));
        }
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };

    return image;
}

// Generate image: radial gradient
Image GenImageGradientRadial(int width, int height, float density, Color inner, Color outer)
{
    Color *pixels = (Color *)RL_MALLOC(width*height*sizeof(Color));
    float radius = (width < height)? (float)width/2.0f : (float)height/2.0f;

    float centerX = (float)width/2.0f;
    float centerY = (float)height/2.0f;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float dist = hypotf((float)x - centerX, (float)y - centerY);
            float factor = (dist - radius*density)/(radius*(1.0f - density));

            factor = (float)fmax(factor, 0.0f);
            factor = (float)fmin(factor, 1.f); // dist can be bigger than radius so we have to check

            pixels[y*width + x].r = (int)((float)outer.r*factor + (float)inner.r*(1.0f - factor));
            pixels[y*width + x].g = (int)((float)outer.g*factor + (float)inner.g*(1.0f - factor));
            pixels[y*width + x].b = (int)((float)outer.b*factor + (float)inner.b*(1.0f - factor));
            pixels[y*width + x].a = (int)((float)outer.a*factor + (float)inner.a*(1.0f - factor));
        }
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };

    return image;
}

// Generate image: checked
Image GenImageChecked(int width, int height, int checksX, int checksY, Color col1, Color col2)
{
    Color *pixels = (Color *)RL_MALLOC(width*height*sizeof(Color));

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if ((x/checksX + y/checksY)%2 == 0) pixels[y*width + x] = col1;
            else pixels[y*width + x] = col2;
        }
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };

    return image;
}

// Generate image: white noise
Image GenImageWhiteNoise(int width, int height, float factor)
{
    Color *pixels = (Color *)RL_MALLOC(width*height*sizeof(Color));

    for (int i = 0; i < width*height; i++)
    {
        if (GetRandomValue(0, 99) < (int)(factor*100.0f)) pixels[i] = WHITE;
        else pixels[i] = BLACK;
    }

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };

    return image;
}

// Generate image: cellular algorithm. Bigger tileSize means bigger cells
Image GenImageCellular(int width, int height, int tileSize)
{
    Color *pixels = (Color *)RL_MALLOC(width*height*sizeof(Color));

    int seedsPerRow = width/tileSize;
    int seedsPerCol = height/tileSize;
    int seedCount = seedsPerRow*seedsPerCol;

    Vector2 *seeds = (Vector2 *)RL_MALLOC(seedCount*sizeof(Vector2));

    for (int i = 0; i < seedCount; i++)
    {
        int y = (i/seedsPerRow)*tileSize + GetRandomValue(0, tileSize - 1);
        int x = (i%seedsPerRow)*tileSize + GetRandomValue(0, tileSize - 1);
        seeds[i] = (Vector2){ (float)x, (float)y };
    }

    for (int y = 0; y < height; y++)
    {
        int tileY = y/tileSize;

        for (int x = 0; x < width; x++)
        {
            int tileX = x/tileSize;

            float minDistance = 65536.0f; //(float)strtod("Inf", NULL);

            // Check all adjacent tiles
            for (int i = -1; i < 2; i++)
            {
                if ((tileX + i < 0) || (tileX + i >= seedsPerRow)) continue;

                for (int j = -1; j < 2; j++)
                {
                    if ((tileY + j < 0) || (tileY + j >= seedsPerCol)) continue;

                    Vector2 neighborSeed = seeds[(tileY + j)*seedsPerRow + tileX + i];

                    float dist = (float)hypot(x - (int)neighborSeed.x, y - (int)neighborSeed.y);
                    minDistance = (float)fmin(minDistance, dist);
                }
            }

            // I made this up but it seems to give good results at all tile sizes
            int intensity = (int)(minDistance*256.0f/tileSize);
            if (intensity > 255) intensity = 255;

            pixels[y*width + x] = (Color){ intensity, intensity, intensity, 255 };
        }
    }

    RL_FREE(seeds);

    Image image = {
        .data = pixels,
        .width = width,
        .height = height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };

    return image;
}
#endif      // SUPPORT_IMAGE_GENERATION

//------------------------------------------------------------------------------------
// Image manipulation functions
//------------------------------------------------------------------------------------
// Copy an image to a new image
Image ImageCopy(Image image)
{
    Image newImage = { 0 };

    int width = image.width;
    int height = image.height;
    int size = 0;

    for (int i = 0; i < image.mipmaps; i++)
    {
        size += GetPixelDataSize(width, height, image.format);

        width /= 2;
        height /= 2;

        // Security check for NPOT textures
        if (width < 1) width = 1;
        if (height < 1) height = 1;
    }

    newImage.data = RL_MALLOC(size);

    if (newImage.data != NULL)
    {
        // NOTE: Size must be provided in bytes
        memcpy(newImage.data, image.data, size);

        newImage.width = image.width;
        newImage.height = image.height;
        newImage.mipmaps = image.mipmaps;
        newImage.format = image.format;
    }

    return newImage;
}

// Create an image from another image piece
Image ImageFromImage(Image image, Rectangle rec)
{
    Image result = { 0 };

    int bytesPerPixel = GetPixelDataSize(1, 1, image.format);

    result.width = (int)rec.width;
    result.height = (int)rec.height;
    result.data = RL_CALLOC((int)(rec.width*rec.height)*bytesPerPixel, 1);
    result.format = image.format;
    result.mipmaps = 1;

    for (int y = 0; y < rec.height; y++)
    {
        memcpy(((unsigned char *)result.data) + y*(int)rec.width*bytesPerPixel, ((unsigned char *)image.data) + ((y + (int)rec.y)*image.width + (int)rec.x)*bytesPerPixel, (int)rec.width*bytesPerPixel);
    }

    return result;
}

// Crop an image to area defined by a rectangle
// NOTE: Security checks are performed in case rectangle goes out of bounds
void ImageCrop(Image *image, Rectangle crop)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    // Security checks to validate crop rectangle
    if (crop.x < 0) { crop.width += crop.x; crop.x = 0; }
    if (crop.y < 0) { crop.height += crop.y; crop.y = 0; }
    if ((crop.x + crop.width) > image->width) crop.width = image->width - crop.x;
    if ((crop.y + crop.height) > image->height) crop.height = image->height - crop.y;
    if ((crop.x > image->width) || (crop.y > image->height))
    {
        TRACELOG(LOG_WARNING, "IMAGE: Failed to crop, rectangle out of bounds");
        return;
    }

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);

        unsigned char *croppedData = (unsigned char *)RL_MALLOC((int)(crop.width*crop.height)*bytesPerPixel);

        // OPTION 1: Move cropped data line-by-line
        for (int y = (int)crop.y, offsetSize = 0; y < (int)(crop.y + crop.height); y++)
        {
            memcpy(croppedData + offsetSize, ((unsigned char *)image->data) + (y*image->width + (int)crop.x)*bytesPerPixel, (int)crop.width*bytesPerPixel);
            offsetSize += ((int)crop.width*bytesPerPixel);
        }

        /*
        // OPTION 2: Move cropped data pixel-by-pixel or byte-by-byte
        for (int y = (int)crop.y; y < (int)(crop.y + crop.height); y++)
        {
            for (int x = (int)crop.x; x < (int)(crop.x + crop.width); x++)
            {
                //memcpy(croppedData + ((y - (int)crop.y)*(int)crop.width + (x - (int)crop.x))*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + x)*bytesPerPixel, bytesPerPixel);
                for (int i = 0; i < bytesPerPixel; i++) croppedData[((y - (int)crop.y)*(int)crop.width + (x - (int)crop.x))*bytesPerPixel + i] = ((unsigned char *)image->data)[(y*image->width + x)*bytesPerPixel + i];
            }
        }
        */

        RL_FREE(image->data);
        image->data = croppedData;
        image->width = (int)crop.width;
        image->height = (int)crop.height;
    }
}

// Convert image data to desired format
void ImageFormat(Image *image, int newFormat)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if ((newFormat != 0) && (image->format != newFormat))
    {
        if ((image->format < PIXELFORMAT_COMPRESSED_DXT1_RGB) && (newFormat < PIXELFORMAT_COMPRESSED_DXT1_RGB))
        {
            Vector4 *pixels = LoadImageDataNormalized(*image);     // Supports 8 to 32 bit per channel

            RL_FREE(image->data);      // WARNING! We loose mipmaps data --> Regenerated at the end...
            image->data = NULL;
            image->format = newFormat;

            int k = 0;

            switch (image->format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    image->data = (unsigned char *)RL_MALLOC(image->width*image->height*sizeof(unsigned char));

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        ((unsigned char *)image->data)[i] = (unsigned char)((pixels[i].x*0.299f + pixels[i].y*0.587f + pixels[i].z*0.114f)*255.0f);
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    image->data = (unsigned char *)RL_MALLOC(image->width*image->height*2*sizeof(unsigned char));

                    for (int i = 0; i < image->width*image->height*2; i += 2, k++)
                    {
                        ((unsigned char *)image->data)[i] = (unsigned char)((pixels[k].x*0.299f + (float)pixels[k].y*0.587f + (float)pixels[k].z*0.114f)*255.0f);
                        ((unsigned char *)image->data)[i + 1] = (unsigned char)(pixels[k].w*255.0f);
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    image->data = (unsigned short *)RL_MALLOC(image->width*image->height*sizeof(unsigned short));

                    unsigned char r = 0;
                    unsigned char g = 0;
                    unsigned char b = 0;

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        r = (unsigned char)(round(pixels[i].x*31.0f));
                        g = (unsigned char)(round(pixels[i].y*63.0f));
                        b = (unsigned char)(round(pixels[i].z*31.0f));

                        ((unsigned short *)image->data)[i] = (unsigned short)r << 11 | (unsigned short)g << 5 | (unsigned short)b;
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    image->data = (unsigned char *)RL_MALLOC(image->width*image->height*3*sizeof(unsigned char));

                    for (int i = 0, k = 0; i < image->width*image->height*3; i += 3, k++)
                    {
                        ((unsigned char *)image->data)[i] = (unsigned char)(pixels[k].x*255.0f);
                        ((unsigned char *)image->data)[i + 1] = (unsigned char)(pixels[k].y*255.0f);
                        ((unsigned char *)image->data)[i + 2] = (unsigned char)(pixels[k].z*255.0f);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    image->data = (unsigned short *)RL_MALLOC(image->width*image->height*sizeof(unsigned short));

                    unsigned char r = 0;
                    unsigned char g = 0;
                    unsigned char b = 0;
                    unsigned char a = 0;

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        r = (unsigned char)(round(pixels[i].x*31.0f));
                        g = (unsigned char)(round(pixels[i].y*31.0f));
                        b = (unsigned char)(round(pixels[i].z*31.0f));
                        a = (pixels[i].w > ((float)PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD/255.0f))? 1 : 0;

                        ((unsigned short *)image->data)[i] = (unsigned short)r << 11 | (unsigned short)g << 6 | (unsigned short)b << 1 | (unsigned short)a;
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    image->data = (unsigned short *)RL_MALLOC(image->width*image->height*sizeof(unsigned short));

                    unsigned char r = 0;
                    unsigned char g = 0;
                    unsigned char b = 0;
                    unsigned char a = 0;

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        r = (unsigned char)(round(pixels[i].x*15.0f));
                        g = (unsigned char)(round(pixels[i].y*15.0f));
                        b = (unsigned char)(round(pixels[i].z*15.0f));
                        a = (unsigned char)(round(pixels[i].w*15.0f));

                        ((unsigned short *)image->data)[i] = (unsigned short)r << 12 | (unsigned short)g << 8 | (unsigned short)b << 4 | (unsigned short)a;
                    }

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    image->data = (unsigned char *)RL_MALLOC(image->width*image->height*4*sizeof(unsigned char));

                    for (int i = 0, k = 0; i < image->width*image->height*4; i += 4, k++)
                    {
                        ((unsigned char *)image->data)[i] = (unsigned char)(pixels[k].x*255.0f);
                        ((unsigned char *)image->data)[i + 1] = (unsigned char)(pixels[k].y*255.0f);
                        ((unsigned char *)image->data)[i + 2] = (unsigned char)(pixels[k].z*255.0f);
                        ((unsigned char *)image->data)[i + 3] = (unsigned char)(pixels[k].w*255.0f);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    // WARNING: Image is converted to GRAYSCALE eqeuivalent 32bit

                    image->data = (float *)RL_MALLOC(image->width*image->height*sizeof(float));

                    for (int i = 0; i < image->width*image->height; i++)
                    {
                        ((float *)image->data)[i] = (float)(pixels[i].x*0.299f + pixels[i].y*0.587f + pixels[i].z*0.114f);
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    image->data = (float *)RL_MALLOC(image->width*image->height*3*sizeof(float));

                    for (int i = 0, k = 0; i < image->width*image->height*3; i += 3, k++)
                    {
                        ((float *)image->data)[i] = pixels[k].x;
                        ((float *)image->data)[i + 1] = pixels[k].y;
                        ((float *)image->data)[i + 2] = pixels[k].z;
                    }
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    image->data = (float *)RL_MALLOC(image->width*image->height*4*sizeof(float));

                    for (int i = 0, k = 0; i < image->width*image->height*4; i += 4, k++)
                    {
                        ((float *)image->data)[i] = pixels[k].x;
                        ((float *)image->data)[i + 1] = pixels[k].y;
                        ((float *)image->data)[i + 2] = pixels[k].z;
                        ((float *)image->data)[i + 3] = pixels[k].w;
                    }
                } break;
                default: break;
            }

            RL_FREE(pixels);
            pixels = NULL;

            // In case original image had mipmaps, generate mipmaps for formated image
            // NOTE: Original mipmaps are replaced by new ones, if custom mipmaps were used, they are lost
            if (image->mipmaps > 1)
            {
                image->mipmaps = 1;
            #if defined(SUPPORT_IMAGE_MANIPULATION)
                if (image->data != NULL) ImageMipmaps(image);
            #endif
            }
        }
        else TRACELOG(LOG_WARNING, "IMAGE: Data format is compressed, can not be converted");
    }
}

// Convert image to POT (power-of-two)
// NOTE: It could be useful on OpenGL ES 2.0 (RPI, HTML5)
void ImageToPOT(Image *image, Color fill)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    // Calculate next power-of-two values
    // NOTE: Just add the required amount of pixels at the right and bottom sides of image...
    int potWidth = (int)powf(2, ceilf(logf((float)image->width)/logf(2)));
    int potHeight = (int)powf(2, ceilf(logf((float)image->height)/logf(2)));

    // Check if POT texture generation is required (if texture is not already POT)
    if ((potWidth != image->width) || (potHeight != image->height)) ImageResizeCanvas(image, potWidth, potHeight, 0, 0, fill);
}

#if defined(SUPPORT_IMAGE_MANIPULATION)
// Create an image from text (default font)
Image ImageText(const char *text, int fontSize, Color color)
{
    Image imText = { 0 };
#if defined(SUPPORT_MODULE_RTEXT)
    int defaultFontSize = 10;   // Default Font chars height in pixel
    if (fontSize < defaultFontSize) fontSize = defaultFontSize;
    int spacing = fontSize/defaultFontSize;
    imText = ImageTextEx(GetFontDefault(), text, (float)fontSize, (float)spacing, color);   // WARNING: Module required: rtext
#else
    imText = GenImageColor(200, 60, BLACK);     // Generating placeholder black image rectangle
    TRACELOG(LOG_WARNING, "IMAGE: ImageTextEx() requires module: rtext");
#endif
    return imText;
}

// Create an image from text (custom sprite font)
Image ImageTextEx(Font font, const char *text, float fontSize, float spacing, Color tint)
{
    Image imText = { 0 };
#if defined(SUPPORT_MODULE_RTEXT)
    int size = (int)strlen(text);   // Get size in bytes of text

    int textOffsetX = 0;            // Image drawing position X
    int textOffsetY = 0;            // Offset between lines (on line break '\n')

    // NOTE: Text image is generated at font base size, later scaled to desired font size
    Vector2 imSize = MeasureTextEx(font, text, (float)font.baseSize, spacing);  // WARNING: Module required: rtext

    // Create image to store text
    imText = GenImageColor((int)imSize.x, (int)imSize.y, BLANK);

    for (int i = 0; i < size; i++)
    {
        // Get next codepoint from byte string and glyph index in font
        int codepointByteCount = 0;
        int codepoint = GetCodepoint(&text[i], &codepointByteCount);    // WARNING: Module required: rtext
        int index = GetGlyphIndex(font, codepoint);                     // WARNING: Module required: rtext

        // NOTE: Normally we exit the decoding sequence as soon as a bad byte is found (and return 0x3f)
        // but we need to draw all of the bad bytes using the '?' symbol moving one byte
        if (codepoint == 0x3f) codepointByteCount = 1;

        if (codepoint == '\n')
        {
            // NOTE: Fixed line spacing of 1.5 line-height
            // TODO: Support custom line spacing defined by user
            textOffsetY += (font.baseSize + font.baseSize/2);
            textOffsetX = 0;
        }
        else
        {
            if ((codepoint != ' ') && (codepoint != '\t'))
            {
                Rectangle rec = { (float)(textOffsetX + font.glyphs[index].offsetX), (float)(textOffsetY + font.glyphs[index].offsetY), (float)font.recs[index].width, (float)font.recs[index].height };
                ImageDraw(&imText, font.glyphs[index].image, (Rectangle){ 0, 0, (float)font.glyphs[index].image.width, (float)font.glyphs[index].image.height }, rec, tint);
            }

            if (font.glyphs[index].advanceX == 0) textOffsetX += (int)(font.recs[index].width + spacing);
            else textOffsetX += font.glyphs[index].advanceX + (int)spacing;
        }

        i += (codepointByteCount - 1);   // Move text bytes counter to next codepoint
    }

    // Scale image depending on text size
    if (fontSize > imSize.y)
    {
        float scaleFactor = fontSize/imSize.y;
        TRACELOG(LOG_INFO, "IMAGE: Text scaled by factor: %f", scaleFactor);

        // Using nearest-neighbor scaling algorithm for default font
        // WARNING: Module required: rtext
        if (font.texture.id == GetFontDefault().texture.id) ImageResizeNN(&imText, (int)(imSize.x*scaleFactor), (int)(imSize.y*scaleFactor));
        else ImageResize(&imText, (int)(imSize.x*scaleFactor), (int)(imSize.y*scaleFactor));
    }
#else
    imText = GenImageColor(200, 60, BLACK);     // Generating placeholder black image rectangle
    TRACELOG(LOG_WARNING, "IMAGE: ImageTextEx() requires module: rtext");
#endif
    return imText;
}

// Crop image depending on alpha value
// NOTE: Threshold is defined as a percentatge: 0.0f -> 1.0f
void ImageAlphaCrop(Image *image, float threshold)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    Rectangle crop = GetImageAlphaBorder(*image, threshold);

    // Crop if rectangle is valid
    if (((int)crop.width != 0) && ((int)crop.height != 0)) ImageCrop(image, crop);
}

// Clear alpha channel to desired color
// NOTE: Threshold defines the alpha limit, 0.0f to 1.0f
void ImageAlphaClear(Image *image, Color color, float threshold)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        switch (image->format)
        {
            case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
            {
                unsigned char thresholdValue = (unsigned char)(threshold*255.0f);
                for (int i = 1; i < image->width*image->height*2; i += 2)
                {
                    if (((unsigned char *)image->data)[i] <= thresholdValue)
                    {
                        ((unsigned char *)image->data)[i - 1] = color.r;
                        ((unsigned char *)image->data)[i] = color.a;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
            {
                unsigned char thresholdValue = ((threshold < 0.5f)? 0 : 1);

                unsigned char r = (unsigned char)(round((float)color.r*31.0f));
                unsigned char g = (unsigned char)(round((float)color.g*31.0f));
                unsigned char b = (unsigned char)(round((float)color.b*31.0f));
                unsigned char a = (color.a < 128)? 0 : 1;

                for (int i = 0; i < image->width*image->height; i++)
                {
                    if ((((unsigned short *)image->data)[i] & 0b0000000000000001) <= thresholdValue)
                    {
                        ((unsigned short *)image->data)[i] = (unsigned short)r << 11 | (unsigned short)g << 6 | (unsigned short)b << 1 | (unsigned short)a;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
            {
                unsigned char thresholdValue = (unsigned char)(threshold*15.0f);

                unsigned char r = (unsigned char)(round((float)color.r*15.0f));
                unsigned char g = (unsigned char)(round((float)color.g*15.0f));
                unsigned char b = (unsigned char)(round((float)color.b*15.0f));
                unsigned char a = (unsigned char)(round((float)color.a*15.0f));

                for (int i = 0; i < image->width*image->height; i++)
                {
                    if ((((unsigned short *)image->data)[i] & 0x000f) <= thresholdValue)
                    {
                        ((unsigned short *)image->data)[i] = (unsigned short)r << 12 | (unsigned short)g << 8 | (unsigned short)b << 4 | (unsigned short)a;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
            {
                unsigned char thresholdValue = (unsigned char)(threshold*255.0f);
                for (int i = 3; i < image->width*image->height*4; i += 4)
                {
                    if (((unsigned char *)image->data)[i] <= thresholdValue)
                    {
                        ((unsigned char *)image->data)[i - 3] = color.r;
                        ((unsigned char *)image->data)[i - 2] = color.g;
                        ((unsigned char *)image->data)[i - 1] = color.b;
                        ((unsigned char *)image->data)[i] = color.a;
                    }
                }
            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
            {
                for (int i = 3; i < image->width*image->height*4; i += 4)
                {
                    if (((float *)image->data)[i] <= threshold)
                    {
                        ((float *)image->data)[i - 3] = (float)color.r/255.0f;
                        ((float *)image->data)[i - 2] = (float)color.g/255.0f;
                        ((float *)image->data)[i - 1] = (float)color.b/255.0f;
                        ((float *)image->data)[i] = (float)color.a/255.0f;
                    }
                }
            } break;
            default: break;
        }
    }
}

// Apply alpha mask to image
// NOTE 1: Returned image is GRAY_ALPHA (16bit) or RGBA (32bit)
// NOTE 2: alphaMask should be same size as image
void ImageAlphaMask(Image *image, Image alphaMask)
{
    if ((image->width != alphaMask.width) || (image->height != alphaMask.height))
    {
        TRACELOG(LOG_WARNING, "IMAGE: Alpha mask must be same size as image");
    }
    else if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB)
    {
        TRACELOG(LOG_WARNING, "IMAGE: Alpha mask can not be applied to compressed data formats");
    }
    else
    {
        // Force mask to be Grayscale
        Image mask = ImageCopy(alphaMask);
        if (mask.format != PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) ImageFormat(&mask, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);

        // In case image is only grayscale, we just add alpha channel
        if (image->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
        {
            unsigned char *data = (unsigned char *)RL_MALLOC(image->width*image->height*2);

            // Apply alpha mask to alpha channel
            for (int i = 0, k = 0; (i < mask.width*mask.height) || (i < image->width*image->height); i++, k += 2)
            {
                data[k] = ((unsigned char *)image->data)[i];
                data[k + 1] = ((unsigned char *)mask.data)[i];
            }

            RL_FREE(image->data);
            image->data = data;
            image->format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
        }
        else
        {
            // Convert image to RGBA
            if (image->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

            // Apply alpha mask to alpha channel
            for (int i = 0, k = 3; (i < mask.width*mask.height) || (i < image->width*image->height); i++, k += 4)
            {
                ((unsigned char *)image->data)[k] = ((unsigned char *)mask.data)[i];
            }
        }

        UnloadImage(mask);
    }
}

// Premultiply alpha channel
void ImageAlphaPremultiply(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    float alpha = 0.0f;
    Color *pixels = LoadImageColors(*image);

    for (int i = 0; i < image->width*image->height; i++)
    {
        if (pixels[i].a == 0)
        {
            pixels[i].r = 0;
            pixels[i].g = 0;
            pixels[i].b = 0;
        }
        else if (pixels[i].a < 255)
        {
            alpha = (float)pixels[i].a/255.0f;
            pixels[i].r = (unsigned char)((float)pixels[i].r*alpha);
            pixels[i].g = (unsigned char)((float)pixels[i].g*alpha);
            pixels[i].b = (unsigned char)((float)pixels[i].b*alpha);
        }
    }

    RL_FREE(image->data);

    int format = image->format;
    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Resize and image to new size
// NOTE: Uses stb default scaling filters (both bicubic):
// STBIR_DEFAULT_FILTER_UPSAMPLE    STBIR_FILTER_CATMULLROM
// STBIR_DEFAULT_FILTER_DOWNSAMPLE  STBIR_FILTER_MITCHELL   (high-quality Catmull-Rom)
void ImageResize(Image *image, int newWidth, int newHeight)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    bool fastPath = true;
    if ((image->format != PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) && (image->format != PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) && (image->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8) && (image->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)) fastPath = true;

    if (fastPath)
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *output = (unsigned char *)RL_MALLOC(newWidth*newHeight*bytesPerPixel);

        switch (image->format)
        {
            case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: stbir_resize_uint8((unsigned char *)image->data, image->width, image->height, 0, output, newWidth, newHeight, 0, 1); break;
            case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: stbir_resize_uint8((unsigned char *)image->data, image->width, image->height, 0, output, newWidth, newHeight, 0, 2); break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8: stbir_resize_uint8((unsigned char *)image->data, image->width, image->height, 0, output, newWidth, newHeight, 0, 3); break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: stbir_resize_uint8((unsigned char *)image->data, image->width, image->height, 0, output, newWidth, newHeight, 0, 4); break;
            default: break;
        }

        RL_FREE(image->data);
        image->data = output;
        image->width = newWidth;
        image->height = newHeight;
    }
    else
    {
        // Get data as Color pixels array to work with it
        Color *pixels = LoadImageColors(*image);
        Color *output = (Color *)RL_MALLOC(newWidth*newHeight*sizeof(Color));

        // NOTE: Color data is casted to (unsigned char *), there shouldn't been any problem...
        stbir_resize_uint8((unsigned char *)pixels, image->width, image->height, 0, (unsigned char *)output, newWidth, newHeight, 0, 4);

        int format = image->format;

        UnloadImageColors(pixels);
        RL_FREE(image->data);

        image->data = output;
        image->width = newWidth;
        image->height = newHeight;
        image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

        ImageFormat(image, format);  // Reformat 32bit RGBA image to original format
    }
}

// Resize and image to new size using Nearest-Neighbor scaling algorithm
void ImageResizeNN(Image *image,int newWidth,int newHeight)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    Color *pixels = LoadImageColors(*image);
    Color *output = (Color *)RL_MALLOC(newWidth*newHeight*sizeof(Color));

    // EDIT: added +1 to account for an early rounding problem
    int xRatio = (int)((image->width << 16)/newWidth) + 1;
    int yRatio = (int)((image->height << 16)/newHeight) + 1;

    int x2, y2;
    for (int y = 0; y < newHeight; y++)
    {
        for (int x = 0; x < newWidth; x++)
        {
            x2 = ((x*xRatio) >> 16);
            y2 = ((y*yRatio) >> 16);

            output[(y*newWidth) + x] = pixels[(y2*image->width) + x2] ;
        }
    }

    int format = image->format;

    RL_FREE(image->data);

    image->data = output;
    image->width = newWidth;
    image->height = newHeight;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);  // Reformat 32bit RGBA image to original format

    UnloadImageColors(pixels);
}

// Resize canvas and fill with color
// NOTE: Resize offset is relative to the top-left corner of the original image
void ImageResizeCanvas(Image *image, int newWidth, int newHeight, int offsetX, int offsetY, Color fill)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else if ((newWidth != image->width) || (newHeight != image->height))
    {
        Rectangle srcRec = { 0, 0, (float)image->width, (float)image->height };
        Vector2 dstPos = { (float)offsetX, (float)offsetY };

        if (offsetX < 0)
        {
            srcRec.x = (float)-offsetX;
            srcRec.width += (float)offsetX;
            dstPos.x = 0;
        }
        else if ((offsetX + image->width) > newWidth) srcRec.width = (float)(newWidth - offsetX);

        if (offsetY < 0)
        {
            srcRec.y = (float)-offsetY;
            srcRec.height += (float)offsetY;
            dstPos.y = 0;
        }
        else if ((offsetY + image->height) > newHeight) srcRec.height = (float)(newHeight - offsetY);

        if (newWidth < srcRec.width) srcRec.width = (float)newWidth;
        if (newHeight < srcRec.height) srcRec.height = (float)newHeight;

        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *resizedData = (unsigned char *)RL_CALLOC(newWidth*newHeight*bytesPerPixel, 1);

        // TODO: Fill resized canvas with fill color (must be formatted to image->format)

        int dstOffsetSize = ((int)dstPos.y*newWidth + (int)dstPos.x)*bytesPerPixel;

        for (int y = 0; y < (int)srcRec.height; y++)
        {
            memcpy(resizedData + dstOffsetSize, ((unsigned char *)image->data) + ((y + (int)srcRec.y)*image->width + (int)srcRec.x)*bytesPerPixel, (int)srcRec.width*bytesPerPixel);
            dstOffsetSize += (newWidth*bytesPerPixel);
        }

        RL_FREE(image->data);
        image->data = resizedData;
        image->width = newWidth;
        image->height = newHeight;
    }
}

// Generate all mipmap levels for a provided image
// NOTE 1: Supports POT and NPOT images
// NOTE 2: image.data is scaled to include mipmap levels
// NOTE 3: Mipmaps format is the same as base image
void ImageMipmaps(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    int mipCount = 1;                   // Required mipmap levels count (including base level)
    int mipWidth = image->width;        // Base image width
    int mipHeight = image->height;      // Base image height
    int mipSize = GetPixelDataSize(mipWidth, mipHeight, image->format);  // Image data size (in bytes)

    // Count mipmap levels required
    while ((mipWidth != 1) || (mipHeight != 1))
    {
        if (mipWidth != 1) mipWidth /= 2;
        if (mipHeight != 1) mipHeight /= 2;

        // Security check for NPOT textures
        if (mipWidth < 1) mipWidth = 1;
        if (mipHeight < 1) mipHeight = 1;

        TRACELOGD("IMAGE: Next mipmap level: %i x %i - current size %i", mipWidth, mipHeight, mipSize);

        mipCount++;
        mipSize += GetPixelDataSize(mipWidth, mipHeight, image->format);       // Add mipmap size (in bytes)
    }

    if (image->mipmaps < mipCount)
    {
        void *temp = RL_REALLOC(image->data, mipSize);

        if (temp != NULL) image->data = temp;      // Assign new pointer (new size) to store mipmaps data
        else TRACELOG(LOG_WARNING, "IMAGE: Mipmaps required memory could not be allocated");

        // Pointer to allocated memory point where store next mipmap level data
        unsigned char *nextmip = (unsigned char *)image->data + GetPixelDataSize(image->width, image->height, image->format);

        mipWidth = image->width/2;
        mipHeight = image->height/2;
        mipSize = GetPixelDataSize(mipWidth, mipHeight, image->format);
        Image imCopy = ImageCopy(*image);

        for (int i = 1; i < mipCount; i++)
        {
            TRACELOGD("IMAGE: Generating mipmap level: %i (%i x %i) - size: %i - offset: 0x%x", i, mipWidth, mipHeight, mipSize, nextmip);

            ImageResize(&imCopy, mipWidth, mipHeight);  // Uses internally Mitchell cubic downscale filter

            memcpy(nextmip, imCopy.data, mipSize);
            nextmip += mipSize;
            image->mipmaps++;

            mipWidth /= 2;
            mipHeight /= 2;

            // Security check for NPOT textures
            if (mipWidth < 1) mipWidth = 1;
            if (mipHeight < 1) mipHeight = 1;

            mipSize = GetPixelDataSize(mipWidth, mipHeight, image->format);
        }

        UnloadImage(imCopy);
    }
    else TRACELOG(LOG_WARNING, "IMAGE: Mipmaps already available");
}

// Dither image data to 16bpp or lower (Floyd-Steinberg dithering)
// NOTE: In case selected bpp do not represent an known 16bit format,
// dithered data is stored in the LSB part of the unsigned short
void ImageDither(Image *image, int rBpp, int gBpp, int bBpp, int aBpp)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB)
    {
        TRACELOG(LOG_WARNING, "IMAGE: Compressed data formats can not be dithered");
        return;
    }

    if ((rBpp + gBpp + bBpp + aBpp) > 16)
    {
        TRACELOG(LOG_WARNING, "IMAGE: Unsupported dithering bpps (%ibpp), only 16bpp or lower modes supported", (rBpp+gBpp+bBpp+aBpp));
    }
    else
    {
        Color *pixels = LoadImageColors(*image);

        RL_FREE(image->data);      // free old image data

        if ((image->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8) && (image->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8))
        {
            TRACELOG(LOG_WARNING, "IMAGE: Format is already 16bpp or lower, dithering could have no effect");
        }

        // Define new image format, check if desired bpp match internal known format
        if ((rBpp == 5) && (gBpp == 6) && (bBpp == 5) && (aBpp == 0)) image->format = PIXELFORMAT_UNCOMPRESSED_R5G6B5;
        else if ((rBpp == 5) && (gBpp == 5) && (bBpp == 5) && (aBpp == 1)) image->format = PIXELFORMAT_UNCOMPRESSED_R5G5B5A1;
        else if ((rBpp == 4) && (gBpp == 4) && (bBpp == 4) && (aBpp == 4)) image->format = PIXELFORMAT_UNCOMPRESSED_R4G4B4A4;
        else
        {
            image->format = 0;
            TRACELOG(LOG_WARNING, "IMAGE: Unsupported dithered OpenGL internal format: %ibpp (R%iG%iB%iA%i)", (rBpp+gBpp+bBpp+aBpp), rBpp, gBpp, bBpp, aBpp);
        }

        // NOTE: We will store the dithered data as unsigned short (16bpp)
        image->data = (unsigned short *)RL_MALLOC(image->width*image->height*sizeof(unsigned short));

        Color oldPixel = WHITE;
        Color newPixel = WHITE;

        int rError, gError, bError;
        unsigned short rPixel, gPixel, bPixel, aPixel;   // Used for 16bit pixel composition

        #define MIN(a,b) (((a)<(b))?(a):(b))

        for (int y = 0; y < image->height; y++)
        {
            for (int x = 0; x < image->width; x++)
            {
                oldPixel = pixels[y*image->width + x];

                // NOTE: New pixel obtained by bits truncate, it would be better to round values (check ImageFormat())
                newPixel.r = oldPixel.r >> (8 - rBpp);     // R bits
                newPixel.g = oldPixel.g >> (8 - gBpp);     // G bits
                newPixel.b = oldPixel.b >> (8 - bBpp);     // B bits
                newPixel.a = oldPixel.a >> (8 - aBpp);     // A bits (not used on dithering)

                // NOTE: Error must be computed between new and old pixel but using same number of bits!
                // We want to know how much color precision we have lost...
                rError = (int)oldPixel.r - (int)(newPixel.r << (8 - rBpp));
                gError = (int)oldPixel.g - (int)(newPixel.g << (8 - gBpp));
                bError = (int)oldPixel.b - (int)(newPixel.b << (8 - bBpp));

                pixels[y*image->width + x] = newPixel;

                // NOTE: Some cases are out of the array and should be ignored
                if (x < (image->width - 1))
                {
                    pixels[y*image->width + x+1].r = MIN((int)pixels[y*image->width + x+1].r + (int)((float)rError*7.0f/16), 0xff);
                    pixels[y*image->width + x+1].g = MIN((int)pixels[y*image->width + x+1].g + (int)((float)gError*7.0f/16), 0xff);
                    pixels[y*image->width + x+1].b = MIN((int)pixels[y*image->width + x+1].b + (int)((float)bError*7.0f/16), 0xff);
                }

                if ((x > 0) && (y < (image->height - 1)))
                {
                    pixels[(y+1)*image->width + x-1].r = MIN((int)pixels[(y+1)*image->width + x-1].r + (int)((float)rError*3.0f/16), 0xff);
                    pixels[(y+1)*image->width + x-1].g = MIN((int)pixels[(y+1)*image->width + x-1].g + (int)((float)gError*3.0f/16), 0xff);
                    pixels[(y+1)*image->width + x-1].b = MIN((int)pixels[(y+1)*image->width + x-1].b + (int)((float)bError*3.0f/16), 0xff);
                }

                if (y < (image->height - 1))
                {
                    pixels[(y+1)*image->width + x].r = MIN((int)pixels[(y+1)*image->width + x].r + (int)((float)rError*5.0f/16), 0xff);
                    pixels[(y+1)*image->width + x].g = MIN((int)pixels[(y+1)*image->width + x].g + (int)((float)gError*5.0f/16), 0xff);
                    pixels[(y+1)*image->width + x].b = MIN((int)pixels[(y+1)*image->width + x].b + (int)((float)bError*5.0f/16), 0xff);
                }

                if ((x < (image->width - 1)) && (y < (image->height - 1)))
                {
                    pixels[(y+1)*image->width + x+1].r = MIN((int)pixels[(y+1)*image->width + x+1].r + (int)((float)rError*1.0f/16), 0xff);
                    pixels[(y+1)*image->width + x+1].g = MIN((int)pixels[(y+1)*image->width + x+1].g + (int)((float)gError*1.0f/16), 0xff);
                    pixels[(y+1)*image->width + x+1].b = MIN((int)pixels[(y+1)*image->width + x+1].b + (int)((float)bError*1.0f/16), 0xff);
                }

                rPixel = (unsigned short)newPixel.r;
                gPixel = (unsigned short)newPixel.g;
                bPixel = (unsigned short)newPixel.b;
                aPixel = (unsigned short)newPixel.a;

                ((unsigned short *)image->data)[y*image->width + x] = (rPixel << (gBpp + bBpp + aBpp)) | (gPixel << (bBpp + aBpp)) | (bPixel << aBpp) | aPixel;
            }
        }

        UnloadImageColors(pixels);
    }
}

// Flip image vertically
void ImageFlipVertical(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *flippedData = (unsigned char *)RL_MALLOC(image->width*image->height*bytesPerPixel);

        for (int i = (image->height - 1), offsetSize = 0; i >= 0; i--)
        {
            memcpy(flippedData + offsetSize, ((unsigned char *)image->data) + i*image->width*bytesPerPixel, image->width*bytesPerPixel);
            offsetSize += image->width*bytesPerPixel;
        }

        RL_FREE(image->data);
        image->data = flippedData;
    }
}

// Flip image horizontally
void ImageFlipHorizontal(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *flippedData = (unsigned char *)RL_MALLOC(image->width*image->height*bytesPerPixel);

        for (int y = 0; y < image->height; y++)
        {
            for (int x = 0; x < image->width; x++)
            {
                // OPTION 1: Move pixels with memcopy()
                //memcpy(flippedData + (y*image->width + x)*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + (image->width - 1 - x))*bytesPerPixel, bytesPerPixel);

                // OPTION 2: Just copy data pixel by pixel
                for (int i = 0; i < bytesPerPixel; i++) flippedData[(y*image->width + x)*bytesPerPixel + i] = ((unsigned char *)image->data)[(y*image->width + (image->width - 1 - x))*bytesPerPixel + i];
            }
        }

        RL_FREE(image->data);
        image->data = flippedData;

        /*
        // OPTION 3: Faster implementation (specific for 32bit pixels)
        // NOTE: It does not require additional allocations
        uint32_t *ptr = (uint32_t *)image->data;
        for (int y = 0; y < image->height; y++)
        {
            for (int x = 0; x < image->width/2; x++)
            {
                uint32_t backup = ptr[y*image->width + x];
                ptr[y*image->width + x] = ptr[y*image->width + (image->width - 1 - x)];
                ptr[y*image->width + (image->width - 1 - x)] = backup;
            }
        }
        */
    }
}

// Rotate image clockwise 90deg
void ImageRotateCW(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *rotatedData = (unsigned char *)RL_MALLOC(image->width*image->height*bytesPerPixel);

        for (int y = 0; y < image->height; y++)
        {
            for (int x = 0; x < image->width; x++)
            {
                //memcpy(rotatedData + (x*image->height + (image->height - y - 1))*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + x)*bytesPerPixel, bytesPerPixel);
                for (int i = 0; i < bytesPerPixel; i++) rotatedData[(x*image->height + (image->height - y - 1))*bytesPerPixel + i] = ((unsigned char *)image->data)[(y*image->width + x)*bytesPerPixel + i];
            }
        }

        RL_FREE(image->data);
        image->data = rotatedData;
        int width = image->width;
        int height = image-> height;

        image->width = height;
        image->height = width;
    }
}

// Rotate image counter-clockwise 90deg
void ImageRotateCCW(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (image->mipmaps > 1) TRACELOG(LOG_WARNING, "Image manipulation only applied to base mipmap level");
    if (image->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image manipulation not supported for compressed formats");
    else
    {
        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        unsigned char *rotatedData = (unsigned char *)RL_MALLOC(image->width*image->height*bytesPerPixel);

        for (int y = 0; y < image->height; y++)
        {
            for (int x = 0; x < image->width; x++)
            {
                //memcpy(rotatedData + (x*image->height + y))*bytesPerPixel, ((unsigned char *)image->data) + (y*image->width + (image->width - x - 1))*bytesPerPixel, bytesPerPixel);
                for (int i = 0; i < bytesPerPixel; i++) rotatedData[(x*image->height + y)*bytesPerPixel + i] = ((unsigned char *)image->data)[(y*image->width + (image->width - x - 1))*bytesPerPixel + i];
            }
        }

        RL_FREE(image->data);
        image->data = rotatedData;
        int width = image->width;
        int height = image-> height;

        image->width = height;
        image->height = width;
    }
}

// Modify image color: tint
void ImageColorTint(Image *image, Color color)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    Color *pixels = LoadImageColors(*image);

    float cR = (float)color.r/255;
    float cG = (float)color.g/255;
    float cB = (float)color.b/255;
    float cA = (float)color.a/255;

    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            int index = y*image->width + x;
            unsigned char r = (unsigned char)(((float)pixels[index].r/255*cR)*255.0f);
            unsigned char g = (unsigned char)(((float)pixels[index].g/255*cG)*255.0f);
            unsigned char b = (unsigned char)(((float)pixels[index].b/255*cB)*255.0f);
            unsigned char a = (unsigned char)(((float)pixels[index].a/255*cA)*255.0f);

            pixels[y*image->width + x].r = r;
            pixels[y*image->width + x].g = g;
            pixels[y*image->width + x].b = b;
            pixels[y*image->width + x].a = a;
        }
    }

    int format = image->format;
    RL_FREE(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: invert
void ImageColorInvert(Image *image)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    Color *pixels = LoadImageColors(*image);

    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            pixels[y*image->width + x].r = 255 - pixels[y*image->width + x].r;
            pixels[y*image->width + x].g = 255 - pixels[y*image->width + x].g;
            pixels[y*image->width + x].b = 255 - pixels[y*image->width + x].b;
        }
    }

    int format = image->format;
    RL_FREE(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: grayscale
void ImageColorGrayscale(Image *image)
{
    ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
}

// Modify image color: contrast
// NOTE: Contrast values between -100 and 100
void ImageColorContrast(Image *image, float contrast)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (contrast < -100) contrast = -100;
    if (contrast > 100) contrast = 100;

    contrast = (100.0f + contrast)/100.0f;
    contrast *= contrast;

    Color *pixels = LoadImageColors(*image);

    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            float pR = (float)pixels[y*image->width + x].r/255.0f;
            pR -= 0.5f;
            pR *= contrast;
            pR += 0.5f;
            pR *= 255;
            if (pR < 0) pR = 0;
            if (pR > 255) pR = 255;

            float pG = (float)pixels[y*image->width + x].g/255.0f;
            pG -= 0.5f;
            pG *= contrast;
            pG += 0.5f;
            pG *= 255;
            if (pG < 0) pG = 0;
            if (pG > 255) pG = 255;

            float pB = (float)pixels[y*image->width + x].b/255.0f;
            pB -= 0.5f;
            pB *= contrast;
            pB += 0.5f;
            pB *= 255;
            if (pB < 0) pB = 0;
            if (pB > 255) pB = 255;

            pixels[y*image->width + x].r = (unsigned char)pR;
            pixels[y*image->width + x].g = (unsigned char)pG;
            pixels[y*image->width + x].b = (unsigned char)pB;
        }
    }

    int format = image->format;
    RL_FREE(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: brightness
// NOTE: Brightness values between -255 and 255
void ImageColorBrightness(Image *image, int brightness)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    if (brightness < -255) brightness = -255;
    if (brightness > 255) brightness = 255;

    Color *pixels = LoadImageColors(*image);

    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            int cR = pixels[y*image->width + x].r + brightness;
            int cG = pixels[y*image->width + x].g + brightness;
            int cB = pixels[y*image->width + x].b + brightness;

            if (cR < 0) cR = 1;
            if (cR > 255) cR = 255;

            if (cG < 0) cG = 1;
            if (cG > 255) cG = 255;

            if (cB < 0) cB = 1;
            if (cB > 255) cB = 255;

            pixels[y*image->width + x].r = (unsigned char)cR;
            pixels[y*image->width + x].g = (unsigned char)cG;
            pixels[y*image->width + x].b = (unsigned char)cB;
        }
    }

    int format = image->format;
    RL_FREE(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}

// Modify image color: replace color
void ImageColorReplace(Image *image, Color color, Color replace)
{
    // Security check to avoid program crash
    if ((image->data == NULL) || (image->width == 0) || (image->height == 0)) return;

    Color *pixels = LoadImageColors(*image);

    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            if ((pixels[y*image->width + x].r == color.r) &&
                (pixels[y*image->width + x].g == color.g) &&
                (pixels[y*image->width + x].b == color.b) &&
                (pixels[y*image->width + x].a == color.a))
            {
                pixels[y*image->width + x].r = replace.r;
                pixels[y*image->width + x].g = replace.g;
                pixels[y*image->width + x].b = replace.b;
                pixels[y*image->width + x].a = replace.a;
            }
        }
    }

    int format = image->format;
    RL_FREE(image->data);

    image->data = pixels;
    image->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    ImageFormat(image, format);
}
#endif      // SUPPORT_IMAGE_MANIPULATION

// Load color data from image as a Color array (RGBA - 32bit)
// NOTE: Memory allocated should be freed using UnloadImageColors();
Color *LoadImageColors(Image image)
{
    if ((image.width == 0) || (image.height == 0)) return NULL;

    Color *pixels = (Color *)RL_MALLOC(image.width*image.height*sizeof(Color));

    if (image.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "IMAGE: Pixel data retrieval not supported for compressed image formats");
    else
    {
        if ((image.format == PIXELFORMAT_UNCOMPRESSED_R32) ||
            (image.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32) ||
            (image.format == PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)) TRACELOG(LOG_WARNING, "IMAGE: Pixel format converted from 32bit to 8bit per channel");

        for (int i = 0, k = 0; i < image.width*image.height; i++)
        {
            switch (image.format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    pixels[i].r = ((unsigned char *)image.data)[i];
                    pixels[i].g = ((unsigned char *)image.data)[i];
                    pixels[i].b = ((unsigned char *)image.data)[i];
                    pixels[i].a = 255;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    pixels[i].r = ((unsigned char *)image.data)[k];
                    pixels[i].g = ((unsigned char *)image.data)[k];
                    pixels[i].b = ((unsigned char *)image.data)[k];
                    pixels[i].a = ((unsigned char *)image.data)[k + 1];

                    k += 2;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].r = (unsigned char)((float)((pixel & 0b1111100000000000) >> 11)*(255/31));
                    pixels[i].g = (unsigned char)((float)((pixel & 0b0000011111000000) >> 6)*(255/31));
                    pixels[i].b = (unsigned char)((float)((pixel & 0b0000000000111110) >> 1)*(255/31));
                    pixels[i].a = (unsigned char)((pixel & 0b0000000000000001)*255);

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].r = (unsigned char)((float)((pixel & 0b1111100000000000) >> 11)*(255/31));
                    pixels[i].g = (unsigned char)((float)((pixel & 0b0000011111100000) >> 5)*(255/63));
                    pixels[i].b = (unsigned char)((float)(pixel & 0b0000000000011111)*(255/31));
                    pixels[i].a = 255;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].r = (unsigned char)((float)((pixel & 0b1111000000000000) >> 12)*(255/15));
                    pixels[i].g = (unsigned char)((float)((pixel & 0b0000111100000000) >> 8)*(255/15));
                    pixels[i].b = (unsigned char)((float)((pixel & 0b0000000011110000) >> 4)*(255/15));
                    pixels[i].a = (unsigned char)((float)(pixel & 0b0000000000001111)*(255/15));

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    pixels[i].r = ((unsigned char *)image.data)[k];
                    pixels[i].g = ((unsigned char *)image.data)[k + 1];
                    pixels[i].b = ((unsigned char *)image.data)[k + 2];
                    pixels[i].a = ((unsigned char *)image.data)[k + 3];

                    k += 4;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    pixels[i].r = (unsigned char)((unsigned char *)image.data)[k];
                    pixels[i].g = (unsigned char)((unsigned char *)image.data)[k + 1];
                    pixels[i].b = (unsigned char)((unsigned char *)image.data)[k + 2];
                    pixels[i].a = 255;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    pixels[i].r = (unsigned char)(((float *)image.data)[k]*255.0f);
                    pixels[i].g = 0;
                    pixels[i].b = 0;
                    pixels[i].a = 255;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    pixels[i].r = (unsigned char)(((float *)image.data)[k]*255.0f);
                    pixels[i].g = (unsigned char)(((float *)image.data)[k + 1]*255.0f);
                    pixels[i].b = (unsigned char)(((float *)image.data)[k + 2]*255.0f);
                    pixels[i].a = 255;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    pixels[i].r = (unsigned char)(((float *)image.data)[k]*255.0f);
                    pixels[i].g = (unsigned char)(((float *)image.data)[k]*255.0f);
                    pixels[i].b = (unsigned char)(((float *)image.data)[k]*255.0f);
                    pixels[i].a = (unsigned char)(((float *)image.data)[k]*255.0f);

                    k += 4;
                } break;
                default: break;
            }
        }
    }

    return pixels;
}

// Load colors palette from image as a Color array (RGBA - 32bit)
// NOTE: Memory allocated should be freed using UnloadImagePalette()
Color *LoadImagePalette(Image image, int maxPaletteSize, int *colorCount)
{
    #define COLOR_EQUAL(col1, col2) ((col1.r == col2.r)&&(col1.g == col2.g)&&(col1.b == col2.b)&&(col1.a == col2.a))

    int palCount = 0;
    Color *palette = NULL;
    Color *pixels = LoadImageColors(image);

    if (pixels != NULL)
    {
        palette = (Color *)RL_MALLOC(maxPaletteSize*sizeof(Color));

        for (int i = 0; i < maxPaletteSize; i++) palette[i] = BLANK;   // Set all colors to BLANK

        for (int i = 0; i < image.width*image.height; i++)
        {
            if (pixels[i].a > 0)
            {
                bool colorInPalette = false;

                // Check if the color is already on palette
                for (int j = 0; j < maxPaletteSize; j++)
                {
                    if (COLOR_EQUAL(pixels[i], palette[j]))
                    {
                        colorInPalette = true;
                        break;
                    }
                }

                // Store color if not on the palette
                if (!colorInPalette)
                {
                    palette[palCount] = pixels[i];      // Add pixels[i] to palette
                    palCount++;

                    // We reached the limit of colors supported by palette
                    if (palCount >= maxPaletteSize)
                    {
                        i = image.width*image.height;   // Finish palette get
                        TRACELOG(LOG_WARNING, "IMAGE: Palette is greater than %i colors", maxPaletteSize);
                    }
                }
            }
        }

        UnloadImageColors(pixels);
    }

    *colorCount = palCount;

    return palette;
}

// Unload color data loaded with LoadImageColors()
void UnloadImageColors(Color *colors)
{
    RL_FREE(colors);
}

// Unload colors palette loaded with LoadImagePalette()
void UnloadImagePalette(Color *colors)
{
    RL_FREE(colors);
}

// Get image alpha border rectangle
// NOTE: Threshold is defined as a percentatge: 0.0f -> 1.0f
Rectangle GetImageAlphaBorder(Image image, float threshold)
{
    Rectangle crop = { 0 };

    Color *pixels = LoadImageColors(image);

    if (pixels != NULL)
    {
        int xMin = 65536;   // Define a big enough number
        int xMax = 0;
        int yMin = 65536;
        int yMax = 0;

        for (int y = 0; y < image.height; y++)
        {
            for (int x = 0; x < image.width; x++)
            {
                if (pixels[y*image.width + x].a > (unsigned char)(threshold*255.0f))
                {
                    if (x < xMin) xMin = x;
                    if (x > xMax) xMax = x;
                    if (y < yMin) yMin = y;
                    if (y > yMax) yMax = y;
                }
            }
        }

        // Check for empty blank image
        if ((xMin != 65536) && (xMax != 65536))
        {
            crop = (Rectangle){ (float)xMin, (float)yMin, (float)((xMax + 1) - xMin), (float)((yMax + 1) - yMin) };
        }

        UnloadImageColors(pixels);
    }

    return crop;
}

// Get image pixel color at (x, y) position
Color GetImageColor(Image image, int x, int y)
{
    Color color = { 0 };

    if ((x >=0) && (x < image.width) && (y >= 0) && (y < image.height))
    {
        switch (image.format)
        {
            case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
            {
                color.r = ((unsigned char *)image.data)[y*image.width + x];
                color.g = ((unsigned char *)image.data)[y*image.width + x];
                color.b = ((unsigned char *)image.data)[y*image.width + x];
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
            {
                color.r = ((unsigned char *)image.data)[(y*image.width + x)*2];
                color.g = ((unsigned char *)image.data)[(y*image.width + x)*2];
                color.b = ((unsigned char *)image.data)[(y*image.width + x)*2];
                color.a = ((unsigned char *)image.data)[(y*image.width + x)*2 + 1];

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
            {
                unsigned short pixel = ((unsigned short *)image.data)[y*image.width + x];

                color.r = (unsigned char)((float)((pixel & 0b1111100000000000) >> 11)*(255/31));
                color.g = (unsigned char)((float)((pixel & 0b0000011111000000) >> 6)*(255/31));
                color.b = (unsigned char)((float)((pixel & 0b0000000000111110) >> 1)*(255/31));
                color.a = (unsigned char)((pixel & 0b0000000000000001)*255);

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
            {
                unsigned short pixel = ((unsigned short *)image.data)[y*image.width + x];

                color.r = (unsigned char)((float)((pixel & 0b1111100000000000) >> 11)*(255/31));
                color.g = (unsigned char)((float)((pixel & 0b0000011111100000) >> 5)*(255/63));
                color.b = (unsigned char)((float)(pixel & 0b0000000000011111)*(255/31));
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
            {
                unsigned short pixel = ((unsigned short *)image.data)[y*image.width + x];

                color.r = (unsigned char)((float)((pixel & 0b1111000000000000) >> 12)*(255/15));
                color.g = (unsigned char)((float)((pixel & 0b0000111100000000) >> 8)*(255/15));
                color.b = (unsigned char)((float)((pixel & 0b0000000011110000) >> 4)*(255/15));
                color.a = (unsigned char)((float)(pixel & 0b0000000000001111)*(255/15));

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
            {
                color.r = ((unsigned char *)image.data)[(y*image.width + x)*4];
                color.g = ((unsigned char *)image.data)[(y*image.width + x)*4 + 1];
                color.b = ((unsigned char *)image.data)[(y*image.width + x)*4 + 2];
                color.a = ((unsigned char *)image.data)[(y*image.width + x)*4 + 3];

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
            {
                color.r = (unsigned char)((unsigned char *)image.data)[(y*image.width + x)*3];
                color.g = (unsigned char)((unsigned char *)image.data)[(y*image.width + x)*3 + 1];
                color.b = (unsigned char)((unsigned char *)image.data)[(y*image.width + x)*3 + 2];
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32:
            {
                color.r = (unsigned char)(((float *)image.data)[y*image.width + x]*255.0f);
                color.g = 0;
                color.b = 0;
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
            {
                color.r = (unsigned char)(((float *)image.data)[(y*image.width + x)*3]*255.0f);
                color.g = (unsigned char)(((float *)image.data)[(y*image.width + x)*3 + 1]*255.0f);
                color.b = (unsigned char)(((float *)image.data)[(y*image.width + x)*3 + 2]*255.0f);
                color.a = 255;

            } break;
            case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
            {
                color.r = (unsigned char)(((float *)image.data)[(y*image.width + x)*4]*255.0f);
                color.g = (unsigned char)(((float *)image.data)[(y*image.width + x)*4]*255.0f);
                color.b = (unsigned char)(((float *)image.data)[(y*image.width + x)*4]*255.0f);
                color.a = (unsigned char)(((float *)image.data)[(y*image.width + x)*4]*255.0f);

            } break;
            default: TRACELOG(LOG_WARNING, "Compressed image format does not support color reading"); break;
        }
    }
    else TRACELOG(LOG_WARNING, "Requested image pixel (%i, %i) out of bounds", x, y);

    return color;
}

//------------------------------------------------------------------------------------
// Image drawing functions
//------------------------------------------------------------------------------------
// Clear image background with given color
void ImageClearBackground(Image *dst, Color color)
{
    for (int i = 0; i < dst->width*dst->height; ++i) ImageDrawPixel(dst, i%dst->width, i/dst->width, color);
}

// Draw pixel within an image
// NOTE: Compressed image formats not supported
void ImageDrawPixel(Image *dst, int x, int y, Color color)
{
    // Security check to avoid program crash
    if ((dst->data == NULL) || (x < 0) || (x >= dst->width) || (y < 0) || (y >= dst->height)) return;

    switch (dst->format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
        {
            // NOTE: Calculate grayscale equivalent color
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };
            unsigned char gray = (unsigned char)((coln.x*0.299f + coln.y*0.587f + coln.z*0.114f)*255.0f);

            ((unsigned char *)dst->data)[y*dst->width + x] = gray;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
        {
            // NOTE: Calculate grayscale equivalent color
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };
            unsigned char gray = (unsigned char)((coln.x*0.299f + coln.y*0.587f + coln.z*0.114f)*255.0f);

            ((unsigned char *)dst->data)[(y*dst->width + x)*2] = gray;
            ((unsigned char *)dst->data)[(y*dst->width + x)*2 + 1] = color.a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        {
            // NOTE: Calculate R5G6B5 equivalent color
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };

            unsigned char r = (unsigned char)(round(coln.x*31.0f));
            unsigned char g = (unsigned char)(round(coln.y*63.0f));
            unsigned char b = (unsigned char)(round(coln.z*31.0f));

            ((unsigned short *)dst->data)[y*dst->width + x] = (unsigned short)r << 11 | (unsigned short)g << 5 | (unsigned short)b;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        {
            // NOTE: Calculate R5G5B5A1 equivalent color
            Vector4 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f, (float)color.a/255.0f };

            unsigned char r = (unsigned char)(round(coln.x*31.0f));
            unsigned char g = (unsigned char)(round(coln.y*31.0f));
            unsigned char b = (unsigned char)(round(coln.z*31.0f));
            unsigned char a = (coln.w > ((float)PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD/255.0f))? 1 : 0;

            ((unsigned short *)dst->data)[y*dst->width + x] = (unsigned short)r << 11 | (unsigned short)g << 6 | (unsigned short)b << 1 | (unsigned short)a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
        {
            // NOTE: Calculate R5G5B5A1 equivalent color
            Vector4 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f, (float)color.a/255.0f };

            unsigned char r = (unsigned char)(round(coln.x*15.0f));
            unsigned char g = (unsigned char)(round(coln.y*15.0f));
            unsigned char b = (unsigned char)(round(coln.z*15.0f));
            unsigned char a = (unsigned char)(round(coln.w*15.0f));

            ((unsigned short *)dst->data)[y*dst->width + x] = (unsigned short)r << 12 | (unsigned short)g << 8 | (unsigned short)b << 4 | (unsigned short)a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
        {
            ((unsigned char *)dst->data)[(y*dst->width + x)*3] = color.r;
            ((unsigned char *)dst->data)[(y*dst->width + x)*3 + 1] = color.g;
            ((unsigned char *)dst->data)[(y*dst->width + x)*3 + 2] = color.b;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
        {
            ((unsigned char *)dst->data)[(y*dst->width + x)*4] = color.r;
            ((unsigned char *)dst->data)[(y*dst->width + x)*4 + 1] = color.g;
            ((unsigned char *)dst->data)[(y*dst->width + x)*4 + 2] = color.b;
            ((unsigned char *)dst->data)[(y*dst->width + x)*4 + 3] = color.a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32:
        {
            // NOTE: Calculate grayscale equivalent color (normalized to 32bit)
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };

            ((float *)dst->data)[y*dst->width + x] = coln.x*0.299f + coln.y*0.587f + coln.z*0.114f;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
        {
            // NOTE: Calculate R32G32B32 equivalent color (normalized to 32bit)
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };

            ((float *)dst->data)[(y*dst->width + x)*3] = coln.x;
            ((float *)dst->data)[(y*dst->width + x)*3 + 1] = coln.y;
            ((float *)dst->data)[(y*dst->width + x)*3 + 2] = coln.z;
        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
        {
            // NOTE: Calculate R32G32B32A32 equivalent color (normalized to 32bit)
            Vector4 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f, (float)color.a/255.0f };

            ((float *)dst->data)[(y*dst->width + x)*4] = coln.x;
            ((float *)dst->data)[(y*dst->width + x)*4 + 1] = coln.y;
            ((float *)dst->data)[(y*dst->width + x)*4 + 2] = coln.z;
            ((float *)dst->data)[(y*dst->width + x)*4 + 3] = coln.w;

        } break;
        default: break;
    }
}

// Draw pixel within an image (Vector version)
void ImageDrawPixelV(Image *dst, Vector2 position, Color color)
{
    ImageDrawPixel(dst, (int)position.x, (int)position.y, color);
}

// Draw line within an image
void ImageDrawLine(Image *dst, int startPosX, int startPosY, int endPosX, int endPosY, Color color)
{
    // Using Bresenham's algorithm as described in
    // Drawing Lines with Pixels - Joshua Scott - March 2012
    // https://classic.csunplugged.org/wp-content/uploads/2014/12/Lines.pdf

    int changeInX = (endPosX - startPosX);
    int absChangeInX = (changeInX < 0)? -changeInX : changeInX;
    int changeInY = (endPosY - startPosY);
    int absChangeInY = (changeInY < 0)? -changeInY : changeInY;

    int startU, startV, endU, stepV; // Substitutions, either U = X, V = Y or vice versa. See loop at end of function
    //int endV;     // Not needed but left for better understanding, check code below
    int A, B, P;    // See linked paper above, explained down in the main loop
    int reversedXY = (absChangeInY < absChangeInX);

    if (reversedXY)
    {
        A = 2*absChangeInY;
        B = A - 2*absChangeInX;
        P = A - absChangeInX;

        if (changeInX > 0)
        {
            startU = startPosX;
            startV = startPosY;
            endU = endPosX;
            //endV = endPosY;
        }
        else
        {
            startU = endPosX;
            startV = endPosY;
            endU = startPosX;
            //endV = startPosY;

            // Since start and end are reversed
            changeInX = -changeInX;
            changeInY = -changeInY;
        }

        stepV = (changeInY < 0)? -1 : 1;

        ImageDrawPixel(dst, startU, startV, color);     // At this point they are correctly ordered...
    }
    else
    {
        A = 2*absChangeInX;
        B = A - 2*absChangeInY;
        P = A - absChangeInY;

        if (changeInY > 0)
        {
            startU = startPosY;
            startV = startPosX;
            endU = endPosY;
            //endV = endPosX;
        }
        else
        {
            startU = endPosY;
            startV = endPosX;
            endU = startPosY;
            //endV = startPosX;

            // Since start and end are reversed
            changeInX = -changeInX;
            changeInY = -changeInY;
        }

        stepV = (changeInX < 0)? -1 : 1;

        ImageDrawPixel(dst, startV, startU, color);     // ... but need to be reversed here. Repeated in the main loop below
    }

    // We already drew the start point. If we started at startU + 0, the line would be crooked and too short
    for (int u = startU + 1, v = startV; u <= endU; u++)
    {
        if (P >= 0)
        {
            v += stepV;     // Adjusts whenever we stray too far from the direct line. Details in the linked paper above
            P += B;         // Remembers that we corrected our path
        }
        else P += A;        // Remembers how far we are from the direct line

        if (reversedXY) ImageDrawPixel(dst, u, v, color);
        else ImageDrawPixel(dst, v, u, color);
    }
}

// Draw line within an image (Vector version)
void ImageDrawLineV(Image *dst, Vector2 start, Vector2 end, Color color)
{
    ImageDrawLine(dst, (int)start.x, (int)start.y, (int)end.x, (int)end.y, color);
}

// Draw circle within an image
void ImageDrawCircle(Image *dst, int centerX, int centerY, int radius, Color color)
{
    int x = 0, y = radius;
    int decesionParameter = 3 - 2*radius;

    while (y >= x)
    {
        ImageDrawPixel(dst, centerX + x, centerY + y, color);
        ImageDrawPixel(dst, centerX - x, centerY + y, color);
        ImageDrawPixel(dst, centerX + x, centerY - y, color);
        ImageDrawPixel(dst, centerX - x, centerY - y, color);
        ImageDrawPixel(dst, centerX + y, centerY + x, color);
        ImageDrawPixel(dst, centerX - y, centerY + x, color);
        ImageDrawPixel(dst, centerX + y, centerY - x, color);
        ImageDrawPixel(dst, centerX - y, centerY - x, color);
        x++;

        if (decesionParameter > 0)
        {
            y--;
            decesionParameter = decesionParameter + 4*(x - y) + 10;
        }
        else decesionParameter = decesionParameter + 4*x + 6;
    }
}

// Draw circle within an image (Vector version)
void ImageDrawCircleV(Image *dst, Vector2 center, int radius, Color color)
{
    ImageDrawCircle(dst, (int)center.x, (int)center.y, radius, color);
}

// Draw rectangle within an image
void ImageDrawRectangle(Image *dst, int posX, int posY, int width, int height, Color color)
{
    ImageDrawRectangleRec(dst, (Rectangle){ (float)posX, (float)posY, (float)width, (float)height }, color);
}

// Draw rectangle within an image (Vector version)
void ImageDrawRectangleV(Image *dst, Vector2 position, Vector2 size, Color color)
{
    ImageDrawRectangle(dst, (int)position.x, (int)position.y, (int)size.x, (int)size.y, color);
}

// Draw rectangle within an image
void ImageDrawRectangleRec(Image *dst, Rectangle rec, Color color)
{
    // Security check to avoid program crash
    if ((dst->data == NULL) || (dst->width == 0) || (dst->height == 0)) return;

    int sy = (int)rec.y;
    int ey = sy + (int)rec.height;

    int sx = (int)rec.x;
    int ex = sx + (int)rec.width;

    for (int y = sy; y < ey; y++)
    {
        for (int x = sx; x < ex; x++)
        {
            ImageDrawPixel(dst, x, y, color);
        }
    }
}

// Draw rectangle lines within an image
void ImageDrawRectangleLines(Image *dst, Rectangle rec, int thick, Color color)
{
    ImageDrawRectangle(dst, (int)rec.x, (int)rec.y, (int)rec.width, thick, color);
    ImageDrawRectangle(dst, (int)rec.x, (int)(rec.y + thick), thick, (int)(rec.height - thick*2), color);
    ImageDrawRectangle(dst, (int)(rec.x + rec.width - thick), (int)(rec.y + thick), thick, (int)(rec.height - thick*2), color);
    ImageDrawRectangle(dst, (int)rec.x, (int)(rec.y + rec.height - thick), (int)rec.width, thick, color);
}

// Draw an image (source) within an image (destination)
// NOTE: Color tint is applied to source image
void ImageDraw(Image *dst, Image src, Rectangle srcRec, Rectangle dstRec, Color tint)
{
    // Security check to avoid program crash
    if ((dst->data == NULL) || (dst->width == 0) || (dst->height == 0) ||
        (src.data == NULL) || (src.width == 0) || (src.height == 0)) return;

    if (dst->mipmaps > 1) TRACELOG(LOG_WARNING, "Image drawing only applied to base mipmap level");
    if (dst->format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "Image drawing not supported for compressed formats");
    else
    {
        Image srcMod = { 0 };       // Source copy (in case it was required)
        Image *srcPtr = &src;       // Pointer to source image
        bool useSrcMod = false;     // Track source copy required

        // Source rectangle out-of-bounds security checks
        if (srcRec.x < 0) { srcRec.width += srcRec.x; srcRec.x = 0; }
        if (srcRec.y < 0) { srcRec.height += srcRec.y; srcRec.y = 0; }
        if ((srcRec.x + srcRec.width) > src.width) srcRec.width = src.width - srcRec.x;
        if ((srcRec.y + srcRec.height) > src.height) srcRec.height = src.height - srcRec.y;

        // Check if source rectangle needs to be resized to destination rectangle
        // In that case, we make a copy of source and we apply all required transform
        if (((int)srcRec.width != (int)dstRec.width) || ((int)srcRec.height != (int)dstRec.height))
        {
            srcMod = ImageFromImage(src, srcRec);   // Create image from another image
            ImageResize(&srcMod, (int)dstRec.width, (int)dstRec.height);   // Resize to destination rectangle
            srcRec = (Rectangle){ 0, 0, (float)srcMod.width, (float)srcMod.height };

            srcPtr = &srcMod;
            useSrcMod = true;
        }

        // Destination rectangle out-of-bounds security checks
        if (dstRec.x < 0)
        {
            srcRec.x = -dstRec.x;
            srcRec.width += dstRec.x;
            dstRec.x = 0;
        }
        else if ((dstRec.x + srcRec.width) > dst->width) srcRec.width = dst->width - dstRec.x;

        if (dstRec.y < 0)
        {
            srcRec.y = -dstRec.y;
            srcRec.height += dstRec.y;
            dstRec.y = 0;
        }
        else if ((dstRec.y + srcRec.height) > dst->height) srcRec.height = dst->height - dstRec.y;

        if (dst->width < srcRec.width) srcRec.width = (float)dst->width;
        if (dst->height < srcRec.height) srcRec.height = (float)dst->height;

        // This blitting method is quite fast! The process followed is:
        // for every pixel -> [get_src_format/get_dst_format -> blend -> format_to_dst]
        // Some optimization ideas:
        //    [x] Avoid creating source copy if not required (no resize required)
        //    [x] Optimize ImageResize() for pixel format (alternative: ImageResizeNN())
        //    [x] Optimize ColorAlphaBlend() to avoid processing (alpha = 0) and (alpha = 1)
        //    [x] Optimize ColorAlphaBlend() for faster operations (maybe avoiding divs?)
        //    [x] Consider fast path: no alpha blending required cases (src has no alpha)
        //    [x] Consider fast path: same src/dst format with no alpha -> direct line copy
        //    [-] GetPixelColor(): Get Vector4 instead of Color, easier for ColorAlphaBlend()
        //    [ ] Support f32bit channels drawing

        // TODO: Support PIXELFORMAT_UNCOMPRESSED_R32, PIXELFORMAT_UNCOMPRESSED_R32G32B32, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32

        Color colSrc, colDst, blend;
        bool blendRequired = true;

        // Fast path: Avoid blend if source has no alpha to blend
        if ((tint.a == 255) && ((srcPtr->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) || (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) || (srcPtr->format == PIXELFORMAT_UNCOMPRESSED_R5G6B5))) blendRequired = false;

        int strideDst = GetPixelDataSize(dst->width, 1, dst->format);
        int bytesPerPixelDst = strideDst/(dst->width);

        int strideSrc = GetPixelDataSize(srcPtr->width, 1, srcPtr->format);
        int bytesPerPixelSrc = strideSrc/(srcPtr->width);

        unsigned char *pSrcBase = (unsigned char *)srcPtr->data + ((int)srcRec.y*srcPtr->width + (int)srcRec.x)*bytesPerPixelSrc;
        unsigned char *pDstBase = (unsigned char *)dst->data + ((int)dstRec.y*dst->width + (int)dstRec.x)*bytesPerPixelDst;

        for (int y = 0; y < (int)srcRec.height; y++)
        {
            unsigned char *pSrc = pSrcBase;
            unsigned char *pDst = pDstBase;

            // Fast path: Avoid moving pixel by pixel if no blend required and same format
            if (!blendRequired && (srcPtr->format == dst->format)) memcpy(pDst, pSrc, (int)(srcRec.width)*bytesPerPixelSrc);
            else
            {
                for (int x = 0; x < (int)srcRec.width; x++)
                {
                    colSrc = GetPixelColor(pSrc, srcPtr->format);
                    colDst = GetPixelColor(pDst, dst->format);

                    // Fast path: Avoid blend if source has no alpha to blend
                    if (blendRequired) blend = ColorAlphaBlend(colDst, colSrc, tint);
                    else blend = colSrc;

                    SetPixelColor(pDst, blend, dst->format);

                    pDst += bytesPerPixelDst;
                    pSrc += bytesPerPixelSrc;
                }
            }

            pSrcBase += strideSrc;
            pDstBase += strideDst;
        }

        if (useSrcMod) UnloadImage(srcMod);     // Unload source modified image
    }
}

// Draw text (default font) within an image (destination)
void ImageDrawText(Image *dst, const char *text, int posX, int posY, int fontSize, Color color)
{
#if defined(SUPPORT_MODULE_RTEXT)
    Vector2 position = { (float)posX, (float)posY };
    // NOTE: For default font, spacing is set to desired font size / default font size (10)
    ImageDrawTextEx(dst, GetFontDefault(), text, position, (float)fontSize, (float)fontSize/10, color);   // WARNING: Module required: rtext
#else
    TRACELOG(LOG_WARNING, "IMAGE: ImageDrawText() requires module: rtext");
#endif
}

// Draw text (custom sprite font) within an image (destination)
void ImageDrawTextEx(Image *dst, Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint)
{
    Image imText = ImageTextEx(font, text, fontSize, spacing, tint);

    Rectangle srcRec = { 0.0f, 0.0f, (float)imText.width, (float)imText.height };
    Rectangle dstRec = { position.x, position.y, (float)imText.width, (float)imText.height };

    ImageDraw(dst, imText, srcRec, dstRec, WHITE);

    UnloadImage(imText);
}

//------------------------------------------------------------------------------------
// Texture loading functions
//------------------------------------------------------------------------------------
// Load texture from file into GPU memory (VRAM)
Texture2D LoadTexture(const char *fileName)
{
    Texture2D texture = { 0 };

    Image image = LoadImage(fileName);

    if (image.data != NULL)
    {
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
    }

    return texture;
}

// Load a texture from image data
// NOTE: image is not unloaded, it must be done manually
Texture2D LoadTextureFromImage(Image image)
{
    Texture2D texture = { 0 };

    if ((image.width != 0) && (image.height != 0))
    {
        texture.id = rlLoadTexture(image.data, image.width, image.height, image.format, image.mipmaps);
    }
    else TRACELOG(LOG_WARNING, "IMAGE: Data is not valid to load texture");

    texture.width = image.width;
    texture.height = image.height;
    texture.mipmaps = image.mipmaps;
    texture.format = image.format;

    return texture;
}

// Load cubemap from image, multiple image cubemap layouts supported
TextureCubemap LoadTextureCubemap(Image image, int layout)
{
    TextureCubemap cubemap = { 0 };

    if (layout == CUBEMAP_LAYOUT_AUTO_DETECT)      // Try to automatically guess layout type
    {
        // Check image width/height to determine the type of cubemap provided
        if (image.width > image.height)
        {
            if ((image.width/6) == image.height) { layout = CUBEMAP_LAYOUT_LINE_HORIZONTAL; cubemap.width = image.width/6; }
            else if ((image.width/4) == (image.height/3)) { layout = CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE; cubemap.width = image.width/4; }
            else if (image.width >= (int)((float)image.height*1.85f)) { layout = CUBEMAP_LAYOUT_PANORAMA; cubemap.width = image.width/4; }
        }
        else if (image.height > image.width)
        {
            if ((image.height/6) == image.width) { layout = CUBEMAP_LAYOUT_LINE_VERTICAL; cubemap.width = image.height/6; }
            else if ((image.width/3) == (image.height/4)) { layout = CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR; cubemap.width = image.width/3; }
        }

        cubemap.height = cubemap.width;
    }

    // Layout provided or already auto-detected
    if (layout != CUBEMAP_LAYOUT_AUTO_DETECT)
    {
        int size = cubemap.width;

        Image faces = { 0 };                // Vertical column image
        Rectangle faceRecs[6] = { 0 };      // Face source rectangles
        for (int i = 0; i < 6; i++) faceRecs[i] = (Rectangle){ 0, 0, (float)size, (float)size };

        if (layout == CUBEMAP_LAYOUT_LINE_VERTICAL)
        {
            faces = ImageCopy(image);       // Image data already follows expected convention
        }
        else if (layout == CUBEMAP_LAYOUT_PANORAMA)
        {
            // TODO: Convert panorama image to square faces...
            // Ref: https://github.com/denivip/panorama/blob/master/panorama.cpp
        }
        else
        {
            if (layout == CUBEMAP_LAYOUT_LINE_HORIZONTAL) for (int i = 0; i < 6; i++) faceRecs[i].x = (float)size*i;
            else if (layout == CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR)
            {
                faceRecs[0].x = (float)size; faceRecs[0].y = (float)size;
                faceRecs[1].x = (float)size; faceRecs[1].y = (float)size*3;
                faceRecs[2].x = (float)size; faceRecs[2].y = 0;
                faceRecs[3].x = (float)size; faceRecs[3].y = (float)size*2;
                faceRecs[4].x = 0; faceRecs[4].y = (float)size;
                faceRecs[5].x = (float)size*2; faceRecs[5].y = (float)size;
            }
            else if (layout == CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE)
            {
                faceRecs[0].x = (float)size*2; faceRecs[0].y = (float)size;
                faceRecs[1].x = 0; faceRecs[1].y = (float)size;
                faceRecs[2].x = (float)size; faceRecs[2].y = 0;
                faceRecs[3].x = (float)size; faceRecs[3].y = (float)size*2;
                faceRecs[4].x = (float)size; faceRecs[4].y = (float)size;
                faceRecs[5].x = (float)size*3; faceRecs[5].y = (float)size;
            }

            // Convert image data to 6 faces in a vertical column, that's the optimum layout for loading
            faces = GenImageColor(size, size*6, MAGENTA);
            ImageFormat(&faces, image.format);

            // NOTE: Image formating does not work with compressed textures

            for (int i = 0; i < 6; i++) ImageDraw(&faces, image, faceRecs[i], (Rectangle){ 0, (float)size*i, (float)size, (float)size }, WHITE);
        }

        // NOTE: Cubemap data is expected to be provided as 6 images in a single data array,
        // one after the other (that's a vertical image), following convention: +X, -X, +Y, -Y, +Z, -Z
        cubemap.id = rlLoadTextureCubemap(faces.data, size, faces.format);
        if (cubemap.id == 0) TRACELOG(LOG_WARNING, "IMAGE: Failed to load cubemap image");

        UnloadImage(faces);
    }
    else TRACELOG(LOG_WARNING, "IMAGE: Failed to detect cubemap image layout");

    return cubemap;
}

// Load texture for rendering (framebuffer)
// NOTE: Render texture is loaded by default with RGBA color attachment and depth RenderBuffer
RenderTexture2D LoadRenderTexture(int width, int height)
{
    RenderTexture2D target = { 0 };

    target.id = rlLoadFramebuffer(width, height);   // Load an empty framebuffer

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // Create color texture (default to RGBA)
        target.texture.id = rlLoadTexture(NULL, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        target.texture.width = width;
        target.texture.height = height;
        target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        target.texture.mipmaps = 1;

        // Create depth renderbuffer/texture
        target.depth.id = rlLoadTextureDepth(width, height, true);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19;       //DEPTH_COMPONENT_24BIT?
        target.depth.mipmaps = 1;

        // Attach color texture and depth renderbuffer/texture to FBO
        rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);

        // Check if fbo is complete with attachments (valid)
        if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);

        rlDisableFramebuffer();
    }
    else TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be created");

    return target;
}

// Unload texture from GPU memory (VRAM)
void UnloadTexture(Texture2D texture)
{
    if (texture.id > 0)
    {
        rlUnloadTexture(texture.id);

        TRACELOG(LOG_INFO, "TEXTURE: [ID %i] Unloaded texture data from VRAM (GPU)", texture.id);
    }
}

// Unload render texture from GPU memory (VRAM)
void UnloadRenderTexture(RenderTexture2D target)
{
    if (target.id > 0)
    {
        // Color texture attached to FBO is deleted
        rlUnloadTexture(target.texture.id);

        // NOTE: Depth texture/renderbuffer is automatically
        // queried and deleted before deleting framebuffer
        rlUnloadFramebuffer(target.id);
    }
}

// Update GPU texture with new data
// NOTE: pixels data must match texture.format
void UpdateTexture(Texture2D texture, const void *pixels)
{
    rlUpdateTexture(texture.id, 0, 0, texture.width, texture.height, texture.format, pixels);
}

// Update GPU texture rectangle with new data
// NOTE: pixels data must match texture.format
void UpdateTextureRec(Texture2D texture, Rectangle rec, const void *pixels)
{
    rlUpdateTexture(texture.id, (int)rec.x, (int)rec.y, (int)rec.width, (int)rec.height, texture.format, pixels);
}

//------------------------------------------------------------------------------------
// Texture configuration functions
//------------------------------------------------------------------------------------
// Generate GPU mipmaps for a texture
void GenTextureMipmaps(Texture2D *texture)
{
    // NOTE: NPOT textures support check inside function
    // On WebGL (OpenGL ES 2.0) NPOT textures support is limited
    rlGenTextureMipmaps(texture->id, texture->width, texture->height, texture->format, &texture->mipmaps);
}

// Set texture scaling filter mode
void SetTextureFilter(Texture2D texture, int filter)
{
    switch (filter)
    {
        case TEXTURE_FILTER_POINT:
        {
            if (texture.mipmaps > 1)
            {
                // RL_TEXTURE_FILTER_MIP_NEAREST - tex filter: POINT, mipmaps filter: POINT (sharp switching between mipmaps)
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_NEAREST);

                // RL_TEXTURE_FILTER_NEAREST - tex filter: POINT (no filter), no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
            }
            else
            {
                // RL_TEXTURE_FILTER_NEAREST - tex filter: POINT (no filter), no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
            }
        } break;
        case TEXTURE_FILTER_BILINEAR:
        {
            if (texture.mipmaps > 1)
            {
                // RL_TEXTURE_FILTER_LINEAR_MIP_NEAREST - tex filter: BILINEAR, mipmaps filter: POINT (sharp switching between mipmaps)
                // Alternative: RL_TEXTURE_FILTER_NEAREST_MIP_LINEAR - tex filter: POINT, mipmaps filter: BILINEAR (smooth transition between mipmaps)
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR_MIP_NEAREST);

                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
            else
            {
                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
        } break;
        case TEXTURE_FILTER_TRILINEAR:
        {
            if (texture.mipmaps > 1)
            {
                // RL_TEXTURE_FILTER_MIP_LINEAR - tex filter: BILINEAR, mipmaps filter: BILINEAR (smooth transition between mipmaps)
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);

                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
            else
            {
                TRACELOG(LOG_WARNING, "TEXTURE: [ID %i] No mipmaps available for TRILINEAR texture filtering", texture.id);

                // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
                rlTextureParameters(texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
                rlTextureParameters(texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
            }
        } break;
        case TEXTURE_FILTER_ANISOTROPIC_4X: rlTextureParameters(texture.id, RL_TEXTURE_FILTER_ANISOTROPIC, 4); break;
        case TEXTURE_FILTER_ANISOTROPIC_8X: rlTextureParameters(texture.id, RL_TEXTURE_FILTER_ANISOTROPIC, 8); break;
        case TEXTURE_FILTER_ANISOTROPIC_16X: rlTextureParameters(texture.id, RL_TEXTURE_FILTER_ANISOTROPIC, 16); break;
        default: break;
    }
}

// Set texture wrapping mode
void SetTextureWrap(Texture2D texture, int wrap)
{
    switch (wrap)
    {
        case TEXTURE_WRAP_REPEAT:
        {
            // NOTE: It only works if NPOT textures are supported, i.e. OpenGL ES 2.0 could not support it
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_REPEAT);
        } break;
        case TEXTURE_WRAP_CLAMP:
        {
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
        } break;
        case TEXTURE_WRAP_MIRROR_REPEAT:
        {
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_MIRROR_REPEAT);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_MIRROR_REPEAT);
        } break;
        case TEXTURE_WRAP_MIRROR_CLAMP:
        {
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_MIRROR_CLAMP);
            rlTextureParameters(texture.id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_MIRROR_CLAMP);
        } break;
        default: break;
    }
}

//------------------------------------------------------------------------------------
// Texture drawing functions
//------------------------------------------------------------------------------------
// Draw a Texture2D
void DrawTexture(Texture2D texture, int posX, int posY, Color tint)
{
    DrawTextureEx(texture, (Vector2){ (float)posX, (float)posY }, 0.0f, 1.0f, tint);
}

// Draw a Texture2D with position defined as Vector2
void DrawTextureV(Texture2D texture, Vector2 position, Color tint)
{
    DrawTextureEx(texture, position, 0, 1.0f, tint);
}

// Draw a Texture2D with extended parameters
void DrawTextureEx(Texture2D texture, Vector2 position, float rotation, float scale, Color tint)
{
    Rectangle source = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
    Rectangle dest = { position.x, position.y, (float)texture.width*scale, (float)texture.height*scale };
    Vector2 origin = { 0.0f, 0.0f };

    DrawTexturePro(texture, source, dest, origin, rotation, tint);
}

// Draw a part of a texture (defined by a rectangle)
void DrawTextureRec(Texture2D texture, Rectangle source, Vector2 position, Color tint)
{
    Rectangle dest = { position.x, position.y, fabsf(source.width), fabsf(source.height) };
    Vector2 origin = { 0.0f, 0.0f };

    DrawTexturePro(texture, source, dest, origin, 0.0f, tint);
}

// Draw texture quad with tiling and offset parameters
// NOTE: Tiling and offset should be provided considering normalized texture values [0..1]
// i.e tiling = { 1.0f, 1.0f } refers to all texture, offset = { 0.5f, 0.5f } moves texture origin to center
void DrawTextureQuad(Texture2D texture, Vector2 tiling, Vector2 offset, Rectangle quad, Color tint)
{
    // WARNING: This solution only works if TEXTURE_WRAP_REPEAT is supported,
    // NPOT textures supported is required and OpenGL ES 2.0 could not support it
    Rectangle source = { offset.x*texture.width, offset.y*texture.height, tiling.x*texture.width, tiling.y*texture.height };
    Vector2 origin = { 0.0f, 0.0f };

    DrawTexturePro(texture, source, quad, origin, 0.0f, tint);
}

// Draw part of a texture (defined by a rectangle) with rotation and scale tiled into dest.
// NOTE: For tilling a whole texture DrawTextureQuad() is better
void DrawTextureTiled(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, float scale, Color tint)
{
    if ((texture.id <= 0) || (scale <= 0.0f)) return;  // Wanna see a infinite loop?!...just delete this line!
    if ((source.width == 0) || (source.height == 0)) return;

    int tileWidth = (int)(source.width*scale), tileHeight = (int)(source.height*scale);
    if ((dest.width < tileWidth) && (dest.height < tileHeight))
    {
        // Can fit only one tile
        DrawTexturePro(texture, (Rectangle){source.x, source.y, ((float)dest.width/tileWidth)*source.width, ((float)dest.height/tileHeight)*source.height},
                    (Rectangle){dest.x, dest.y, dest.width, dest.height}, origin, rotation, tint);
    }
    else if (dest.width <= tileWidth)
    {
        // Tiled vertically (one column)
        int dy = 0;
        for (;dy+tileHeight < dest.height; dy += tileHeight)
        {
            DrawTexturePro(texture, (Rectangle){source.x, source.y, ((float)dest.width/tileWidth)*source.width, source.height}, (Rectangle){dest.x, dest.y + dy, dest.width, (float)tileHeight}, origin, rotation, tint);
        }

        // Fit last tile
        if (dy < dest.height)
        {
            DrawTexturePro(texture, (Rectangle){source.x, source.y, ((float)dest.width/tileWidth)*source.width, ((float)(dest.height - dy)/tileHeight)*source.height},
                        (Rectangle){dest.x, dest.y + dy, dest.width, dest.height - dy}, origin, rotation, tint);
        }
    }
    else if (dest.height <= tileHeight)
    {
        // Tiled horizontally (one row)
        int dx = 0;
        for (;dx+tileWidth < dest.width; dx += tileWidth)
        {
            DrawTexturePro(texture, (Rectangle){source.x, source.y, source.width, ((float)dest.height/tileHeight)*source.height}, (Rectangle){dest.x + dx, dest.y, (float)tileWidth, dest.height}, origin, rotation, tint);
        }

        // Fit last tile
        if (dx < dest.width)
        {
            DrawTexturePro(texture, (Rectangle){source.x, source.y, ((float)(dest.width - dx)/tileWidth)*source.width, ((float)dest.height/tileHeight)*source.height},
                        (Rectangle){dest.x + dx, dest.y, dest.width - dx, dest.height}, origin, rotation, tint);
        }
    }
    else
    {
        // Tiled both horizontally and vertically (rows and columns)
        int dx = 0;
        for (;dx+tileWidth < dest.width; dx += tileWidth)
        {
            int dy = 0;
            for (;dy+tileHeight < dest.height; dy += tileHeight)
            {
                DrawTexturePro(texture, source, (Rectangle){dest.x + dx, dest.y + dy, (float)tileWidth, (float)tileHeight}, origin, rotation, tint);
            }

            if (dy < dest.height)
            {
                DrawTexturePro(texture, (Rectangle){source.x, source.y, source.width, ((float)(dest.height - dy)/tileHeight)*source.height},
                    (Rectangle){dest.x + dx, dest.y + dy, (float)tileWidth, dest.height - dy}, origin, rotation, tint);
            }
        }

        // Fit last column of tiles
        if (dx < dest.width)
        {
            int dy = 0;
            for (;dy+tileHeight < dest.height; dy += tileHeight)
            {
                DrawTexturePro(texture, (Rectangle){source.x, source.y, ((float)(dest.width - dx)/tileWidth)*source.width, source.height},
                        (Rectangle){dest.x + dx, dest.y + dy, dest.width - dx, (float)tileHeight}, origin, rotation, tint);
            }

            // Draw final tile in the bottom right corner
            if (dy < dest.height)
            {
                DrawTexturePro(texture, (Rectangle){source.x, source.y, ((float)(dest.width - dx)/tileWidth)*source.width, ((float)(dest.height - dy)/tileHeight)*source.height},
                    (Rectangle){dest.x + dx, dest.y + dy, dest.width - dx, dest.height - dy}, origin, rotation, tint);
            }
        }
    }
}

// Draw a part of a texture (defined by a rectangle) with 'pro' parameters
// NOTE: origin is relative to destination rectangle size
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, Color tint)
{
    // Check if texture is valid
    if (texture.id > 0)
    {
        float width = (float)texture.width;
        float height = (float)texture.height;

        bool flipX = false;

        if (source.width < 0) { flipX = true; source.width *= -1; }
        if (source.height < 0) source.y -= source.height;

        Vector2 topLeft = { 0 };
        Vector2 topRight = { 0 };
        Vector2 bottomLeft = { 0 };
        Vector2 bottomRight = { 0 };

        // Only calculate rotation if needed
        if (rotation == 0.0f)
        {
            float x = dest.x - origin.x;
            float y = dest.y - origin.y;
            topLeft = (Vector2){ x, y };
            topRight = (Vector2){ x + dest.width, y };
            bottomLeft = (Vector2){ x, y + dest.height };
            bottomRight = (Vector2){ x + dest.width, y + dest.height };
        }
        else
        {
            float sinRotation = sinf(rotation*DEG2RAD);
            float cosRotation = cosf(rotation*DEG2RAD);
            float x = dest.x;
            float y = dest.y;
            float dx = -origin.x;
            float dy = -origin.y;

            topLeft.x = x + dx*cosRotation - dy*sinRotation;
            topLeft.y = y + dx*sinRotation + dy*cosRotation;

            topRight.x = x + (dx + dest.width)*cosRotation - dy*sinRotation;
            topRight.y = y + (dx + dest.width)*sinRotation + dy*cosRotation;

            bottomLeft.x = x + dx*cosRotation - (dy + dest.height)*sinRotation;
            bottomLeft.y = y + dx*sinRotation + (dy + dest.height)*cosRotation;

            bottomRight.x = x + (dx + dest.width)*cosRotation - (dy + dest.height)*sinRotation;
            bottomRight.y = y + (dx + dest.width)*sinRotation + (dy + dest.height)*cosRotation;
        }

        rlCheckRenderBatchLimit(4);     // Make sure there is enough free space on the batch buffer

        rlSetTexture(texture.id);
        rlBegin(RL_QUADS);

            rlColor4ub(tint.r, tint.g, tint.b, tint.a);
            rlNormal3f(0.0f, 0.0f, 1.0f);                          // Normal vector pointing towards viewer

            // Top-left corner for texture and quad
            if (flipX) rlTexCoord2f((source.x + source.width)/width, source.y/height);
            else rlTexCoord2f(source.x/width, source.y/height);
            rlVertex2f(topLeft.x, topLeft.y);

            // Bottom-left corner for texture and quad
            if (flipX) rlTexCoord2f((source.x + source.width)/width, (source.y + source.height)/height);
            else rlTexCoord2f(source.x/width, (source.y + source.height)/height);
            rlVertex2f(bottomLeft.x, bottomLeft.y);

            // Bottom-right corner for texture and quad
            if (flipX) rlTexCoord2f(source.x/width, (source.y + source.height)/height);
            else rlTexCoord2f((source.x + source.width)/width, (source.y + source.height)/height);
            rlVertex2f(bottomRight.x, bottomRight.y);

            // Top-right corner for texture and quad
            if (flipX) rlTexCoord2f(source.x/width, source.y/height);
            else rlTexCoord2f((source.x + source.width)/width, source.y/height);
            rlVertex2f(topRight.x, topRight.y);

        rlEnd();
        rlSetTexture(0);

        // NOTE: Vertex position can be transformed using matrices
        // but the process is way more costly than just calculating
        // the vertex positions manually, like done above.
        // I leave here the old implementation for educational pourposes,
        // just in case someone wants to do some performance test
        /*
        rlSetTexture(texture.id);
        rlPushMatrix();
            rlTranslatef(dest.x, dest.y, 0.0f);
            if (rotation != 0.0f) rlRotatef(rotation, 0.0f, 0.0f, 1.0f);
            rlTranslatef(-origin.x, -origin.y, 0.0f);

            rlBegin(RL_QUADS);
                rlColor4ub(tint.r, tint.g, tint.b, tint.a);
                rlNormal3f(0.0f, 0.0f, 1.0f);                          // Normal vector pointing towards viewer

                // Bottom-left corner for texture and quad
                if (flipX) rlTexCoord2f((source.x + source.width)/width, source.y/height);
                else rlTexCoord2f(source.x/width, source.y/height);
                rlVertex2f(0.0f, 0.0f);

                // Bottom-right corner for texture and quad
                if (flipX) rlTexCoord2f((source.x + source.width)/width, (source.y + source.height)/height);
                else rlTexCoord2f(source.x/width, (source.y + source.height)/height);
                rlVertex2f(0.0f, dest.height);

                // Top-right corner for texture and quad
                if (flipX) rlTexCoord2f(source.x/width, (source.y + source.height)/height);
                else rlTexCoord2f((source.x + source.width)/width, (source.y + source.height)/height);
                rlVertex2f(dest.width, dest.height);

                // Top-left corner for texture and quad
                if (flipX) rlTexCoord2f(source.x/width, source.y/height);
                else rlTexCoord2f((source.x + source.width)/width, source.y/height);
                rlVertex2f(dest.width, 0.0f);
            rlEnd();
        rlPopMatrix();
        rlSetTexture(0);
        */
    }
}

// Draws a texture (or part of it) that stretches or shrinks nicely using n-patch info
void DrawTextureNPatch(Texture2D texture, NPatchInfo nPatchInfo, Rectangle dest, Vector2 origin, float rotation, Color tint)
{
    if (texture.id > 0)
    {
        float width = (float)texture.width;
        float height = (float)texture.height;

        float patchWidth = ((int)dest.width <= 0)? 0.0f : dest.width;
        float patchHeight = ((int)dest.height <= 0)? 0.0f : dest.height;

        if (nPatchInfo.source.width < 0) nPatchInfo.source.x -= nPatchInfo.source.width;
        if (nPatchInfo.source.height < 0) nPatchInfo.source.y -= nPatchInfo.source.height;
        if (nPatchInfo.layout == NPATCH_THREE_PATCH_HORIZONTAL) patchHeight = nPatchInfo.source.height;
        if (nPatchInfo.layout == NPATCH_THREE_PATCH_VERTICAL) patchWidth = nPatchInfo.source.width;

        bool drawCenter = true;
        bool drawMiddle = true;
        float leftBorder = (float)nPatchInfo.left;
        float topBorder = (float)nPatchInfo.top;
        float rightBorder = (float)nPatchInfo.right;
        float bottomBorder = (float)nPatchInfo.bottom;

        // Adjust the lateral (left and right) border widths in case patchWidth < texture.width
        if (patchWidth <= (leftBorder + rightBorder) && nPatchInfo.layout != NPATCH_THREE_PATCH_VERTICAL)
        {
            drawCenter = false;
            leftBorder = (leftBorder/(leftBorder + rightBorder))*patchWidth;
            rightBorder = patchWidth - leftBorder;
        }

        // Adjust the lateral (top and bottom) border heights in case patchHeight < texture.height
        if (patchHeight <= (topBorder + bottomBorder) && nPatchInfo.layout != NPATCH_THREE_PATCH_HORIZONTAL)
        {
            drawMiddle = false;
            topBorder = (topBorder/(topBorder + bottomBorder))*patchHeight;
            bottomBorder = patchHeight - topBorder;
        }

        Vector2 vertA, vertB, vertC, vertD;
        vertA.x = 0.0f;                             // outer left
        vertA.y = 0.0f;                             // outer top
        vertB.x = leftBorder;                       // inner left
        vertB.y = topBorder;                        // inner top
        vertC.x = patchWidth  - rightBorder;        // inner right
        vertC.y = patchHeight - bottomBorder;       // inner bottom
        vertD.x = patchWidth;                       // outer right
        vertD.y = patchHeight;                      // outer bottom

        Vector2 coordA, coordB, coordC, coordD;
        coordA.x = nPatchInfo.source.x/width;
        coordA.y = nPatchInfo.source.y/height;
        coordB.x = (nPatchInfo.source.x + leftBorder)/width;
        coordB.y = (nPatchInfo.source.y + topBorder)/height;
        coordC.x = (nPatchInfo.source.x + nPatchInfo.source.width  - rightBorder)/width;
        coordC.y = (nPatchInfo.source.y + nPatchInfo.source.height - bottomBorder)/height;
        coordD.x = (nPatchInfo.source.x + nPatchInfo.source.width)/width;
        coordD.y = (nPatchInfo.source.y + nPatchInfo.source.height)/height;

        rlCheckRenderBatchLimit(9 * 3 * 2);         // Maxium number of verts that could happen

        rlSetTexture(texture.id);

        rlPushMatrix();
            rlTranslatef(dest.x, dest.y, 0.0f);
            rlRotatef(rotation, 0.0f, 0.0f, 1.0f);
            rlTranslatef(-origin.x, -origin.y, 0.0f);

            rlBegin(RL_QUADS);
                rlColor4ub(tint.r, tint.g, tint.b, tint.a);
                rlNormal3f(0.0f, 0.0f, 1.0f);               // Normal vector pointing towards viewer

                if (nPatchInfo.layout == NPATCH_NINE_PATCH)
                {
                    // ------------------------------------------------------------
                    // TOP-LEFT QUAD
                    rlTexCoord2f(coordA.x, coordB.y); rlVertex2f(vertA.x, vertB.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordB.x, coordB.y); rlVertex2f(vertB.x, vertB.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordB.x, coordA.y); rlVertex2f(vertB.x, vertA.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordA.x, coordA.y); rlVertex2f(vertA.x, vertA.y);  // Top-left corner for texture and quad
                    if (drawCenter)
                    {
                        // TOP-CENTER QUAD
                        rlTexCoord2f(coordB.x, coordB.y); rlVertex2f(vertB.x, vertB.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordC.x, coordB.y); rlVertex2f(vertC.x, vertB.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordC.x, coordA.y); rlVertex2f(vertC.x, vertA.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordB.x, coordA.y); rlVertex2f(vertB.x, vertA.y);  // Top-left corner for texture and quad
                    }
                    // TOP-RIGHT QUAD
                    rlTexCoord2f(coordC.x, coordB.y); rlVertex2f(vertC.x, vertB.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordD.x, coordB.y); rlVertex2f(vertD.x, vertB.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordD.x, coordA.y); rlVertex2f(vertD.x, vertA.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordC.x, coordA.y); rlVertex2f(vertC.x, vertA.y);  // Top-left corner for texture and quad
                    if (drawMiddle)
                    {
                        // ------------------------------------------------------------
                        // MIDDLE-LEFT QUAD
                        rlTexCoord2f(coordA.x, coordC.y); rlVertex2f(vertA.x, vertC.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordB.x, coordC.y); rlVertex2f(vertB.x, vertC.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordB.x, coordB.y); rlVertex2f(vertB.x, vertB.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordA.x, coordB.y); rlVertex2f(vertA.x, vertB.y);  // Top-left corner for texture and quad
                        if (drawCenter)
                        {
                            // MIDDLE-CENTER QUAD
                            rlTexCoord2f(coordB.x, coordC.y); rlVertex2f(vertB.x, vertC.y);  // Bottom-left corner for texture and quad
                            rlTexCoord2f(coordC.x, coordC.y); rlVertex2f(vertC.x, vertC.y);  // Bottom-right corner for texture and quad
                            rlTexCoord2f(coordC.x, coordB.y); rlVertex2f(vertC.x, vertB.y);  // Top-right corner for texture and quad
                            rlTexCoord2f(coordB.x, coordB.y); rlVertex2f(vertB.x, vertB.y);  // Top-left corner for texture and quad
                        }

                        // MIDDLE-RIGHT QUAD
                        rlTexCoord2f(coordC.x, coordC.y); rlVertex2f(vertC.x, vertC.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordD.x, coordC.y); rlVertex2f(vertD.x, vertC.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordD.x, coordB.y); rlVertex2f(vertD.x, vertB.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordC.x, coordB.y); rlVertex2f(vertC.x, vertB.y);  // Top-left corner for texture and quad
                    }

                    // ------------------------------------------------------------
                    // BOTTOM-LEFT QUAD
                    rlTexCoord2f(coordA.x, coordD.y); rlVertex2f(vertA.x, vertD.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordB.x, coordD.y); rlVertex2f(vertB.x, vertD.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordB.x, coordC.y); rlVertex2f(vertB.x, vertC.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordA.x, coordC.y); rlVertex2f(vertA.x, vertC.y);  // Top-left corner for texture and quad
                    if (drawCenter)
                    {
                        // BOTTOM-CENTER QUAD
                        rlTexCoord2f(coordB.x, coordD.y); rlVertex2f(vertB.x, vertD.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordC.x, coordD.y); rlVertex2f(vertC.x, vertD.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordC.x, coordC.y); rlVertex2f(vertC.x, vertC.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordB.x, coordC.y); rlVertex2f(vertB.x, vertC.y);  // Top-left corner for texture and quad
                    }

                    // BOTTOM-RIGHT QUAD
                    rlTexCoord2f(coordC.x, coordD.y); rlVertex2f(vertC.x, vertD.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordD.x, coordD.y); rlVertex2f(vertD.x, vertD.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordD.x, coordC.y); rlVertex2f(vertD.x, vertC.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordC.x, coordC.y); rlVertex2f(vertC.x, vertC.y);  // Top-left corner for texture and quad
                }
                else if (nPatchInfo.layout == NPATCH_THREE_PATCH_VERTICAL)
                {
                    // TOP QUAD
                    // -----------------------------------------------------------
                    // Texture coords                 Vertices
                    rlTexCoord2f(coordA.x, coordB.y); rlVertex2f(vertA.x, vertB.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordD.x, coordB.y); rlVertex2f(vertD.x, vertB.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordD.x, coordA.y); rlVertex2f(vertD.x, vertA.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordA.x, coordA.y); rlVertex2f(vertA.x, vertA.y);  // Top-left corner for texture and quad
                    if (drawCenter)
                    {
                        // MIDDLE QUAD
                        // -----------------------------------------------------------
                        // Texture coords                 Vertices
                        rlTexCoord2f(coordA.x, coordC.y); rlVertex2f(vertA.x, vertC.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordD.x, coordC.y); rlVertex2f(vertD.x, vertC.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordD.x, coordB.y); rlVertex2f(vertD.x, vertB.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordA.x, coordB.y); rlVertex2f(vertA.x, vertB.y);  // Top-left corner for texture and quad
                    }
                    // BOTTOM QUAD
                    // -----------------------------------------------------------
                    // Texture coords                 Vertices
                    rlTexCoord2f(coordA.x, coordD.y); rlVertex2f(vertA.x, vertD.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordD.x, coordD.y); rlVertex2f(vertD.x, vertD.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordD.x, coordC.y); rlVertex2f(vertD.x, vertC.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordA.x, coordC.y); rlVertex2f(vertA.x, vertC.y);  // Top-left corner for texture and quad
                }
                else if (nPatchInfo.layout == NPATCH_THREE_PATCH_HORIZONTAL)
                {
                    // LEFT QUAD
                    // -----------------------------------------------------------
                    // Texture coords                 Vertices
                    rlTexCoord2f(coordA.x, coordD.y); rlVertex2f(vertA.x, vertD.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordB.x, coordD.y); rlVertex2f(vertB.x, vertD.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordB.x, coordA.y); rlVertex2f(vertB.x, vertA.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordA.x, coordA.y); rlVertex2f(vertA.x, vertA.y);  // Top-left corner for texture and quad
                    if (drawCenter)
                    {
                        // CENTER QUAD
                        // -----------------------------------------------------------
                        // Texture coords                 Vertices
                        rlTexCoord2f(coordB.x, coordD.y); rlVertex2f(vertB.x, vertD.y);  // Bottom-left corner for texture and quad
                        rlTexCoord2f(coordC.x, coordD.y); rlVertex2f(vertC.x, vertD.y);  // Bottom-right corner for texture and quad
                        rlTexCoord2f(coordC.x, coordA.y); rlVertex2f(vertC.x, vertA.y);  // Top-right corner for texture and quad
                        rlTexCoord2f(coordB.x, coordA.y); rlVertex2f(vertB.x, vertA.y);  // Top-left corner for texture and quad
                    }
                    // RIGHT QUAD
                    // -----------------------------------------------------------
                    // Texture coords                 Vertices
                    rlTexCoord2f(coordC.x, coordD.y); rlVertex2f(vertC.x, vertD.y);  // Bottom-left corner for texture and quad
                    rlTexCoord2f(coordD.x, coordD.y); rlVertex2f(vertD.x, vertD.y);  // Bottom-right corner for texture and quad
                    rlTexCoord2f(coordD.x, coordA.y); rlVertex2f(vertD.x, vertA.y);  // Top-right corner for texture and quad
                    rlTexCoord2f(coordC.x, coordA.y); rlVertex2f(vertC.x, vertA.y);  // Top-left corner for texture and quad
                }
            rlEnd();
        rlPopMatrix();

        rlSetTexture(0);
    }
}

// Draw textured polygon, defined by vertex and texturecoordinates
// NOTE: Polygon center must have straight line path to all points
// without crossing perimeter, points must be in anticlockwise order
void DrawTexturePoly(Texture2D texture, Vector2 center, Vector2 *points, Vector2 *texcoords, int pointCount, Color tint)
{
    rlCheckRenderBatchLimit((pointCount - 1)*4);

    rlSetTexture(texture.id);

    // Texturing is only supported on QUADs
    rlBegin(RL_QUADS);

        rlColor4ub(tint.r, tint.g, tint.b, tint.a);

        for (int i = 0; i < pointCount - 1; i++)
        {
            rlTexCoord2f(0.5f, 0.5f);
            rlVertex2f(center.x, center.y);

            rlTexCoord2f(texcoords[i].x, texcoords[i].y);
            rlVertex2f(points[i].x + center.x, points[i].y + center.y);

            rlTexCoord2f(texcoords[i + 1].x, texcoords[i + 1].y);
            rlVertex2f(points[i + 1].x + center.x, points[i + 1].y + center.y);

            rlTexCoord2f(texcoords[i + 1].x, texcoords[i + 1].y);
            rlVertex2f(points[i + 1].x + center.x, points[i + 1].y + center.y);
        }
    rlEnd();

    rlSetTexture(0);
}

// Get color with alpha applied, alpha goes from 0.0f to 1.0f
Color Fade(Color color, float alpha)
{
    if (alpha < 0.0f) alpha = 0.0f;
    else if (alpha > 1.0f) alpha = 1.0f;

    return (Color){color.r, color.g, color.b, (unsigned char)(255.0f*alpha)};
}

// Get hexadecimal value for a Color
int ColorToInt(Color color)
{
    return (((int)color.r << 24) | ((int)color.g << 16) | ((int)color.b << 8) | (int)color.a);
}

// Get color normalized as float [0..1]
Vector4 ColorNormalize(Color color)
{
    Vector4 result;

    result.x = (float)color.r/255.0f;
    result.y = (float)color.g/255.0f;
    result.z = (float)color.b/255.0f;
    result.w = (float)color.a/255.0f;

    return result;
}

// Get color from normalized values [0..1]
Color ColorFromNormalized(Vector4 normalized)
{
    Color result;

    result.r = (unsigned char)(normalized.x*255.0f);
    result.g = (unsigned char)(normalized.y*255.0f);
    result.b = (unsigned char)(normalized.z*255.0f);
    result.a = (unsigned char)(normalized.w*255.0f);

    return result;
}

// Get HSV values for a Color
// NOTE: Hue is returned as degrees [0..360]
Vector3 ColorToHSV(Color color)
{
    Vector3 hsv = { 0 };
    Vector3 rgb = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };
    float min, max, delta;

    min = rgb.x < rgb.y? rgb.x : rgb.y;
    min = min  < rgb.z? min  : rgb.z;

    max = rgb.x > rgb.y? rgb.x : rgb.y;
    max = max  > rgb.z? max  : rgb.z;

    hsv.z = max;            // Value
    delta = max - min;

    if (delta < 0.00001f)
    {
        hsv.y = 0.0f;
        hsv.x = 0.0f;       // Undefined, maybe NAN?
        return hsv;
    }

    if (max > 0.0f)
    {
        // NOTE: If max is 0, this divide would cause a crash
        hsv.y = (delta/max);    // Saturation
    }
    else
    {
        // NOTE: If max is 0, then r = g = b = 0, s = 0, h is undefined
        hsv.y = 0.0f;
        hsv.x = NAN;        // Undefined
        return hsv;
    }

    // NOTE: Comparing float values could not work properly
    if (rgb.x >= max) hsv.x = (rgb.y - rgb.z)/delta;    // Between yellow & magenta
    else
    {
        if (rgb.y >= max) hsv.x = 2.0f + (rgb.z - rgb.x)/delta;  // Between cyan & yellow
        else hsv.x = 4.0f + (rgb.x - rgb.y)/delta;      // Between magenta & cyan
    }

    hsv.x *= 60.0f;     // Convert to degrees

    if (hsv.x < 0.0f) hsv.x += 360.0f;

    return hsv;
}

// Get a Color from HSV values
// Implementation reference: https://en.wikipedia.org/wiki/HSL_and_HSV#Alternative_HSV_conversion
// NOTE: Color->HSV->Color conversion will not yield exactly the same color due to rounding errors
// Hue is provided in degrees: [0..360]
// Saturation/Value are provided normalized: [0.0f..1.0f]
Color ColorFromHSV(float hue, float saturation, float value)
{
    Color color = { 0, 0, 0, 255 };

    // Red channel
    float k = fmodf((5.0f + hue/60.0f), 6);
    float t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.r = (unsigned char)((value - value*saturation*k)*255.0f);

    // Green channel
    k = fmodf((3.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.g = (unsigned char)((value - value*saturation*k)*255.0f);

    // Blue channel
    k = fmodf((1.0f + hue/60.0f), 6);
    t = 4.0f - k;
    k = (t < k)? t : k;
    k = (k < 1)? k : 1;
    k = (k > 0)? k : 0;
    color.b = (unsigned char)((value - value*saturation*k)*255.0f);

    return color;
}

// Get color with alpha applied, alpha goes from 0.0f to 1.0f
Color ColorAlpha(Color color, float alpha)
{
    if (alpha < 0.0f) alpha = 0.0f;
    else if (alpha > 1.0f) alpha = 1.0f;

    return (Color){color.r, color.g, color.b, (unsigned char)(255.0f*alpha)};
}

// Get src alpha-blended into dst color with tint
Color ColorAlphaBlend(Color dst, Color src, Color tint)
{
    Color out = WHITE;

    // Apply color tint to source color
    src.r = (unsigned char)(((unsigned int)src.r*(unsigned int)tint.r) >> 8);
    src.g = (unsigned char)(((unsigned int)src.g*(unsigned int)tint.g) >> 8);
    src.b = (unsigned char)(((unsigned int)src.b*(unsigned int)tint.b) >> 8);
    src.a = (unsigned char)(((unsigned int)src.a*(unsigned int)tint.a) >> 8);

//#define COLORALPHABLEND_FLOAT
#define COLORALPHABLEND_INTEGERS
#if defined(COLORALPHABLEND_INTEGERS)
    if (src.a == 0) out = dst;
    else if (src.a == 255) out = src;
    else
    {
        unsigned int alpha = (unsigned int)src.a + 1;     // We are shifting by 8 (dividing by 256), so we need to take that excess into account
        out.a = (unsigned char)(((unsigned int)alpha*256 + (unsigned int)dst.a*(256 - alpha)) >> 8);

        if (out.a > 0)
        {
            out.r = (unsigned char)((((unsigned int)src.r*alpha*256 + (unsigned int)dst.r*(unsigned int)dst.a*(256 - alpha))/out.a) >> 8);
            out.g = (unsigned char)((((unsigned int)src.g*alpha*256 + (unsigned int)dst.g*(unsigned int)dst.a*(256 - alpha))/out.a) >> 8);
            out.b = (unsigned char)((((unsigned int)src.b*alpha*256 + (unsigned int)dst.b*(unsigned int)dst.a*(256 - alpha))/out.a) >> 8);
        }
    }
#endif
#if defined(COLORALPHABLEND_FLOAT)
    if (src.a == 0) out = dst;
    else if (src.a == 255) out = src;
    else
    {
        Vector4 fdst = ColorNormalize(dst);
        Vector4 fsrc = ColorNormalize(src);
        Vector4 ftint = ColorNormalize(tint);
        Vector4 fout = { 0 };

        fout.w = fsrc.w + fdst.w*(1.0f - fsrc.w);

        if (fout.w > 0.0f)
        {
            fout.x = (fsrc.x*fsrc.w + fdst.x*fdst.w*(1 - fsrc.w))/fout.w;
            fout.y = (fsrc.y*fsrc.w + fdst.y*fdst.w*(1 - fsrc.w))/fout.w;
            fout.z = (fsrc.z*fsrc.w + fdst.z*fdst.w*(1 - fsrc.w))/fout.w;
        }

        out = (Color){ (unsigned char)(fout.x*255.0f), (unsigned char)(fout.y*255.0f), (unsigned char)(fout.z*255.0f), (unsigned char)(fout.w*255.0f) };
    }
#endif

    return out;
}

// Get a Color struct from hexadecimal value
Color GetColor(unsigned int hexValue)
{
    Color color;

    color.r = (unsigned char)(hexValue >> 24) & 0xFF;
    color.g = (unsigned char)(hexValue >> 16) & 0xFF;
    color.b = (unsigned char)(hexValue >> 8) & 0xFF;
    color.a = (unsigned char)hexValue & 0xFF;

    return color;
}

// Get color from a pixel from certain format
Color GetPixelColor(void *srcPtr, int format)
{
    Color color = { 0 };

    switch (format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: color = (Color){ ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[0], 255 }; break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: color = (Color){ ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[1] }; break;
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        {
            color.r = (unsigned char)((((unsigned short *)srcPtr)[0] >> 11)*255/31);
            color.g = (unsigned char)(((((unsigned short *)srcPtr)[0] >> 5) & 0b0000000000111111)*255/63);
            color.b = (unsigned char)((((unsigned short *)srcPtr)[0] & 0b0000000000011111)*255/31);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        {
            color.r = (unsigned char)((((unsigned short *)srcPtr)[0] >> 11)*255/31);
            color.g = (unsigned char)(((((unsigned short *)srcPtr)[0] >> 6) & 0b0000000000011111)*255/31);
            color.b = (unsigned char)((((unsigned short *)srcPtr)[0] & 0b0000000000011111)*255/31);
            color.a = (((unsigned short *)srcPtr)[0] & 0b0000000000000001)? 255 : 0;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
        {
            color.r = (unsigned char)((((unsigned short *)srcPtr)[0] >> 12)*255/15);
            color.g = (unsigned char)(((((unsigned short *)srcPtr)[0] >> 8) & 0b0000000000001111)*255/15);
            color.b = (unsigned char)(((((unsigned short *)srcPtr)[0] >> 4) & 0b0000000000001111)*255/15);
            color.a = (unsigned char)((((unsigned short *)srcPtr)[0] & 0b0000000000001111)*255/15);

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: color = (Color){ ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[1], ((unsigned char *)srcPtr)[2], ((unsigned char *)srcPtr)[3] }; break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8: color = (Color){ ((unsigned char *)srcPtr)[0], ((unsigned char *)srcPtr)[1], ((unsigned char *)srcPtr)[2], 255 }; break;
        case PIXELFORMAT_UNCOMPRESSED_R32:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = (unsigned char)(((float *)srcPtr)[0]*255.0f);
            color.g = (unsigned char)(((float *)srcPtr)[0]*255.0f);
            color.b = (unsigned char)(((float *)srcPtr)[0]*255.0f);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = (unsigned char)(((float *)srcPtr)[0]*255.0f);
            color.g = (unsigned char)(((float *)srcPtr)[1]*255.0f);
            color.b = (unsigned char)(((float *)srcPtr)[2]*255.0f);
            color.a = 255;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
        {
            // NOTE: Pixel normalized float value is converted to [0..255]
            color.r = (unsigned char)(((float *)srcPtr)[0]*255.0f);
            color.g = (unsigned char)(((float *)srcPtr)[1]*255.0f);
            color.b = (unsigned char)(((float *)srcPtr)[2]*255.0f);
            color.a = (unsigned char)(((float *)srcPtr)[3]*255.0f);

        } break;
        default: break;
    }

    return color;
}

// Set pixel color formatted into destination pointer
void SetPixelColor(void *dstPtr, Color color, int format)
{
    switch (format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
        {
            // NOTE: Calculate grayscale equivalent color
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };
            unsigned char gray = (unsigned char)((coln.x*0.299f + coln.y*0.587f + coln.z*0.114f)*255.0f);

            ((unsigned char *)dstPtr)[0] = gray;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
        {
            // NOTE: Calculate grayscale equivalent color
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };
            unsigned char gray = (unsigned char)((coln.x*0.299f + coln.y*0.587f + coln.z*0.114f)*255.0f);

            ((unsigned char *)dstPtr)[0] = gray;
            ((unsigned char *)dstPtr)[1] = color.a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        {
            // NOTE: Calculate R5G6B5 equivalent color
            Vector3 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f };

            unsigned char r = (unsigned char)(round(coln.x*31.0f));
            unsigned char g = (unsigned char)(round(coln.y*63.0f));
            unsigned char b = (unsigned char)(round(coln.z*31.0f));

            ((unsigned short *)dstPtr)[0] = (unsigned short)r << 11 | (unsigned short)g << 5 | (unsigned short)b;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        {
            // NOTE: Calculate R5G5B5A1 equivalent color
            Vector4 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f, (float)color.a/255.0f };

            unsigned char r = (unsigned char)(round(coln.x*31.0f));
            unsigned char g = (unsigned char)(round(coln.y*31.0f));
            unsigned char b = (unsigned char)(round(coln.z*31.0f));
            unsigned char a = (coln.w > ((float)PIXELFORMAT_UNCOMPRESSED_R5G5B5A1_ALPHA_THRESHOLD/255.0f))? 1 : 0;

            ((unsigned short *)dstPtr)[0] = (unsigned short)r << 11 | (unsigned short)g << 6 | (unsigned short)b << 1 | (unsigned short)a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
        {
            // NOTE: Calculate R5G5B5A1 equivalent color
            Vector4 coln = { (float)color.r/255.0f, (float)color.g/255.0f, (float)color.b/255.0f, (float)color.a/255.0f };

            unsigned char r = (unsigned char)(round(coln.x*15.0f));
            unsigned char g = (unsigned char)(round(coln.y*15.0f));
            unsigned char b = (unsigned char)(round(coln.z*15.0f));
            unsigned char a = (unsigned char)(round(coln.w*15.0f));

            ((unsigned short *)dstPtr)[0] = (unsigned short)r << 12 | (unsigned short)g << 8 | (unsigned short)b << 4 | (unsigned short)a;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
        {
            ((unsigned char *)dstPtr)[0] = color.r;
            ((unsigned char *)dstPtr)[1] = color.g;
            ((unsigned char *)dstPtr)[2] = color.b;

        } break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
        {
            ((unsigned char *)dstPtr)[0] = color.r;
            ((unsigned char *)dstPtr)[1] = color.g;
            ((unsigned char *)dstPtr)[2] = color.b;
            ((unsigned char *)dstPtr)[3] = color.a;

        } break;
        default: break;
    }
}

// Get pixel data size in bytes for certain format
// NOTE: Size can be requested for Image or Texture data
int GetPixelDataSize(int width, int height, int format)
{
    int dataSize = 0;       // Size in bytes
    int bpp = 0;            // Bits per pixel

    switch (format)
    {
        case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: bpp = 8; break;
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
        case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4: bpp = 16; break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: bpp = 32; break;
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8: bpp = 24; break;
        case PIXELFORMAT_UNCOMPRESSED_R32: bpp = 32; break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32: bpp = 32*3; break;
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32: bpp = 32*4; break;
        case PIXELFORMAT_COMPRESSED_DXT1_RGB:
        case PIXELFORMAT_COMPRESSED_DXT1_RGBA:
        case PIXELFORMAT_COMPRESSED_ETC1_RGB:
        case PIXELFORMAT_COMPRESSED_ETC2_RGB:
        case PIXELFORMAT_COMPRESSED_PVRT_RGB:
        case PIXELFORMAT_COMPRESSED_PVRT_RGBA: bpp = 4; break;
        case PIXELFORMAT_COMPRESSED_DXT3_RGBA:
        case PIXELFORMAT_COMPRESSED_DXT5_RGBA:
        case PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA:
        case PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA: bpp = 8; break;
        case PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA: bpp = 2; break;
        default: break;
    }

    dataSize = width*height*bpp/8;  // Total data size in bytes

    // Most compressed formats works on 4x4 blocks,
    // if texture is smaller, minimum dataSize is 8 or 16
    if ((width < 4) && (height < 4))
    {
        if ((format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) && (format < PIXELFORMAT_COMPRESSED_DXT3_RGBA)) dataSize = 8;
        else if ((format >= PIXELFORMAT_COMPRESSED_DXT3_RGBA) && (format < PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA)) dataSize = 16;
    }

    return dataSize;
}

//----------------------------------------------------------------------------------
// Module specific Functions Definition
//----------------------------------------------------------------------------------
#if defined(SUPPORT_FILEFORMAT_DDS)
// Loading DDS image data (compressed or uncompressed)
static Image LoadDDS(const unsigned char *fileData, unsigned int fileSize)
{
    unsigned char *fileDataPtr = (unsigned char *)fileData;

    // Required extension:
    // GL_EXT_texture_compression_s3tc

    // Supported tokens (defined by extensions)
    // GL_COMPRESSED_RGB_S3TC_DXT1_EXT      0x83F0
    // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT     0x83F1
    // GL_COMPRESSED_RGBA_S3TC_DXT3_EXT     0x83F2
    // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT     0x83F3

    #define FOURCC_DXT1 0x31545844  // Equivalent to "DXT1" in ASCII
    #define FOURCC_DXT3 0x33545844  // Equivalent to "DXT3" in ASCII
    #define FOURCC_DXT5 0x35545844  // Equivalent to "DXT5" in ASCII

    // DDS Pixel Format
    typedef struct {
        unsigned int size;
        unsigned int flags;
        unsigned int fourCC;
        unsigned int rgbBitCount;
        unsigned int rBitMask;
        unsigned int gBitMask;
        unsigned int bBitMask;
        unsigned int aBitMask;
    } DDSPixelFormat;

    // DDS Header (124 bytes)
    typedef struct {
        unsigned int size;
        unsigned int flags;
        unsigned int height;
        unsigned int width;
        unsigned int pitchOrLinearSize;
        unsigned int depth;
        unsigned int mipmapCount;
        unsigned int reserved1[11];
        DDSPixelFormat ddspf;
        unsigned int caps;
        unsigned int caps2;
        unsigned int caps3;
        unsigned int caps4;
        unsigned int reserved2;
    } DDSHeader;

    Image image = { 0 };

    if (fileDataPtr != NULL)
    {
        // Verify the type of file
        unsigned char *ddsHeaderId = fileDataPtr;
        fileDataPtr += 4;

        if ((ddsHeaderId[0] != 'D') || (ddsHeaderId[1] != 'D') || (ddsHeaderId[2] != 'S') || (ddsHeaderId[3] != ' '))
        {
            TRACELOG(LOG_WARNING, "IMAGE: DDS file data not valid");
        }
        else
        {
            DDSHeader *ddsHeader = (DDSHeader *)fileDataPtr;

            TRACELOGD("IMAGE: DDS file data info:");
            TRACELOGD("    > Header size:        %i", sizeof(DDSHeader));
            TRACELOGD("    > Pixel format size:  %i", ddsHeader->ddspf.size);
            TRACELOGD("    > Pixel format flags: 0x%x", ddsHeader->ddspf.flags);
            TRACELOGD("    > File format:        0x%x", ddsHeader->ddspf.fourCC);
            TRACELOGD("    > File bit count:     0x%x", ddsHeader->ddspf.rgbBitCount);

            fileDataPtr += sizeof(DDSHeader);   // Skip header

            image.width = ddsHeader->width;
            image.height = ddsHeader->height;

            if (ddsHeader->mipmapCount == 0) image.mipmaps = 1;      // Parameter not used
            else image.mipmaps = ddsHeader->mipmapCount;

            if (ddsHeader->ddspf.rgbBitCount == 16)     // 16bit mode, no compressed
            {
                if (ddsHeader->ddspf.flags == 0x40)         // no alpha channel
                {
                    int dataSize = image.width*image.height*sizeof(unsigned short);
                    image.data = (unsigned short *)RL_MALLOC(dataSize);

                    memcpy(image.data, fileDataPtr, dataSize);

                    image.format = PIXELFORMAT_UNCOMPRESSED_R5G6B5;
                }
                else if (ddsHeader->ddspf.flags == 0x41)        // with alpha channel
                {
                    if (ddsHeader->ddspf.aBitMask == 0x8000)    // 1bit alpha
                    {
                        int dataSize = image.width*image.height*sizeof(unsigned short);
                        image.data = (unsigned short *)RL_MALLOC(dataSize);

                        memcpy(image.data, fileDataPtr, dataSize);

                        unsigned char alpha = 0;

                        // NOTE: Data comes as A1R5G5B5, it must be reordered to R5G5B5A1
                        for (int i = 0; i < image.width*image.height; i++)
                        {
                            alpha = ((unsigned short *)image.data)[i] >> 15;
                            ((unsigned short *)image.data)[i] = ((unsigned short *)image.data)[i] << 1;
                            ((unsigned short *)image.data)[i] += alpha;
                        }

                        image.format = PIXELFORMAT_UNCOMPRESSED_R5G5B5A1;
                    }
                    else if (ddsHeader->ddspf.aBitMask == 0xf000)   // 4bit alpha
                    {
                        int dataSize = image.width*image.height*sizeof(unsigned short);
                        image.data = (unsigned short *)RL_MALLOC(dataSize);

                        memcpy(image.data, fileDataPtr, dataSize);

                        unsigned char alpha = 0;

                        // NOTE: Data comes as A4R4G4B4, it must be reordered R4G4B4A4
                        for (int i = 0; i < image.width*image.height; i++)
                        {
                            alpha = ((unsigned short *)image.data)[i] >> 12;
                            ((unsigned short *)image.data)[i] = ((unsigned short *)image.data)[i] << 4;
                            ((unsigned short *)image.data)[i] += alpha;
                        }

                        image.format = PIXELFORMAT_UNCOMPRESSED_R4G4B4A4;
                    }
                }
            }
            else if (ddsHeader->ddspf.flags == 0x40 && ddsHeader->ddspf.rgbBitCount == 24)   // DDS_RGB, no compressed
            {
                int dataSize = image.width*image.height*3*sizeof(unsigned char);
                image.data = (unsigned short *)RL_MALLOC(dataSize);

                memcpy(image.data, fileDataPtr, dataSize);

                image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
            }
            else if (ddsHeader->ddspf.flags == 0x41 && ddsHeader->ddspf.rgbBitCount == 32) // DDS_RGBA, no compressed
            {
                int dataSize = image.width*image.height*4*sizeof(unsigned char);
                image.data = (unsigned short *)RL_MALLOC(dataSize);

                memcpy(image.data, fileDataPtr, dataSize);

                unsigned char blue = 0;

                // NOTE: Data comes as A8R8G8B8, it must be reordered R8G8B8A8 (view next comment)
                // DirecX understand ARGB as a 32bit DWORD but the actual memory byte alignment is BGRA
                // So, we must realign B8G8R8A8 to R8G8B8A8
                for (int i = 0; i < image.width*image.height*4; i += 4)
                {
                    blue = ((unsigned char *)image.data)[i];
                    ((unsigned char *)image.data)[i] = ((unsigned char *)image.data)[i + 2];
                    ((unsigned char *)image.data)[i + 2] = blue;
                }

                image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            }
            else if (((ddsHeader->ddspf.flags == 0x04) || (ddsHeader->ddspf.flags == 0x05)) && (ddsHeader->ddspf.fourCC > 0)) // Compressed
            {
                int dataSize = 0;

                // Calculate data size, including all mipmaps
                if (ddsHeader->mipmapCount > 1) dataSize = ddsHeader->pitchOrLinearSize*2;
                else dataSize = ddsHeader->pitchOrLinearSize;

                image.data = (unsigned char *)RL_MALLOC(dataSize*sizeof(unsigned char));

                memcpy(image.data, fileDataPtr, dataSize);

                switch (ddsHeader->ddspf.fourCC)
                {
                    case FOURCC_DXT1:
                    {
                        if (ddsHeader->ddspf.flags == 0x04) image.format = PIXELFORMAT_COMPRESSED_DXT1_RGB;
                        else image.format = PIXELFORMAT_COMPRESSED_DXT1_RGBA;
                    } break;
                    case FOURCC_DXT3: image.format = PIXELFORMAT_COMPRESSED_DXT3_RGBA; break;
                    case FOURCC_DXT5: image.format = PIXELFORMAT_COMPRESSED_DXT5_RGBA; break;
                    default: break;
                }
            }
        }
    }

    return image;
}
#endif

#if defined(SUPPORT_FILEFORMAT_PKM)
// Loading PKM image data (ETC1/ETC2 compression)
// NOTE: KTX is the standard Khronos Group compression format (ETC1/ETC2, mipmaps)
// PKM is a much simpler file format used mainly to contain a single ETC1/ETC2 compressed image (no mipmaps)
static Image LoadPKM(const unsigned char *fileData, unsigned int fileSize)
{
    unsigned char *fileDataPtr = (unsigned char *)fileData;

    // Required extensions:
    // GL_OES_compressed_ETC1_RGB8_texture  (ETC1) (OpenGL ES 2.0)
    // GL_ARB_ES3_compatibility  (ETC2/EAC) (OpenGL ES 3.0)

    // Supported tokens (defined by extensions)
    // GL_ETC1_RGB8_OES                 0x8D64
    // GL_COMPRESSED_RGB8_ETC2          0x9274
    // GL_COMPRESSED_RGBA8_ETC2_EAC     0x9278

    // PKM file (ETC1) Header (16 bytes)
    typedef struct {
        char id[4];                 // "PKM "
        char version[2];            // "10" or "20"
        unsigned short format;      // Data format (big-endian) (Check list below)
        unsigned short width;       // Texture width (big-endian) (origWidth rounded to multiple of 4)
        unsigned short height;      // Texture height (big-endian) (origHeight rounded to multiple of 4)
        unsigned short origWidth;   // Original width (big-endian)
        unsigned short origHeight;  // Original height (big-endian)
    } PKMHeader;

    // Formats list
    // version 10: format: 0=ETC1_RGB, [1=ETC1_RGBA, 2=ETC1_RGB_MIP, 3=ETC1_RGBA_MIP] (not used)
    // version 20: format: 0=ETC1_RGB, 1=ETC2_RGB, 2=ETC2_RGBA_OLD, 3=ETC2_RGBA, 4=ETC2_RGBA1, 5=ETC2_R, 6=ETC2_RG, 7=ETC2_SIGNED_R, 8=ETC2_SIGNED_R

    // NOTE: The extended width and height are the widths rounded up to a multiple of 4.
    // NOTE: ETC is always 4bit per pixel (64 bit for each 4x4 block of pixels)

    Image image = { 0 };

    if (fileDataPtr != NULL)
    {
        PKMHeader *pkmHeader = (PKMHeader *)fileDataPtr;

        if ((pkmHeader->id[0] != 'P') || (pkmHeader->id[1] != 'K') || (pkmHeader->id[2] != 'M') || (pkmHeader->id[3] != ' '))
        {
            TRACELOG(LOG_WARNING, "IMAGE: PKM file data not valid");
        }
        else
        {
            fileDataPtr += sizeof(PKMHeader);   // Skip header

            // NOTE: format, width and height come as big-endian, data must be swapped to little-endian
            pkmHeader->format = ((pkmHeader->format & 0x00FF) << 8) | ((pkmHeader->format & 0xFF00) >> 8);
            pkmHeader->width = ((pkmHeader->width & 0x00FF) << 8) | ((pkmHeader->width & 0xFF00) >> 8);
            pkmHeader->height = ((pkmHeader->height & 0x00FF) << 8) | ((pkmHeader->height & 0xFF00) >> 8);

            TRACELOGD("IMAGE: PKM file data info:");
            TRACELOGD("    > Image width:  %i", pkmHeader->width);
            TRACELOGD("    > Image height: %i", pkmHeader->height);
            TRACELOGD("    > Image format: %i", pkmHeader->format);

            image.width = pkmHeader->width;
            image.height = pkmHeader->height;
            image.mipmaps = 1;

            int bpp = 4;
            if (pkmHeader->format == 3) bpp = 8;

            int dataSize = image.width*image.height*bpp/8;  // Total data size in bytes

            image.data = (unsigned char *)RL_MALLOC(dataSize*sizeof(unsigned char));

            memcpy(image.data, fileDataPtr, dataSize);

            if (pkmHeader->format == 0) image.format = PIXELFORMAT_COMPRESSED_ETC1_RGB;
            else if (pkmHeader->format == 1) image.format = PIXELFORMAT_COMPRESSED_ETC2_RGB;
            else if (pkmHeader->format == 3) image.format = PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA;
        }
    }

    return image;
}
#endif

#if defined(SUPPORT_FILEFORMAT_KTX)
// Load KTX compressed image data (ETC1/ETC2 compression)
// TODO: Review KTX loading, many things changed!
static Image LoadKTX(const unsigned char *fileData, unsigned int fileSize)
{
    unsigned char *fileDataPtr = (unsigned char *)fileData;

    // Required extensions:
    // GL_OES_compressed_ETC1_RGB8_texture  (ETC1)
    // GL_ARB_ES3_compatibility  (ETC2/EAC)

    // Supported tokens (defined by extensions)
    // GL_ETC1_RGB8_OES                 0x8D64
    // GL_COMPRESSED_RGB8_ETC2          0x9274
    // GL_COMPRESSED_RGBA8_ETC2_EAC     0x9278

    // KTX file Header (64 bytes)
    // v1.1 - https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/
    // v2.0 - http://github.khronos.org/KTX-Specification/

    // TODO: Support KTX 2.2 specs!

    typedef struct {
        char id[12];                        // Identifier: "«KTX 11»\r\n\x1A\n"
        unsigned int endianness;            // Little endian: 0x01 0x02 0x03 0x04
        unsigned int glType;                // For compressed textures, glType must equal 0
        unsigned int glTypeSize;            // For compressed texture data, usually 1
        unsigned int glFormat;              // For compressed textures is 0
        unsigned int glInternalFormat;      // Compressed internal format
        unsigned int glBaseInternalFormat;  // Same as glFormat (RGB, RGBA, ALPHA...)
        unsigned int width;                 // Texture image width in pixels
        unsigned int height;                // Texture image height in pixels
        unsigned int depth;                 // For 2D textures is 0
        unsigned int elements;              // Number of array elements, usually 0
        unsigned int faces;                 // Cubemap faces, for no-cubemap = 1
        unsigned int mipmapLevels;          // Non-mipmapped textures = 1
        unsigned int keyValueDataSize;      // Used to encode any arbitrary data...
    } KTXHeader;

    // NOTE: Before start of every mipmap data block, we have: unsigned int dataSize

    Image image = { 0 };

    if (fileDataPtr != NULL)
    {
        KTXHeader *ktxHeader = (KTXHeader *)fileDataPtr;

        if ((ktxHeader->id[1] != 'K') || (ktxHeader->id[2] != 'T') || (ktxHeader->id[3] != 'X') ||
            (ktxHeader->id[4] != ' ') || (ktxHeader->id[5] != '1') || (ktxHeader->id[6] != '1'))
        {
            TRACELOG(LOG_WARNING, "IMAGE: KTX file data not valid");
        }
        else
        {
            fileDataPtr += sizeof(KTXHeader);           // Move file data pointer

            image.width = ktxHeader->width;
            image.height = ktxHeader->height;
            image.mipmaps = ktxHeader->mipmapLevels;

            TRACELOGD("IMAGE: KTX file data info:");
            TRACELOGD("    > Image width:  %i", ktxHeader->width);
            TRACELOGD("    > Image height: %i", ktxHeader->height);
            TRACELOGD("    > Image format: 0x%x", ktxHeader->glInternalFormat);

            fileDataPtr += ktxHeader->keyValueDataSize; // Skip value data size

            int dataSize = ((int *)fileDataPtr)[0];
            fileDataPtr += sizeof(int);

            image.data = (unsigned char *)RL_MALLOC(dataSize*sizeof(unsigned char));

            memcpy(image.data, fileDataPtr, dataSize);

            if (ktxHeader->glInternalFormat == 0x8D64) image.format = PIXELFORMAT_COMPRESSED_ETC1_RGB;
            else if (ktxHeader->glInternalFormat == 0x9274) image.format = PIXELFORMAT_COMPRESSED_ETC2_RGB;
            else if (ktxHeader->glInternalFormat == 0x9278) image.format = PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA;

            // TODO: Support uncompressed data formats? Right now it returns format = 0!
        }
    }

    return image;
}

// Save image data as KTX file
// NOTE: By default KTX 1.1 spec is used, 2.0 is still on draft (01Oct2018)
// TODO: Review KTX saving, many things changed!
static int SaveKTX(Image image, const char *fileName)
{
    // KTX file Header (64 bytes)
    // v1.1 - https://www.khronos.org/opengles/sdk/tools/KTX/file_format_spec/
    // v2.0 - http://github.khronos.org/KTX-Specification/ - Final specs by 2021-04-18
    typedef struct {
        char id[12];                        // Identifier: "«KTX 11»\r\n\x1A\n"             // KTX 2.0: "«KTX 22»\r\n\x1A\n"
        unsigned int endianness;            // Little endian: 0x01 0x02 0x03 0x04
        unsigned int glType;                // For compressed textures, glType must equal 0
        unsigned int glTypeSize;            // For compressed texture data, usually 1
        unsigned int glFormat;              // For compressed textures is 0
        unsigned int glInternalFormat;      // Compressed internal format
        unsigned int glBaseInternalFormat;  // Same as glFormat (RGB, RGBA, ALPHA...)       // KTX 2.0: UInt32 vkFormat
        unsigned int width;                 // Texture image width in pixels
        unsigned int height;                // Texture image height in pixels
        unsigned int depth;                 // For 2D textures is 0
        unsigned int elements;              // Number of array elements, usually 0
        unsigned int faces;                 // Cubemap faces, for no-cubemap = 1
        unsigned int mipmapLevels;          // Non-mipmapped textures = 1
        unsigned int keyValueDataSize;      // Used to encode any arbitrary data...         // KTX 2.0: UInt32 levelOrder - ordering of the mipmap levels, usually 0
                                                                                            // KTX 2.0: UInt32 supercompressionScheme - 0 (None), 1 (Crunch CRN), 2 (Zlib DEFLATE)...
        // KTX 2.0 defines additional header elements...
    } KTXHeader;

    // Calculate file dataSize required
    int dataSize = sizeof(KTXHeader);

    for (int i = 0, width = image.width, height = image.height; i < image.mipmaps; i++)
    {
        dataSize += GetPixelDataSize(width, height, image.format);
        width /= 2; height /= 2;
    }

    unsigned char *fileData = RL_CALLOC(dataSize, 1);
    unsigned char *fileDataPtr = fileData;

    KTXHeader ktxHeader = { 0 };

    // KTX identifier (v1.1)
    //unsigned char id[12] = { '«', 'K', 'T', 'X', ' ', '1', '1', '»', '\r', '\n', '\x1A', '\n' };
    //unsigned char id[12] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

    const char ktxIdentifier[12] = { 0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, '\r', '\n', 0x1A, '\n' };

    // Get the image header
    memcpy(ktxHeader.id, ktxIdentifier, 12);  // KTX 1.1 signature
    ktxHeader.endianness = 0;
    ktxHeader.glType = 0;                     // Obtained from image.format
    ktxHeader.glTypeSize = 1;
    ktxHeader.glFormat = 0;                   // Obtained from image.format
    ktxHeader.glInternalFormat = 0;           // Obtained from image.format
    ktxHeader.glBaseInternalFormat = 0;
    ktxHeader.width = image.width;
    ktxHeader.height = image.height;
    ktxHeader.depth = 0;
    ktxHeader.elements = 0;
    ktxHeader.faces = 1;
    ktxHeader.mipmapLevels = image.mipmaps;   // If it was 0, it means mipmaps should be generated on loading (not for compressed formats)
    ktxHeader.keyValueDataSize = 0;           // No extra data after the header

    rlGetGlTextureFormats(image.format, &ktxHeader.glInternalFormat, &ktxHeader.glFormat, &ktxHeader.glType);   // rlgl module function
    ktxHeader.glBaseInternalFormat = ktxHeader.glFormat;    // KTX 1.1 only

    // NOTE: We can save into a .ktx all PixelFormats supported by raylib, including compressed formats like DXT, ETC or ASTC

    if (ktxHeader.glFormat == -1) TRACELOG(LOG_WARNING, "IMAGE: GL format not supported for KTX export (%i)", ktxHeader.glFormat);
    else
    {
        memcpy(fileDataPtr, &ktxHeader, sizeof(KTXHeader));
        fileDataPtr += sizeof(KTXHeader);

        int width = image.width;
        int height = image.height;
        int dataOffset = 0;

        // Save all mipmaps data
        for (int i = 0; i < image.mipmaps; i++)
        {
            unsigned int dataSize = GetPixelDataSize(width, height, image.format);

            memcpy(fileDataPtr, &dataSize, sizeof(unsigned int));
            memcpy(fileDataPtr + 4, (unsigned char *)image.data + dataOffset, dataSize);

            width /= 2;
            height /= 2;
            dataOffset += dataSize;
            fileDataPtr += (4 + dataSize);
        }
    }

    int success = SaveFileData(fileName, fileData, dataSize);

    RL_FREE(fileData);    // Free file data buffer

    // If all data has been written correctly to file, success = 1
    return success;
}
#endif

#if defined(SUPPORT_FILEFORMAT_PVR)
// Loading PVR image data (uncompressed or PVRT compression)
// NOTE: PVR v2 not supported, use PVR v3 instead
static Image LoadPVR(const unsigned char *fileData, unsigned int fileSize)
{
    unsigned char *fileDataPtr = (unsigned char *)fileData;

    // Required extension:
    // GL_IMG_texture_compression_pvrtc

    // Supported tokens (defined by extensions)
    // GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG       0x8C00
    // GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG      0x8C02

#if 0   // Not used...
    // PVR file v2 Header (52 bytes)
    typedef struct {
        unsigned int headerLength;
        unsigned int height;
        unsigned int width;
        unsigned int numMipmaps;
        unsigned int flags;
        unsigned int dataLength;
        unsigned int bpp;
        unsigned int bitmaskRed;
        unsigned int bitmaskGreen;
        unsigned int bitmaskBlue;
        unsigned int bitmaskAlpha;
        unsigned int pvrTag;
        unsigned int numSurfs;
    } PVRHeaderV2;
#endif

    // PVR file v3 Header (52 bytes)
    // NOTE: After it could be metadata (15 bytes?)
    typedef struct {
        char id[4];
        unsigned int flags;
        unsigned char channels[4];      // pixelFormat high part
        unsigned char channelDepth[4];  // pixelFormat low part
        unsigned int colourSpace;
        unsigned int channelType;
        unsigned int height;
        unsigned int width;
        unsigned int depth;
        unsigned int numSurfaces;
        unsigned int numFaces;
        unsigned int numMipmaps;
        unsigned int metaDataSize;
    } PVRHeaderV3;

#if 0   // Not used...
    // Metadata (usually 15 bytes)
    typedef struct {
        unsigned int devFOURCC;
        unsigned int key;
        unsigned int dataSize;      // Not used?
        unsigned char *data;        // Not used?
    } PVRMetadata;
#endif

    Image image = { 0 };

    if (fileDataPtr != NULL)
    {
        // Check PVR image version
        unsigned char pvrVersion = fileDataPtr[0];

        // Load different PVR data formats
        if (pvrVersion == 0x50)
        {
            PVRHeaderV3 *pvrHeader = (PVRHeaderV3 *)fileDataPtr;

            if ((pvrHeader->id[0] != 'P') || (pvrHeader->id[1] != 'V') || (pvrHeader->id[2] != 'R') || (pvrHeader->id[3] != 3))
            {
                TRACELOG(LOG_WARNING, "IMAGE: PVR file data not valid");
            }
            else
            {
                fileDataPtr += sizeof(PVRHeaderV3);   // Skip header

                image.width = pvrHeader->width;
                image.height = pvrHeader->height;
                image.mipmaps = pvrHeader->numMipmaps;

                // Check data format
                if (((pvrHeader->channels[0] == 'l') && (pvrHeader->channels[1] == 0)) && (pvrHeader->channelDepth[0] == 8)) image.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
                else if (((pvrHeader->channels[0] == 'l') && (pvrHeader->channels[1] == 'a')) && ((pvrHeader->channelDepth[0] == 8) && (pvrHeader->channelDepth[1] == 8))) image.format = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
                else if ((pvrHeader->channels[0] == 'r') && (pvrHeader->channels[1] == 'g') && (pvrHeader->channels[2] == 'b'))
                {
                    if (pvrHeader->channels[3] == 'a')
                    {
                        if ((pvrHeader->channelDepth[0] == 5) && (pvrHeader->channelDepth[1] == 5) && (pvrHeader->channelDepth[2] == 5) && (pvrHeader->channelDepth[3] == 1)) image.format = PIXELFORMAT_UNCOMPRESSED_R5G5B5A1;
                        else if ((pvrHeader->channelDepth[0] == 4) && (pvrHeader->channelDepth[1] == 4) && (pvrHeader->channelDepth[2] == 4) && (pvrHeader->channelDepth[3] == 4)) image.format = PIXELFORMAT_UNCOMPRESSED_R4G4B4A4;
                        else if ((pvrHeader->channelDepth[0] == 8) && (pvrHeader->channelDepth[1] == 8) && (pvrHeader->channelDepth[2] == 8) && (pvrHeader->channelDepth[3] == 8)) image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                    }
                    else if (pvrHeader->channels[3] == 0)
                    {
                        if ((pvrHeader->channelDepth[0] == 5) && (pvrHeader->channelDepth[1] == 6) && (pvrHeader->channelDepth[2] == 5)) image.format = PIXELFORMAT_UNCOMPRESSED_R5G6B5;
                        else if ((pvrHeader->channelDepth[0] == 8) && (pvrHeader->channelDepth[1] == 8) && (pvrHeader->channelDepth[2] == 8)) image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
                    }
                }
                else if (pvrHeader->channels[0] == 2) image.format = PIXELFORMAT_COMPRESSED_PVRT_RGB;
                else if (pvrHeader->channels[0] == 3) image.format = PIXELFORMAT_COMPRESSED_PVRT_RGBA;

                fileDataPtr += pvrHeader->metaDataSize;    // Skip meta data header

                // Calculate data size (depends on format)
                int bpp = 0;
                switch (image.format)
                {
                    case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: bpp = 8; break;
                    case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                    case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                    case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                    case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4: bpp = 16; break;
                    case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: bpp = 32; break;
                    case PIXELFORMAT_UNCOMPRESSED_R8G8B8: bpp = 24; break;
                    case PIXELFORMAT_COMPRESSED_PVRT_RGB:
                    case PIXELFORMAT_COMPRESSED_PVRT_RGBA: bpp = 4; break;
                    default: break;
                }

                int dataSize = image.width*image.height*bpp/8;  // Total data size in bytes
                image.data = (unsigned char *)RL_MALLOC(dataSize*sizeof(unsigned char));

                memcpy(image.data, fileDataPtr, dataSize);
            }
        }
        else if (pvrVersion == 52) TRACELOG(LOG_INFO, "IMAGE: PVRv2 format not supported, update your files to PVRv3");
    }

    return image;
}
#endif

#if defined(SUPPORT_FILEFORMAT_ASTC)
// Load ASTC compressed image data (ASTC compression)
static Image LoadASTC(const unsigned char *fileData, unsigned int fileSize)
{
    unsigned char *fileDataPtr = (unsigned char *)fileData;

    // Required extensions:
    // GL_KHR_texture_compression_astc_hdr
    // GL_KHR_texture_compression_astc_ldr

    // Supported tokens (defined by extensions)
    // GL_COMPRESSED_RGBA_ASTC_4x4_KHR      0x93b0
    // GL_COMPRESSED_RGBA_ASTC_8x8_KHR      0x93b7

    // ASTC file Header (16 bytes)
    typedef struct {
        unsigned char id[4];        // Signature: 0x13 0xAB 0xA1 0x5C
        unsigned char blockX;       // Block X dimensions
        unsigned char blockY;       // Block Y dimensions
        unsigned char blockZ;       // Block Z dimensions (1 for 2D images)
        unsigned char width[3];     // Image width in pixels (24bit value)
        unsigned char height[3];    // Image height in pixels (24bit value)
        unsigned char length[3];    // Image Z-size (1 for 2D images)
    } ASTCHeader;

    Image image = { 0 };

    if (fileDataPtr != NULL)
    {
        ASTCHeader *astcHeader = (ASTCHeader *)fileDataPtr;

        if ((astcHeader->id[3] != 0x5c) || (astcHeader->id[2] != 0xa1) || (astcHeader->id[1] != 0xab) || (astcHeader->id[0] != 0x13))
        {
            TRACELOG(LOG_WARNING, "IMAGE: ASTC file data not valid");
        }
        else
        {
            fileDataPtr += sizeof(ASTCHeader);   // Skip header

            // NOTE: Assuming Little Endian (could it be wrong?)
            image.width = 0x00000000 | ((int)astcHeader->width[2] << 16) | ((int)astcHeader->width[1] << 8) | ((int)astcHeader->width[0]);
            image.height = 0x00000000 | ((int)astcHeader->height[2] << 16) | ((int)astcHeader->height[1] << 8) | ((int)astcHeader->height[0]);

            TRACELOGD("IMAGE: ASTC file data info:");
            TRACELOGD("    > Image width:  %i", image.width);
            TRACELOGD("    > Image height: %i", image.height);
            TRACELOGD("    > Image blocks: %ix%i", astcHeader->blockX, astcHeader->blockY);

            image.mipmaps = 1;      // NOTE: ASTC format only contains one mipmap level

            // NOTE: Each block is always stored in 128bit so we can calculate the bpp
            int bpp = 128/(astcHeader->blockX*astcHeader->blockY);

            // NOTE: Currently we only support 2 blocks configurations: 4x4 and 8x8
            if ((bpp == 8) || (bpp == 2))
            {
                int dataSize = image.width*image.height*bpp/8;  // Data size in bytes

                image.data = (unsigned char *)RL_MALLOC(dataSize*sizeof(unsigned char));

                memcpy(image.data, fileDataPtr, dataSize);

                if (bpp == 8) image.format = PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA;
                else if (bpp == 2) image.format = PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA;
            }
            else TRACELOG(LOG_WARNING, "IMAGE: ASTC block size configuration not supported");
        }
    }

    return image;
}
#endif

// Get pixel data from image as Vector4 array (float normalized)
static Vector4 *LoadImageDataNormalized(Image image)
{
    Vector4 *pixels = (Vector4 *)RL_MALLOC(image.width*image.height*sizeof(Vector4));

    if (image.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB) TRACELOG(LOG_WARNING, "IMAGE: Pixel data retrieval not supported for compressed image formats");
    else
    {
        for (int i = 0, k = 0; i < image.width*image.height; i++)
        {
            switch (image.format)
            {
                case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
                {
                    pixels[i].x = (float)((unsigned char *)image.data)[i]/255.0f;
                    pixels[i].y = (float)((unsigned char *)image.data)[i]/255.0f;
                    pixels[i].z = (float)((unsigned char *)image.data)[i]/255.0f;
                    pixels[i].w = 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
                {
                    pixels[i].x = (float)((unsigned char *)image.data)[k]/255.0f;
                    pixels[i].y = (float)((unsigned char *)image.data)[k]/255.0f;
                    pixels[i].z = (float)((unsigned char *)image.data)[k]/255.0f;
                    pixels[i].w = (float)((unsigned char *)image.data)[k + 1]/255.0f;

                    k += 2;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].x = (float)((pixel & 0b1111100000000000) >> 11)*(1.0f/31);
                    pixels[i].y = (float)((pixel & 0b0000011111000000) >> 6)*(1.0f/31);
                    pixels[i].z = (float)((pixel & 0b0000000000111110) >> 1)*(1.0f/31);
                    pixels[i].w = ((pixel & 0b0000000000000001) == 0)? 0.0f : 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R5G6B5:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].x = (float)((pixel & 0b1111100000000000) >> 11)*(1.0f/31);
                    pixels[i].y = (float)((pixel & 0b0000011111100000) >> 5)*(1.0f/63);
                    pixels[i].z = (float)(pixel & 0b0000000000011111)*(1.0f/31);
                    pixels[i].w = 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
                {
                    unsigned short pixel = ((unsigned short *)image.data)[i];

                    pixels[i].x = (float)((pixel & 0b1111000000000000) >> 12)*(1.0f/15);
                    pixels[i].y = (float)((pixel & 0b0000111100000000) >> 8)*(1.0f/15);
                    pixels[i].z = (float)((pixel & 0b0000000011110000) >> 4)*(1.0f/15);
                    pixels[i].w = (float)(pixel & 0b0000000000001111)*(1.0f/15);

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
                {
                    pixels[i].x = (float)((unsigned char *)image.data)[k]/255.0f;
                    pixels[i].y = (float)((unsigned char *)image.data)[k + 1]/255.0f;
                    pixels[i].z = (float)((unsigned char *)image.data)[k + 2]/255.0f;
                    pixels[i].w = (float)((unsigned char *)image.data)[k + 3]/255.0f;

                    k += 4;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
                {
                    pixels[i].x = (float)((unsigned char *)image.data)[k]/255.0f;
                    pixels[i].y = (float)((unsigned char *)image.data)[k + 1]/255.0f;
                    pixels[i].z = (float)((unsigned char *)image.data)[k + 2]/255.0f;
                    pixels[i].w = 1.0f;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32:
                {
                    pixels[i].x = ((float *)image.data)[k];
                    pixels[i].y = 0.0f;
                    pixels[i].z = 0.0f;
                    pixels[i].w = 1.0f;

                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
                {
                    pixels[i].x = ((float *)image.data)[k];
                    pixels[i].y = ((float *)image.data)[k + 1];
                    pixels[i].z = ((float *)image.data)[k + 2];
                    pixels[i].w = 1.0f;

                    k += 3;
                } break;
                case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
                {
                    pixels[i].x = ((float *)image.data)[k];
                    pixels[i].y = ((float *)image.data)[k + 1];
                    pixels[i].z = ((float *)image.data)[k + 2];
                    pixels[i].w = ((float *)image.data)[k + 3];

                    k += 4;
                }
                default: break;
            }
        }
    }

    return pixels;
}



//*****************************************************************************************************************************************************************************************


#if defined(_WIN32)
// To avoid conflicting windows.h symbols with raylib, some flags are defined
// WARNING: Those flags avoid inclusion of some Win32 headers that could be required
// by user at some point and won't be included...
//-------------------------------------------------------------------------------------

// If defined, the following flags inhibit definition of the indicated items.
#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES // VK_*
#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS      // SM_*
#define NOMENUS           // MF_*
#define NOICONS           // IDI_*
#define NOKEYSTATES       // MK_*
#define NOSYSCOMMANDS     // SC_*
#define NORASTEROPS       // Binary and Tertiary raster ops
#define NOSHOWWINDOW      // SW_*
#define OEMRESOURCE       // OEM Resource values
#define NOATOM            // Atom Manager routines
#define NOCLIPBOARD       // Clipboard routines
#define NOCOLOR           // Screen colors
#define NOCTLMGR          // Control and Dialog routines
#define NODRAWTEXT        // DrawText() and DT_*
#define NOGDI             // All GDI defines and routines
#define NOKERNEL          // All KERNEL defines and routines
#define NOUSER            // All USER defines and routines
//#define NONLS             // All NLS defines and routines
#define NOMB              // MB_* and MessageBox()
#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        // typedef METAFILEPICT
#define NOMINMAX          // Macros min(a,b) and max(a,b)
#define NOMSG             // typedef MSG and associated routines
#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL          // SB_* and scrolling routines
#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           // Sound driver routines
#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
#define NOWH              // SetWindowsHook and WH_*
#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
#define NOCOMM            // COMM driver routines
#define NOKANJI           // Kanji support stuff.
#define NOHELP            // Help engine interface.
#define NOPROFILER        // Profiler interface.
#define NODEFERWINDOWPOS  // DeferWindowPos routines
#define NOMCX             // Modem Configuration Extensions

// Type required before windows.h inclusion
typedef struct tagMSG *LPMSG;

#include <windows.h>        // Windows functionality (miniaudio)

// Type required by some unused function...
typedef struct tagBITMAPINFOHEADER {
  DWORD biSize;
  LONG  biWidth;
  LONG  biHeight;
  WORD  biPlanes;
  WORD  biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG  biXPelsPerMeter;
  LONG  biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

#include <objbase.h>        // Component Object Model (COM) header
#include <mmreg.h>          // Windows Multimedia, defines some WAVE structs
#include <mmsystem.h>       // Windows Multimedia, used by Windows GDI, defines DIBINDEX macro

// Some required types defined for MSVC/TinyC compiler
#if defined(_MSC_VER) || defined(__TINYC__)
    #include "propidl.h"
#endif
#endif

#define MA_MALLOC RL_MALLOC
#define MA_FREE RL_FREE

#define MA_NO_JACK
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_MP3
#define MINIAUDIO_IMPLEMENTATION
//#define MA_DEBUG_OUTPUT
#include "external/miniaudio.h"         // Audio device initialization and management
#undef PlaySound                        // Win32 API: windows.h > mmsystem.h defines PlaySound macro

#include <stdlib.h>                     // Required for: malloc(), free()
#include <stdio.h>                      // Required for: FILE, fopen(), fclose(), fread()
#include <string.h>                     // Required for: strcmp() [Used in IsFileExtension(), LoadWaveFromMemory(), LoadMusicStreamFromMemory()]



#if defined(SUPPORT_FILEFORMAT_OGG)
    // TODO: Remap stb_vorbis malloc()/free() calls to RL_MALLOC/RL_FREE
    #define STB_VORBIS_IMPLEMENTATION
    #include "external/stb_vorbis.h"    // OGG loading functions
#endif

#if defined(SUPPORT_FILEFORMAT_XM)
    #define JARXM_MALLOC RL_MALLOC
    #define JARXM_FREE RL_FREE
    #define JAR_XM_IMPLEMENTATION
    #include "external/jar_xm.h"        // XM loading functions
#endif

#if defined(SUPPORT_FILEFORMAT_MOD)
    #define JARMOD_MALLOC RL_MALLOC
    #define JARMOD_FREE RL_FREE

    #define JAR_MOD_IMPLEMENTATION
    #include "external/jar_mod.h"       // MOD loading functions
#endif

#if defined(SUPPORT_FILEFORMAT_WAV)
    #define DRWAV_MALLOC RL_MALLOC
    #define DRWAV_REALLOC RL_REALLOC
    #define DRWAV_FREE RL_FREE

    #define DR_WAV_IMPLEMENTATION
    #include "external/dr_wav.h"        // WAV loading functions
#endif

#if defined(SUPPORT_FILEFORMAT_MP3)
    #define DRMP3_MALLOC RL_MALLOC
    #define DRMP3_REALLOC RL_REALLOC
    #define DRMP3_FREE RL_FREE

    #define DR_MP3_IMPLEMENTATION
    #include "external/dr_mp3.h"        // MP3 loading functions
#endif

#if defined(SUPPORT_FILEFORMAT_FLAC)
    #define DRFLAC_MALLOC RL_MALLOC
    #define DRFLAC_REALLOC RL_REALLOC
    #define DRFLAC_FREE RL_FREE
    #define DR_FLAC_IMPLEMENTATION
    #define DR_FLAC_NO_WIN32_IO
    #include "external/dr_flac.h"       // FLAC loading functions
#endif

#if defined(_MSC_VER)
    #undef bool
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef AUDIO_DEVICE_FORMAT
    #define AUDIO_DEVICE_FORMAT    ma_format_f32    // Device output format (float-32bit)
#endif
#ifndef AUDIO_DEVICE_CHANNELS
    #define AUDIO_DEVICE_CHANNELS              2    // Device output channels: stereo
#endif
#ifndef AUDIO_DEVICE_SAMPLE_RATE
    #define AUDIO_DEVICE_SAMPLE_RATE           0    // Device output sample rate
#endif

#ifndef MAX_AUDIO_BUFFER_POOL_CHANNELS
    #define MAX_AUDIO_BUFFER_POOL_CHANNELS    16    // Audio pool channels
#endif
#ifndef DEFAULT_AUDIO_BUFFER_SIZE
    #define DEFAULT_AUDIO_BUFFER_SIZE       4096    // Default audio buffer size
#endif


//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Music context type
// NOTE: Depends on data structure provided by the library
// in charge of reading the different file types
typedef enum {
    MUSIC_AUDIO_NONE = 0,   // No audio context loaded
    MUSIC_AUDIO_WAV,        // WAV audio context
    MUSIC_AUDIO_OGG,        // OGG audio context
    MUSIC_AUDIO_FLAC,       // FLAC audio context
    MUSIC_AUDIO_MP3,        // MP3 audio context
    MUSIC_MODULE_XM,        // XM module audio context
    MUSIC_MODULE_MOD        // MOD module audio context
} MusicContextType;


// NOTE: Different logic is used when feeding data to the playback device
// depending on whether or not data is streamed (Music vs Sound)
typedef enum {
    AUDIO_BUFFER_USAGE_STATIC = 0,
    AUDIO_BUFFER_USAGE_STREAM
} AudioBufferUsage;

// Audio buffer struct
struct rAudioBuffer {
    ma_data_converter converter;    // Audio data converter

    AudioCallback callback;   // Audio buffer callback for buffer filling on audio threads
    rAudioProcessor *processor;     // Audio processor

    float volume;                   // Audio buffer volume
    float pitch;                    // Audio buffer pitch
    float pan;                      // Audio buffer pan (0.0f to 1.0f)

    bool playing;                   // Audio buffer state: AUDIO_PLAYING
    bool paused;                    // Audio buffer state: AUDIO_PAUSED
    bool looping;                   // Audio buffer looping, always true for AudioStreams
    int usage;                      // Audio buffer usage mode: STATIC or STREAM

    bool isSubBufferProcessed[2];   // SubBuffer processed (virtual double buffer)
    unsigned int sizeInFrames;      // Total buffer size in frames
    unsigned int frameCursorPos;    // Frame cursor position
    unsigned int framesProcessed;   // Total frames processed in this buffer (required for play timing)

    unsigned char *data;            // Data buffer, on music stream keeps filling

    rAudioBuffer *next;             // Next audio buffer on the list
    rAudioBuffer *prev;             // Previous audio buffer on the list
};

// Audio processor struct
// NOTE: Useful to apply effects to an AudioBuffer
struct rAudioProcessor {
    AudioCallback process;          // Processor callback function
    rAudioProcessor *next;          // Next audio processor on the list
    rAudioProcessor *prev;          // Previous audio processor on the list
};

#define AudioBuffer rAudioBuffer    // HACK: To avoid CoreAudio (macOS) symbol collision

// Audio data context
typedef struct AudioData {
    struct {
        ma_context context;         // miniaudio context data
        ma_device device;           // miniaudio device
        ma_mutex lock;              // miniaudio mutex lock
        bool isReady;               // Check if audio device is ready
    } System;
    struct {
        AudioBuffer *first;         // Pointer to first AudioBuffer in the list
        AudioBuffer *last;          // Pointer to last AudioBuffer in the list
        int defaultSize;            // Default audio buffer size for audio streams
    } Buffer;
    struct {
        unsigned int poolCounter;                               // AudioBuffer pointers pool counter
        AudioBuffer *pool[MAX_AUDIO_BUFFER_POOL_CHANNELS];      // Multichannel AudioBuffer pointers pool
        unsigned int channels[MAX_AUDIO_BUFFER_POOL_CHANNELS];  // AudioBuffer pool channels
    } MultiChannel;
} AudioData;

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static AudioData AUDIO = {          // Global AUDIO context

    // NOTE: Music buffer size is defined by number of samples, independent of sample size and channels number
    // After some math, considering a sampleRate of 48000, a buffer refill rate of 1/60 seconds and a
    // standard double-buffering system, a 4096 samples buffer has been chosen, it should be enough
    // In case of music-stalls, just increase this number
    .Buffer.defaultSize = 0
};

//----------------------------------------------------------------------------------
// Module specific Functions Declaration
//----------------------------------------------------------------------------------
static void OnLog(void *pUserData, ma_uint32 level, const char *pMessage);
static void OnSendAudioDataToDevice(ma_device *pDevice, void *pFramesOut, const void *pFramesInput, ma_uint32 frameCount);
static void MixAudioFrames(float *framesOut, const float *framesIn, ma_uint32 frameCount, AudioBuffer *buffer);



//----------------------------------------------------------------------------------
// AudioBuffer management functions declaration
// NOTE: Those functions are not exposed by raylib... for the moment
//----------------------------------------------------------------------------------
AudioBuffer *LoadAudioBuffer(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 sizeInFrames, int usage);
void UnloadAudioBuffer(AudioBuffer *buffer);

bool IsAudioBufferPlaying(AudioBuffer *buffer);
void PlayAudioBuffer(AudioBuffer *buffer);
void StopAudioBuffer(AudioBuffer *buffer);
void PauseAudioBuffer(AudioBuffer *buffer);
void ResumeAudioBuffer(AudioBuffer *buffer);
void SetAudioBufferVolume(AudioBuffer *buffer, float volume);
void SetAudioBufferPitch(AudioBuffer *buffer, float pitch);
void SetAudioBufferPan(AudioBuffer *buffer, float pan);
void TrackAudioBuffer(AudioBuffer *buffer);
void UntrackAudioBuffer(AudioBuffer *buffer);

//----------------------------------------------------------------------------------
// Module Functions Definition - Audio Device initialization and Closing
//----------------------------------------------------------------------------------
// Initialize audio device
void InitAudioDevice(void)
{
    // Init audio context
    ma_context_config ctxConfig = ma_context_config_init();
    ma_log_callback_init(OnLog, NULL);

    ma_result result = ma_context_init(NULL, 0, &ctxConfig, &AUDIO.System.context);
    if (result != MA_SUCCESS)
    {
        TRACELOG(LOG_WARNING, "AUDIO: Failed to initialize context");
        return;
    }

    // Init audio device
    // NOTE: Using the default device. Format is floating point because it simplifies mixing.
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.pDeviceID = NULL;  // NULL for the default playback AUDIO.System.device.
    config.playback.format = AUDIO_DEVICE_FORMAT;
    config.playback.channels = AUDIO_DEVICE_CHANNELS;
    config.capture.pDeviceID = NULL;  // NULL for the default capture AUDIO.System.device.
    config.capture.format = ma_format_s16;
    config.capture.channels = 1;
    config.sampleRate = AUDIO_DEVICE_SAMPLE_RATE;
    config.dataCallback = OnSendAudioDataToDevice;
    config.pUserData = NULL;

    result = ma_device_init(&AUDIO.System.context, &config, &AUDIO.System.device);
    if (result != MA_SUCCESS)
    {
        TRACELOG(LOG_WARNING, "AUDIO: Failed to initialize playback device");
        ma_context_uninit(&AUDIO.System.context);
        return;
    }

    // Keep the device running the whole time. May want to consider doing something a bit smarter and only have the device running
    // while there's at least one sound being played.
    result = ma_device_start(&AUDIO.System.device);
    if (result != MA_SUCCESS)
    {
        TRACELOG(LOG_WARNING, "AUDIO: Failed to start playback device");
        ma_device_uninit(&AUDIO.System.device);
        ma_context_uninit(&AUDIO.System.context);
        return;
    }

    // Mixing happens on a seperate thread which means we need to synchronize. I'm using a mutex here to make things simple, but may
    // want to look at something a bit smarter later on to keep everything real-time, if that's necessary.
    if (ma_mutex_init(&AUDIO.System.lock) != MA_SUCCESS)
    {
        TRACELOG(LOG_WARNING, "AUDIO: Failed to create mutex for mixing");
        ma_device_uninit(&AUDIO.System.device);
        ma_context_uninit(&AUDIO.System.context);
        return;
    }

    // Init dummy audio buffers pool for multichannel sound playing
    for (int i = 0; i < MAX_AUDIO_BUFFER_POOL_CHANNELS; i++)
    {
        // WARNING: An empty audio buffer is created (data = 0) and added to list, AudioBuffer data is filled on PlaySoundMulti()
        AUDIO.MultiChannel.pool[i] = LoadAudioBuffer(AUDIO_DEVICE_FORMAT, AUDIO_DEVICE_CHANNELS, AUDIO.System.device.sampleRate, 0, AUDIO_BUFFER_USAGE_STATIC);
    }

    TRACELOG(LOG_INFO, "AUDIO: Device initialized successfully");
    TRACELOG(LOG_INFO, "    > Backend:       miniaudio / %s", ma_get_backend_name(AUDIO.System.context.backend));
    TRACELOG(LOG_INFO, "    > Format:        %s -> %s", ma_get_format_name(AUDIO.System.device.playback.format), ma_get_format_name(AUDIO.System.device.playback.internalFormat));
    TRACELOG(LOG_INFO, "    > Channels:      %d -> %d", AUDIO.System.device.playback.channels, AUDIO.System.device.playback.internalChannels);
    TRACELOG(LOG_INFO, "    > Sample rate:   %d -> %d", AUDIO.System.device.sampleRate, AUDIO.System.device.playback.internalSampleRate);
    TRACELOG(LOG_INFO, "    > Periods size:  %d", AUDIO.System.device.playback.internalPeriodSizeInFrames*AUDIO.System.device.playback.internalPeriods);

    AUDIO.System.isReady = true;
}

// Close the audio device for all contexts
void CloseAudioDevice(void)
{
    if (AUDIO.System.isReady)
    {
        // Unload dummy audio buffers pool
        // WARNING: They can be pointing to already unloaded data
        for (int i = 0; i < MAX_AUDIO_BUFFER_POOL_CHANNELS; i++)
        {
            //UnloadAudioBuffer(AUDIO.MultiChannel.pool[i]);
            if (AUDIO.MultiChannel.pool[i] != NULL)
            {
                ma_data_converter_uninit(&AUDIO.MultiChannel.pool[i]->converter, NULL);
                UntrackAudioBuffer(AUDIO.MultiChannel.pool[i]);
                //RL_FREE(buffer->data);    // Already unloaded by UnloadSound()
                RL_FREE(AUDIO.MultiChannel.pool[i]);
            }
        }

        ma_mutex_uninit(&AUDIO.System.lock);
        ma_device_uninit(&AUDIO.System.device);
        ma_context_uninit(&AUDIO.System.context);

        AUDIO.System.isReady = false;

        TRACELOG(LOG_INFO, "AUDIO: Device closed successfully");
    }
    else TRACELOG(LOG_WARNING, "AUDIO: Device could not be closed, not currently initialized");
}

// Check if device has been initialized successfully
bool IsAudioDeviceReady(void)
{
    return AUDIO.System.isReady;
}

// Set master volume (listener)
void SetMasterVolume(float volume)
{
    ma_device_set_master_volume(&AUDIO.System.device, volume);
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Audio Buffer management
//----------------------------------------------------------------------------------

// Initialize a new audio buffer (filled with silence)
AudioBuffer *LoadAudioBuffer(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 sizeInFrames, int usage)
{
    AudioBuffer *audioBuffer = (AudioBuffer *)RL_CALLOC(1, sizeof(AudioBuffer));

    if (audioBuffer == NULL)
    {
        TRACELOG(LOG_WARNING, "AUDIO: Failed to allocate memory for buffer");
        return NULL;
    }

    if (sizeInFrames > 0) audioBuffer->data = RL_CALLOC(sizeInFrames*channels*ma_get_bytes_per_sample(format), 1);

    // Audio data runs through a format converter
    ma_data_converter_config converterConfig = ma_data_converter_config_init(format, AUDIO_DEVICE_FORMAT, channels, AUDIO_DEVICE_CHANNELS, sampleRate, AUDIO.System.device.sampleRate);
    converterConfig.allowDynamicSampleRate = true;

    ma_result result = ma_data_converter_init(&converterConfig, NULL, &audioBuffer->converter);

    if (result != MA_SUCCESS)
    {
        TRACELOG(LOG_WARNING, "AUDIO: Failed to create data conversion pipeline");
        RL_FREE(audioBuffer);
        return NULL;
    }

    // Init audio buffer values
    audioBuffer->volume = 1.0f;
    audioBuffer->pitch = 1.0f;
    audioBuffer->pan = 0.5f;

    audioBuffer->callback = NULL;
    audioBuffer->processor = NULL;

    audioBuffer->playing = false;
    audioBuffer->paused = false;
    audioBuffer->looping = false;

    audioBuffer->usage = usage;
    audioBuffer->frameCursorPos = 0;
    audioBuffer->sizeInFrames = sizeInFrames;

    // Buffers should be marked as processed by default so that a call to
    // UpdateAudioStream() immediately after initialization works correctly
    audioBuffer->isSubBufferProcessed[0] = true;
    audioBuffer->isSubBufferProcessed[1] = true;

    // Track audio buffer to linked list next position
    TrackAudioBuffer(audioBuffer);

    return audioBuffer;
}

// Delete an audio buffer
void UnloadAudioBuffer(AudioBuffer *buffer)
{
    if (buffer != NULL)
    {
        ma_data_converter_uninit(&buffer->converter, NULL);
        UntrackAudioBuffer(buffer);
        RL_FREE(buffer->data);
        RL_FREE(buffer);
    }
}

// Check if an audio buffer is playing
bool IsAudioBufferPlaying(AudioBuffer *buffer)
{
    bool result = false;

    if (buffer != NULL) result = (buffer->playing && !buffer->paused);

    return result;
}

// Play an audio buffer
// NOTE: Buffer is restarted to the start.
// Use PauseAudioBuffer() and ResumeAudioBuffer() if the playback position should be maintained.
void PlayAudioBuffer(AudioBuffer *buffer)
{
    if (buffer != NULL)
    {
        buffer->playing = true;
        buffer->paused = false;
        buffer->frameCursorPos = 0;
    }
}

// Stop an audio buffer
void StopAudioBuffer(AudioBuffer *buffer)
{
    if (buffer != NULL)
    {
        if (IsAudioBufferPlaying(buffer))
        {
            buffer->playing = false;
            buffer->paused = false;
            buffer->frameCursorPos = 0;
            buffer->framesProcessed = 0;
            buffer->isSubBufferProcessed[0] = true;
            buffer->isSubBufferProcessed[1] = true;
        }
    }
}

// Pause an audio buffer
void PauseAudioBuffer(AudioBuffer *buffer)
{
    if (buffer != NULL) buffer->paused = true;
}

// Resume an audio buffer
void ResumeAudioBuffer(AudioBuffer *buffer)
{
    if (buffer != NULL) buffer->paused = false;
}

// Set volume for an audio buffer
void SetAudioBufferVolume(AudioBuffer *buffer, float volume)
{
    if (buffer != NULL) buffer->volume = volume;
}

// Set pitch for an audio buffer
void SetAudioBufferPitch(AudioBuffer *buffer, float pitch)
{
    if ((buffer != NULL) && (pitch > 0.0f))
    {
        // Pitching is just an adjustment of the sample rate.
        // Note that this changes the duration of the sound:
        //  - higher pitches will make the sound faster
        //  - lower pitches make it slower
        ma_uint32 outputSampleRate = (ma_uint32)((float)buffer->converter.sampleRateOut/pitch);
        ma_data_converter_set_rate(&buffer->converter, buffer->converter.sampleRateIn, outputSampleRate);

        buffer->pitch = pitch;
    }
}

// Set pan for an audio buffer
void SetAudioBufferPan(AudioBuffer *buffer, float pan)
{
    if (pan < 0.0f) pan = 0.0f;
    else if (pan > 1.0f) pan = 1.0f;

    if (buffer != NULL) buffer->pan = pan;
}

// Track audio buffer to linked list next position
void TrackAudioBuffer(AudioBuffer *buffer)
{
    ma_mutex_lock(&AUDIO.System.lock);
    {
        if (AUDIO.Buffer.first == NULL) AUDIO.Buffer.first = buffer;
        else
        {
            AUDIO.Buffer.last->next = buffer;
            buffer->prev = AUDIO.Buffer.last;
        }

        AUDIO.Buffer.last = buffer;
    }
    ma_mutex_unlock(&AUDIO.System.lock);
}

// Untrack audio buffer from linked list
void UntrackAudioBuffer(AudioBuffer *buffer)
{
    ma_mutex_lock(&AUDIO.System.lock);
    {
        if (buffer->prev == NULL) AUDIO.Buffer.first = buffer->next;
        else buffer->prev->next = buffer->next;

        if (buffer->next == NULL) AUDIO.Buffer.last = buffer->prev;
        else buffer->next->prev = buffer->prev;

        buffer->prev = NULL;
        buffer->next = NULL;
    }
    ma_mutex_unlock(&AUDIO.System.lock);
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Sounds loading and playing (.WAV)
//----------------------------------------------------------------------------------

// Load wave data from file
Wave LoadWave(const char *fileName)
{
    Wave wave = { 0 };

    // Loading file to memory
    unsigned int fileSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &fileSize);

    // Loading wave from memory data
    if (fileData != NULL) wave = LoadWaveFromMemory(GetFileExtension(fileName), fileData, fileSize);

    RL_FREE(fileData);

    return wave;
}

// Load wave from memory buffer, fileType refers to extension: i.e. ".wav"
// WARNING: File extension must be provided in lower-case
Wave LoadWaveFromMemory(const char *fileType, const unsigned char *fileData, int dataSize)
{
    Wave wave = { 0 };

    if (false) { }
#if defined(SUPPORT_FILEFORMAT_WAV)
    else if (strcmp(fileType, ".wav") == 0)
    {
        drwav wav = { 0 };
        bool success = drwav_init_memory(&wav, fileData, dataSize, NULL);

        if (success)
        {
            wave.frameCount = (unsigned int)wav.totalPCMFrameCount;
            wave.sampleRate = wav.sampleRate;
            wave.sampleSize = 16;
            wave.channels = wav.channels;
            wave.data = (short *)RL_MALLOC(wave.frameCount*wave.channels*sizeof(short));

            // NOTE: We are forcing conversion to 16bit sample size on reading
            drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, wave.data);
        }
        else TRACELOG(LOG_WARNING, "WAVE: Failed to load WAV data");

        drwav_uninit(&wav);
    }
#endif
#if defined(SUPPORT_FILEFORMAT_OGG)
    else if (strcmp(fileType, ".ogg") == 0)
    {
        stb_vorbis *oggData = stb_vorbis_open_memory((unsigned char *)fileData, dataSize, NULL, NULL);

        if (oggData != NULL)
        {
            stb_vorbis_info info = stb_vorbis_get_info(oggData);

            wave.sampleRate = info.sample_rate;
            wave.sampleSize = 16;       // By default, ogg data is 16 bit per sample (short)
            wave.channels = info.channels;
            wave.frameCount = (unsigned int)stb_vorbis_stream_length_in_samples(oggData);  // NOTE: It returns frames!
            wave.data = (short *)RL_MALLOC(wave.frameCount*wave.channels*sizeof(short));

            // NOTE: Get the number of samples to process (be careful! we ask for number of shorts, not bytes!)
            stb_vorbis_get_samples_short_interleaved(oggData, info.channels, (short *)wave.data, wave.frameCount*wave.channels);
            stb_vorbis_close(oggData);
        }
        else TRACELOG(LOG_WARNING, "WAVE: Failed to load OGG data");
    }
#endif
#if defined(SUPPORT_FILEFORMAT_FLAC)
    else if (strcmp(fileType, ".flac") == 0)
    {
        unsigned long long int totalFrameCount = 0;

        // NOTE: We are forcing conversion to 16bit sample size on reading
        wave.data = drflac_open_memory_and_read_pcm_frames_s16(fileData, dataSize, &wave.channels, &wave.sampleRate, &totalFrameCount, NULL);
        wave.sampleSize = 16;

        if (wave.data != NULL) wave.frameCount = (unsigned int)totalFrameCount;
        else TRACELOG(LOG_WARNING, "WAVE: Failed to load FLAC data");
    }
#endif
#if defined(SUPPORT_FILEFORMAT_MP3)
    else if (strcmp(fileType, ".mp3") == 0)
    {
        drmp3_config config = { 0 };
        unsigned long long int totalFrameCount = 0;

        // NOTE: We are forcing conversion to 32bit float sample size on reading
        wave.data = drmp3_open_memory_and_read_pcm_frames_f32(fileData, dataSize, &config, &totalFrameCount, NULL);
        wave.sampleSize = 32;

        if (wave.data != NULL)
        {
            wave.channels = config.channels;
            wave.sampleRate = config.sampleRate;
            wave.frameCount = (int)totalFrameCount;
        }
        else TRACELOG(LOG_WARNING, "WAVE: Failed to load MP3 data");

    }
#endif
    else TRACELOG(LOG_WARNING, "WAVE: Data format not supported");

    TRACELOG(LOG_INFO, "WAVE: Data loaded successfully (%i Hz, %i bit, %i channels)", wave.sampleRate, wave.sampleSize, wave.channels);

    return wave;
}

// Load sound from file
// NOTE: The entire file is loaded to memory to be played (no-streaming)
Sound LoadSound(const char *fileName)
{
    Wave wave = LoadWave(fileName);

    Sound sound = LoadSoundFromWave(wave);

    UnloadWave(wave);       // Sound is loaded, we can unload wave

    return sound;
}

// Load sound from wave data
// NOTE: Wave data must be unallocated manually
Sound LoadSoundFromWave(Wave wave)
{
    Sound sound = { 0 };

    if (wave.data != NULL)
    {
        // When using miniaudio we need to do our own mixing.
        // To simplify this we need convert the format of each sound to be consistent with
        // the format used to open the playback AUDIO.System.device. We can do this two ways:
        //
        //   1) Convert the whole sound in one go at load time (here).
        //   2) Convert the audio data in chunks at mixing time.
        //
        // First option has been selected, format conversion is done on the loading stage.
        // The downside is that it uses more memory if the original sound is u8 or s16.
        ma_format formatIn = ((wave.sampleSize == 8)? ma_format_u8 : ((wave.sampleSize == 16)? ma_format_s16 : ma_format_f32));
        ma_uint32 frameCountIn = wave.frameCount;

        ma_uint32 frameCount = (ma_uint32)ma_convert_frames(NULL, 0, AUDIO_DEVICE_FORMAT, AUDIO_DEVICE_CHANNELS, AUDIO.System.device.sampleRate, NULL, frameCountIn, formatIn, wave.channels, wave.sampleRate);
        if (frameCount == 0) TRACELOG(LOG_WARNING, "SOUND: Failed to get frame count for format conversion");

        AudioBuffer *audioBuffer = LoadAudioBuffer(AUDIO_DEVICE_FORMAT, AUDIO_DEVICE_CHANNELS, AUDIO.System.device.sampleRate, frameCount, AUDIO_BUFFER_USAGE_STATIC);
        if (audioBuffer == NULL)
        {
            TRACELOG(LOG_WARNING, "SOUND: Failed to create buffer");
            return sound; // early return to avoid dereferencing the audioBuffer null pointer
        }

        frameCount = (ma_uint32)ma_convert_frames(audioBuffer->data, frameCount, AUDIO_DEVICE_FORMAT, AUDIO_DEVICE_CHANNELS, AUDIO.System.device.sampleRate, wave.data, frameCountIn, formatIn, wave.channels, wave.sampleRate);
        if (frameCount == 0) TRACELOG(LOG_WARNING, "SOUND: Failed format conversion");

        sound.frameCount = frameCount;
        sound.stream.sampleRate = AUDIO.System.device.sampleRate;
        sound.stream.sampleSize = 32;
        sound.stream.channels = AUDIO_DEVICE_CHANNELS;
        sound.stream.buffer = audioBuffer;
    }

    return sound;
}

// Unload wave data
void UnloadWave(Wave wave)
{
    RL_FREE(wave.data);
    //TRACELOG(LOG_INFO, "WAVE: Unloaded wave data from RAM");
}

// Unload sound
void UnloadSound(Sound sound)
{
    UnloadAudioBuffer(sound.stream.buffer);
    //TRACELOG(LOG_INFO, "SOUND: Unloaded sound data from RAM");
}

// Update sound buffer with new data
void UpdateSound(Sound sound, const void *data, int sampleCount)
{
    if (sound.stream.buffer != NULL)
    {
        StopAudioBuffer(sound.stream.buffer);

        // TODO: May want to lock/unlock this since this data buffer is read at mixing time
        memcpy(sound.stream.buffer->data, data, sampleCount*ma_get_bytes_per_frame(sound.stream.buffer->converter.formatIn, sound.stream.buffer->converter.channelsIn));
    }
}

// Export wave data to file
bool ExportWave(Wave wave, const char *fileName)
{
    bool success = false;

    if (false) { }
#if defined(SUPPORT_FILEFORMAT_WAV)
    else if (IsFileExtension(fileName, ".wav"))
    {
        drwav wav = { 0 };
        drwav_data_format format = { 0 };
        format.container = drwav_container_riff;
        if (wave.sampleSize == 32) format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
        else format.format = DR_WAVE_FORMAT_PCM;
        format.channels = wave.channels;
        format.sampleRate = wave.sampleRate;
        format.bitsPerSample = wave.sampleSize;

        void *fileData = NULL;
        size_t fileDataSize = 0;
        success = drwav_init_memory_write(&wav, &fileData, &fileDataSize, &format, NULL);
        if (success) success = (int)drwav_write_pcm_frames(&wav, wave.frameCount, wave.data);
        drwav_result result = drwav_uninit(&wav);

        if (result == DRWAV_SUCCESS) success = SaveFileData(fileName, (unsigned char *)fileData, (unsigned int)fileDataSize);

        drwav_free(fileData, NULL);
    }
#endif
    else if (IsFileExtension(fileName, ".raw"))
    {
        // Export raw sample data (without header)
        // NOTE: It's up to the user to track wave parameters
        success = SaveFileData(fileName, wave.data, wave.frameCount*wave.channels*wave.sampleSize/8);
    }

    if (success) TRACELOG(LOG_INFO, "FILEIO: [%s] Wave data exported successfully", fileName);
    else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to export wave data", fileName);

    return success;
}

// Export wave sample data to code (.h)
bool ExportWaveAsCode(Wave wave, const char *fileName)
{
    bool success = false;

#ifndef TEXT_BYTES_PER_LINE
    #define TEXT_BYTES_PER_LINE     20
#endif

    int waveDataSize = wave.frameCount*wave.channels*wave.sampleSize/8;

    // NOTE: Text data buffer size is estimated considering wave data size in bytes
    // and requiring 6 char bytes for every byte: "0x00, "
    char *txtData = (char *)RL_CALLOC(waveDataSize*6 + 2000, sizeof(char));

    int byteCount = 0;
    byteCount += sprintf(txtData + byteCount, "\n//////////////////////////////////////////////////////////////////////////////////\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                              //\n");
    byteCount += sprintf(txtData + byteCount, "// WaveAsCode exporter v1.1 - Wave data exported as an array of bytes           //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                              //\n");
    byteCount += sprintf(txtData + byteCount, "// more info and bugs-report:  github.com/raysan5/raylib                        //\n");
    byteCount += sprintf(txtData + byteCount, "// feedback and support:       ray[at]raylib.com                                //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                              //\n");
    byteCount += sprintf(txtData + byteCount, "// Copyright (c) 2018-2022 Ramon Santamaria (@raysan5)                          //\n");
    byteCount += sprintf(txtData + byteCount, "//                                                                              //\n");
    byteCount += sprintf(txtData + byteCount, "//////////////////////////////////////////////////////////////////////////////////\n\n");

    char fileNameLower[256] = { 0 };
    char fileNameUpper[256] = { 0 };
    for (int i = 0; fileName[i] != '.'; i++) { fileNameLower[i] = fileName[i]; }      // Get filename without extension
    for (int i = 0; fileNameLower[i] != '\0'; i++) if (fileNameLower[i] >= 'a' && fileNameLower[i] <= 'z') { fileNameUpper[i] = fileNameLower[i] - 32; }

    byteCount += sprintf(txtData + byteCount, "// Wave data information\n");
    byteCount += sprintf(txtData + byteCount, "#define %s_FRAME_COUNT      %u\n", fileNameUpper, wave.frameCount);
    byteCount += sprintf(txtData + byteCount, "#define %s_SAMPLE_RATE      %u\n", fileNameUpper, wave.sampleRate);
    byteCount += sprintf(txtData + byteCount, "#define %s_SAMPLE_SIZE      %u\n", fileNameUpper, wave.sampleSize);
    byteCount += sprintf(txtData + byteCount, "#define %s_CHANNELS         %u\n\n", fileNameUpper, wave.channels);

    // Write wave data as an array of values
    // Wave data is exported as byte array for 8/16bit and float array for 32bit float data
    // NOTE: Frame data exported is channel-interlaced: frame01[sampleChannel1, sampleChannel2, ...], frame02[], frame03[]
    if (wave.sampleSize == 32)
    {
        byteCount += sprintf(txtData + byteCount, "static float %sData[%i] = {\n", fileNameLower, waveDataSize/4);
        for (int i = 1; i < waveDataSize/4; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "%.4ff,\n    " : "%.4ff, "), ((float *)wave.data)[i - 1]);
        byteCount += sprintf(txtData + byteCount, "%.4ff };\n", ((float *)wave.data)[waveDataSize/4 - 1]);
    }
    else
    {
        byteCount += sprintf(txtData + byteCount, "static unsigned char %sData[%i] = { ", fileNameLower, waveDataSize);
        for (int i = 1; i < waveDataSize; i++) byteCount += sprintf(txtData + byteCount, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%x,\n    " : "0x%x, "), ((unsigned char *)wave.data)[i - 1]);
        byteCount += sprintf(txtData + byteCount, "0x%x };\n", ((unsigned char *)wave.data)[waveDataSize - 1]);
    }

    // NOTE: Text data length exported is determined by '\0' (NULL) character
    success = SaveFileText(fileName, txtData);

    RL_FREE(txtData);

    if (success != 0) TRACELOG(LOG_INFO, "FILEIO: [%s] Wave as code exported successfully", fileName);
    else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to export wave as code", fileName);

    return success;
}

// Play a sound
void PlaySound(Sound sound)
{
    PlayAudioBuffer(sound.stream.buffer);
}

// Play a sound in the multichannel buffer pool
void PlaySoundMulti(Sound sound)
{
    int index = -1;
    unsigned int oldAge = 0;
    int oldIndex = -1;

    // find the first non playing pool entry
    for (int i = 0; i < MAX_AUDIO_BUFFER_POOL_CHANNELS; i++)
    {
        if (AUDIO.MultiChannel.channels[i] > oldAge)
        {
            oldAge = AUDIO.MultiChannel.channels[i];
            oldIndex = i;
        }

        if (!IsAudioBufferPlaying(AUDIO.MultiChannel.pool[i]))
        {
            index = i;
            break;
        }
    }

    // If no none playing pool members can be index choose the oldest
    if (index == -1)
    {
        TRACELOG(LOG_WARNING, "SOUND: Buffer pool is already full, count: %i", AUDIO.MultiChannel.poolCounter);

        if (oldIndex == -1)
        {
            // Shouldn't be able to get here... but just in case something odd happens!
            TRACELOG(LOG_WARNING, "SOUND: Buffer pool could not determine oldest buffer not playing sound");
            return;
        }

        index = oldIndex;

        // Just in case...
        StopAudioBuffer(AUDIO.MultiChannel.pool[index]);
    }

    // Experimentally mutex lock doesn't seem to be needed this makes sense
    // as pool[index] isn't playing and the only stuff we're copying
    // shouldn't be changing...

    AUDIO.MultiChannel.channels[index] = AUDIO.MultiChannel.poolCounter;
    AUDIO.MultiChannel.poolCounter++;

    SetAudioBufferVolume(AUDIO.MultiChannel.pool[index], sound.stream.buffer->volume);
    SetAudioBufferPitch(AUDIO.MultiChannel.pool[index], sound.stream.buffer->pitch);
    SetAudioBufferPan(AUDIO.MultiChannel.pool[index], sound.stream.buffer->pan);

    AUDIO.MultiChannel.pool[index]->looping = sound.stream.buffer->looping;
    AUDIO.MultiChannel.pool[index]->usage = sound.stream.buffer->usage;
    AUDIO.MultiChannel.pool[index]->isSubBufferProcessed[0] = false;
    AUDIO.MultiChannel.pool[index]->isSubBufferProcessed[1] = false;
    AUDIO.MultiChannel.pool[index]->sizeInFrames = sound.stream.buffer->sizeInFrames;

    AUDIO.MultiChannel.pool[index]->data = sound.stream.buffer->data;       // Fill dummy track with data for playing

    PlayAudioBuffer(AUDIO.MultiChannel.pool[index]);
}

// Stop any sound played with PlaySoundMulti()
void StopSoundMulti(void)
{
    for (int i = 0; i < MAX_AUDIO_BUFFER_POOL_CHANNELS; i++) StopAudioBuffer(AUDIO.MultiChannel.pool[i]);
}

// Get number of sounds playing in the multichannel buffer pool
int GetSoundsPlaying(void)
{
    int counter = 0;

    for (int i = 0; i < MAX_AUDIO_BUFFER_POOL_CHANNELS; i++)
    {
        if (IsAudioBufferPlaying(AUDIO.MultiChannel.pool[i])) counter++;
    }

    return counter;
}

// Pause a sound
void PauseSound(Sound sound)
{
    PauseAudioBuffer(sound.stream.buffer);
}

// Resume a paused sound
void ResumeSound(Sound sound)
{
    ResumeAudioBuffer(sound.stream.buffer);
}

// Stop reproducing a sound
void StopSound(Sound sound)
{
    StopAudioBuffer(sound.stream.buffer);
}

// Check if a sound is playing
bool IsSoundPlaying(Sound sound)
{
    return IsAudioBufferPlaying(sound.stream.buffer);
}

// Set volume for a sound
void SetSoundVolume(Sound sound, float volume)
{
    SetAudioBufferVolume(sound.stream.buffer, volume);
}

// Set pitch for a sound
void SetSoundPitch(Sound sound, float pitch)
{
    SetAudioBufferPitch(sound.stream.buffer, pitch);
}

// Set pan for a sound
void SetSoundPan(Sound sound, float pan)
{
    SetAudioBufferPan(sound.stream.buffer, pan);
}

// Convert wave data to desired format
void WaveFormat(Wave *wave, int sampleRate, int sampleSize, int channels)
{
    ma_format formatIn = ((wave->sampleSize == 8)? ma_format_u8 : ((wave->sampleSize == 16)? ma_format_s16 : ma_format_f32));
    ma_format formatOut = ((sampleSize == 8)? ma_format_u8 : ((sampleSize == 16)? ma_format_s16 : ma_format_f32));

    ma_uint32 frameCountIn = wave->frameCount;

    ma_uint32 frameCount = (ma_uint32)ma_convert_frames(NULL, 0, formatOut, channels, sampleRate, NULL, frameCountIn, formatIn, wave->channels, wave->sampleRate);
    if (frameCount == 0)
    {
        TRACELOG(LOG_WARNING, "WAVE: Failed to get frame count for format conversion");
        return;
    }

    void *data = RL_MALLOC(frameCount*channels*(sampleSize/8));

    frameCount = (ma_uint32)ma_convert_frames(data, frameCount, formatOut, channels, sampleRate, wave->data, frameCountIn, formatIn, wave->channels, wave->sampleRate);
    if (frameCount == 0)
    {
        TRACELOG(LOG_WARNING, "WAVE: Failed format conversion");
        return;
    }

    wave->frameCount = frameCount;
    wave->sampleSize = sampleSize;
    wave->sampleRate = sampleRate;
    wave->channels = channels;
    RL_FREE(wave->data);
    wave->data = data;
}

// Copy a wave to a new wave
Wave WaveCopy(Wave wave)
{
    Wave newWave = { 0 };

    newWave.data = RL_MALLOC(wave.frameCount*wave.channels*wave.sampleSize/8);

    if (newWave.data != NULL)
    {
        // NOTE: Size must be provided in bytes
        memcpy(newWave.data, wave.data, wave.frameCount*wave.channels*wave.sampleSize/8);

        newWave.frameCount = wave.frameCount;
        newWave.sampleRate = wave.sampleRate;
        newWave.sampleSize = wave.sampleSize;
        newWave.channels = wave.channels;
    }

    return newWave;
}

// Crop a wave to defined samples range
// NOTE: Security check in case of out-of-range
void WaveCrop(Wave *wave, int initSample, int finalSample)
{
    if ((initSample >= 0) && (initSample < finalSample) &&
        (finalSample > 0) && ((unsigned int)finalSample < (wave->frameCount*wave->channels)))
    {
        int sampleCount = finalSample - initSample;

        void *data = RL_MALLOC(sampleCount*wave->sampleSize/8);

        memcpy(data, (unsigned char *)wave->data + (initSample*wave->channels*wave->sampleSize/8), sampleCount*wave->sampleSize/8);

        RL_FREE(wave->data);
        wave->data = data;
    }
    else TRACELOG(LOG_WARNING, "WAVE: Crop range out of bounds");
}

// Load samples data from wave as a floats array
// NOTE 1: Returned sample values are normalized to range [-1..1]
// NOTE 2: Sample data allocated should be freed with UnloadWaveSamples()
float *LoadWaveSamples(Wave wave)
{
    float *samples = (float *)RL_MALLOC(wave.frameCount*wave.channels*sizeof(float));

    // NOTE: sampleCount is the total number of interlaced samples (including channels)

    for (unsigned int i = 0; i < wave.frameCount*wave.channels; i++)
    {
        if (wave.sampleSize == 8) samples[i] = (float)(((unsigned char *)wave.data)[i] - 127)/256.0f;
        else if (wave.sampleSize == 16) samples[i] = (float)(((short *)wave.data)[i])/32767.0f;
        else if (wave.sampleSize == 32) samples[i] = ((float *)wave.data)[i];
    }

    return samples;
}

// Unload samples data loaded with LoadWaveSamples()
void UnloadWaveSamples(float *samples)
{
    RL_FREE(samples);
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Music loading and stream playing (.OGG)
//----------------------------------------------------------------------------------

// Load music stream from file
Music LoadMusicStream(const char *fileName)
{
    Music music = { 0 };
    bool musicLoaded = false;

    if (false) { }
#if defined(SUPPORT_FILEFORMAT_WAV)
    else if (IsFileExtension(fileName, ".wav"))
    {
        drwav *ctxWav = RL_CALLOC(1, sizeof(drwav));
        bool success = drwav_init_file(ctxWav, fileName, NULL);

        music.ctxType = MUSIC_AUDIO_WAV;
        music.ctxData = ctxWav;

        if (success)
        {
            int sampleSize = ctxWav->bitsPerSample;
            if (ctxWav->bitsPerSample == 24) sampleSize = 16;   // Forcing conversion to s16 on UpdateMusicStream()

            music.stream = LoadAudioStream(ctxWav->sampleRate, sampleSize, ctxWav->channels);
            music.frameCount = (unsigned int)ctxWav->totalPCMFrameCount;
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_OGG)
    else if (IsFileExtension(fileName, ".ogg"))
    {
        // Open ogg audio stream
        music.ctxType = MUSIC_AUDIO_OGG;
        music.ctxData = stb_vorbis_open_filename(fileName, NULL, NULL);

        if (music.ctxData != NULL)
        {
            stb_vorbis_info info = stb_vorbis_get_info((stb_vorbis *)music.ctxData);  // Get Ogg file info

            // OGG bit rate defaults to 16 bit, it's enough for compressed format
            music.stream = LoadAudioStream(info.sample_rate, 16, info.channels);

            // WARNING: It seems this function returns length in frames, not samples, so we multiply by channels
            music.frameCount = (unsigned int)stb_vorbis_stream_length_in_samples((stb_vorbis *)music.ctxData);
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_FLAC)
    else if (IsFileExtension(fileName, ".flac"))
    {
        music.ctxType = MUSIC_AUDIO_FLAC;
        music.ctxData = drflac_open_file(fileName, NULL);

        if (music.ctxData != NULL)
        {
            drflac *ctxFlac = (drflac *)music.ctxData;

            music.stream = LoadAudioStream(ctxFlac->sampleRate, ctxFlac->bitsPerSample, ctxFlac->channels);
            music.frameCount = (unsigned int)ctxFlac->totalPCMFrameCount;
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_MP3)
    else if (IsFileExtension(fileName, ".mp3"))
    {
        drmp3 *ctxMp3 = RL_CALLOC(1, sizeof(drmp3));
        int result = drmp3_init_file(ctxMp3, fileName, NULL);

        music.ctxType = MUSIC_AUDIO_MP3;
        music.ctxData = ctxMp3;

        if (result > 0)
        {
            music.stream = LoadAudioStream(ctxMp3->sampleRate, 32, ctxMp3->channels);
            music.frameCount = (unsigned int)drmp3_get_pcm_frame_count(ctxMp3);
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_XM)
    else if (IsFileExtension(fileName, ".xm"))
    {
        jar_xm_context_t *ctxXm = NULL;
        int result = jar_xm_create_context_from_file(&ctxXm, AUDIO.System.device.sampleRate, fileName);

        music.ctxType = MUSIC_MODULE_XM;
        music.ctxData = ctxXm;

        if (result == 0)    // XM AUDIO.System.context created successfully
        {
            jar_xm_set_max_loop_count(ctxXm, 0);    // Set infinite number of loops

            unsigned int bits = 32;
            if (AUDIO_DEVICE_FORMAT == ma_format_s16) bits = 16;
            else if (AUDIO_DEVICE_FORMAT == ma_format_u8) bits = 8;

            // NOTE: Only stereo is supported for XM
            music.stream = LoadAudioStream(AUDIO.System.device.sampleRate, bits, AUDIO_DEVICE_CHANNELS);
            music.frameCount = (unsigned int)jar_xm_get_remaining_samples(ctxXm);    // NOTE: Always 2 channels (stereo)
            music.looping = true;   // Looping enabled by default
            jar_xm_reset(ctxXm);    // make sure we start at the beginning of the song
            musicLoaded = true;
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_MOD)
    else if (IsFileExtension(fileName, ".mod"))
    {
        jar_mod_context_t *ctxMod = RL_CALLOC(1, sizeof(jar_mod_context_t));
        jar_mod_init(ctxMod);
        int result = jar_mod_load_file(ctxMod, fileName);

        music.ctxType = MUSIC_MODULE_MOD;
        music.ctxData = ctxMod;

        if (result > 0)
        {
            // NOTE: Only stereo is supported for MOD
            music.stream = LoadAudioStream(AUDIO.System.device.sampleRate, 16, AUDIO_DEVICE_CHANNELS);
            music.frameCount = (unsigned int)jar_mod_max_samples(ctxMod);    // NOTE: Always 2 channels (stereo)
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
    }
#endif
    else TRACELOG(LOG_WARNING, "STREAM: [%s] File format not supported", fileName);

    if (!musicLoaded)
    {
        if (false) { }
    #if defined(SUPPORT_FILEFORMAT_WAV)
        else if (music.ctxType == MUSIC_AUDIO_WAV) drwav_uninit((drwav *)music.ctxData);
    #endif
    #if defined(SUPPORT_FILEFORMAT_OGG)
        else if (music.ctxType == MUSIC_AUDIO_OGG) stb_vorbis_close((stb_vorbis *)music.ctxData);
    #endif
    #if defined(SUPPORT_FILEFORMAT_FLAC)
        else if (music.ctxType == MUSIC_AUDIO_FLAC) drflac_free((drflac *)music.ctxData, NULL);
    #endif
    #if defined(SUPPORT_FILEFORMAT_MP3)
        else if (music.ctxType == MUSIC_AUDIO_MP3) { drmp3_uninit((drmp3 *)music.ctxData); RL_FREE(music.ctxData); }
    #endif
    #if defined(SUPPORT_FILEFORMAT_XM)
        else if (music.ctxType == MUSIC_MODULE_XM) jar_xm_free_context((jar_xm_context_t *)music.ctxData);
    #endif
    #if defined(SUPPORT_FILEFORMAT_MOD)
        else if (music.ctxType == MUSIC_MODULE_MOD) { jar_mod_unload((jar_mod_context_t *)music.ctxData); RL_FREE(music.ctxData); }
    #endif

        music.ctxData = NULL;
        TRACELOG(LOG_WARNING, "FILEIO: [%s] Music file could not be opened", fileName);
    }
    else
    {
        // Show some music stream info
        TRACELOG(LOG_INFO, "FILEIO: [%s] Music file loaded successfully", fileName);
        TRACELOG(LOG_INFO, "    > Sample rate:   %i Hz", music.stream.sampleRate);
        TRACELOG(LOG_INFO, "    > Sample size:   %i bits", music.stream.sampleSize);
        TRACELOG(LOG_INFO, "    > Channels:      %i (%s)", music.stream.channels, (music.stream.channels == 1)? "Mono" : (music.stream.channels == 2)? "Stereo" : "Multi");
        TRACELOG(LOG_INFO, "    > Total frames:  %i", music.frameCount);
    }

    return music;
}

// Load music stream from memory buffer, fileType refers to extension: i.e. ".wav"
// WARNING: File extension must be provided in lower-case
Music LoadMusicStreamFromMemory(const char *fileType, const unsigned char *data, int dataSize)
{
    Music music = { 0 };
    bool musicLoaded = false;

    if (false) { }
#if defined(SUPPORT_FILEFORMAT_WAV)
    else if (strcmp(fileType, ".wav") == 0)
    {
        drwav *ctxWav = RL_CALLOC(1, sizeof(drwav));

        bool success = drwav_init_memory(ctxWav, (const void *)data, dataSize, NULL);

        music.ctxType = MUSIC_AUDIO_WAV;
        music.ctxData = ctxWav;

        if (success)
        {
            int sampleSize = ctxWav->bitsPerSample;
            if (ctxWav->bitsPerSample == 24) sampleSize = 16;   // Forcing conversion to s16 on UpdateMusicStream()

            music.stream = LoadAudioStream(ctxWav->sampleRate, sampleSize, ctxWav->channels);
            music.frameCount = (unsigned int)ctxWav->totalPCMFrameCount;
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_FLAC)
    else if (strcmp(fileType, ".flac") == 0)
    {
        music.ctxType = MUSIC_AUDIO_FLAC;
        music.ctxData = drflac_open_memory((const void*)data, dataSize, NULL);

        if (music.ctxData != NULL)
        {
            drflac *ctxFlac = (drflac *)music.ctxData;

            music.stream = LoadAudioStream(ctxFlac->sampleRate, ctxFlac->bitsPerSample, ctxFlac->channels);
            music.frameCount = (unsigned int)ctxFlac->totalPCMFrameCount;
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_MP3)
    else if (strcmp(fileType, ".mp3") == 0)
    {
        drmp3 *ctxMp3 = RL_CALLOC(1, sizeof(drmp3));
        int success = drmp3_init_memory(ctxMp3, (const void*)data, dataSize, NULL);

        music.ctxType = MUSIC_AUDIO_MP3;
        music.ctxData = ctxMp3;

        if (success)
        {
            music.stream = LoadAudioStream(ctxMp3->sampleRate, 32, ctxMp3->channels);
            music.frameCount = (unsigned int)drmp3_get_pcm_frame_count(ctxMp3);
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_OGG)
    else if (strcmp(fileType, ".ogg") == 0)
    {
        // Open ogg audio stream
        music.ctxType = MUSIC_AUDIO_OGG;
        //music.ctxData = stb_vorbis_open_filename(fileName, NULL, NULL);
        music.ctxData = stb_vorbis_open_memory((const unsigned char *)data, dataSize, NULL, NULL);

        if (music.ctxData != NULL)
        {
            stb_vorbis_info info = stb_vorbis_get_info((stb_vorbis *)music.ctxData);  // Get Ogg file info

            // OGG bit rate defaults to 16 bit, it's enough for compressed format
            music.stream = LoadAudioStream(info.sample_rate, 16, info.channels);

            // WARNING: It seems this function returns length in frames, not samples, so we multiply by channels
            music.frameCount = (unsigned int)stb_vorbis_stream_length_in_samples((stb_vorbis *)music.ctxData);
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_XM)
    else if (strcmp(fileType, ".xm") == 0)
    {
        jar_xm_context_t *ctxXm = NULL;
        int result = jar_xm_create_context_safe(&ctxXm, (const char *)data, dataSize, AUDIO.System.device.sampleRate);
        if (result == 0)    // XM AUDIO.System.context created successfully
        {
            music.ctxType = MUSIC_MODULE_XM;
            jar_xm_set_max_loop_count(ctxXm, 0);    // Set infinite number of loops

            unsigned int bits = 32;
            if (AUDIO_DEVICE_FORMAT == ma_format_s16)
                bits = 16;
            else if (AUDIO_DEVICE_FORMAT == ma_format_u8)
                bits = 8;

            // NOTE: Only stereo is supported for XM
            music.stream = LoadAudioStream(AUDIO.System.device.sampleRate, bits, 2);
            music.frameCount = (unsigned int)jar_xm_get_remaining_samples(ctxXm);    // NOTE: Always 2 channels (stereo)
            music.looping = true;   // Looping enabled by default
            jar_xm_reset(ctxXm);   // make sure we start at the beginning of the song

            music.ctxData = ctxXm;
            musicLoaded = true;
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_MOD)
    else if (strcmp(fileType, ".mod") == 0)
    {
        jar_mod_context_t *ctxMod = (jar_mod_context_t *)RL_MALLOC(sizeof(jar_mod_context_t));
        int result = 0;

        jar_mod_init(ctxMod);

        // Copy data to allocated memory for default UnloadMusicStream
        unsigned char *newData = (unsigned char *)RL_MALLOC(dataSize);
        int it = dataSize/sizeof(unsigned char);
        for (int i = 0; i < it; i++) newData[i] = data[i];

        // Memory loaded version for jar_mod_load_file()
        if (dataSize && dataSize < 32*1024*1024)
        {
            ctxMod->modfilesize = dataSize;
            ctxMod->modfile = newData;
            if (jar_mod_load(ctxMod, (void *)ctxMod->modfile, dataSize)) result = dataSize;
        }

        if (result > 0)
        {
            music.ctxType = MUSIC_MODULE_MOD;

            // NOTE: Only stereo is supported for MOD
            music.stream = LoadAudioStream(AUDIO.System.device.sampleRate, 16, 2);
            music.frameCount = (unsigned int)jar_mod_max_samples(ctxMod);    // NOTE: Always 2 channels (stereo)
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;

            music.ctxData = ctxMod;
            musicLoaded = true;
        }
    }
#endif
    else TRACELOG(LOG_WARNING, "STREAM: Data format not supported");

    if (!musicLoaded)
    {
        if (false) { }
    #if defined(SUPPORT_FILEFORMAT_WAV)
        else if (music.ctxType == MUSIC_AUDIO_WAV) drwav_uninit((drwav *)music.ctxData);
    #endif
    #if defined(SUPPORT_FILEFORMAT_FLAC)
        else if (music.ctxType == MUSIC_AUDIO_FLAC) drflac_free((drflac *)music.ctxData, NULL);
    #endif
    #if defined(SUPPORT_FILEFORMAT_MP3)
        else if (music.ctxType == MUSIC_AUDIO_MP3) { drmp3_uninit((drmp3 *)music.ctxData); RL_FREE(music.ctxData); }
    #endif
    #if defined(SUPPORT_FILEFORMAT_OGG)
        else if (music.ctxType == MUSIC_AUDIO_OGG) stb_vorbis_close((stb_vorbis *)music.ctxData);
    #endif
    #if defined(SUPPORT_FILEFORMAT_XM)
        else if (music.ctxType == MUSIC_MODULE_XM) jar_xm_free_context((jar_xm_context_t *)music.ctxData);
    #endif
    #if defined(SUPPORT_FILEFORMAT_MOD)
        else if (music.ctxType == MUSIC_MODULE_MOD) { jar_mod_unload((jar_mod_context_t *)music.ctxData); RL_FREE(music.ctxData); }
    #endif

        music.ctxData = NULL;
        TRACELOG(LOG_WARNING, "FILEIO: Music data could not be loaded");
    }
    else
    {
        // Show some music stream info
        TRACELOG(LOG_INFO, "FILEIO: Music data loaded successfully");
        TRACELOG(LOG_INFO, "    > Sample rate:   %i Hz", music.stream.sampleRate);
        TRACELOG(LOG_INFO, "    > Sample size:   %i bits", music.stream.sampleSize);
        TRACELOG(LOG_INFO, "    > Channels:      %i (%s)", music.stream.channels, (music.stream.channels == 1)? "Mono" : (music.stream.channels == 2)? "Stereo" : "Multi");
        TRACELOG(LOG_INFO, "    > Total frames:  %i", music.frameCount);
    }

    return music;
}

// Unload music stream
void UnloadMusicStream(Music music)
{
    UnloadAudioStream(music.stream);

    if (music.ctxData != NULL)
    {
        if (false) { }
#if defined(SUPPORT_FILEFORMAT_WAV)
        else if (music.ctxType == MUSIC_AUDIO_WAV) drwav_uninit((drwav *)music.ctxData);
#endif
#if defined(SUPPORT_FILEFORMAT_OGG)
        else if (music.ctxType == MUSIC_AUDIO_OGG) stb_vorbis_close((stb_vorbis *)music.ctxData);
#endif
#if defined(SUPPORT_FILEFORMAT_FLAC)
        else if (music.ctxType == MUSIC_AUDIO_FLAC) drflac_free((drflac *)music.ctxData, NULL);
#endif
#if defined(SUPPORT_FILEFORMAT_MP3)
    else if (music.ctxType == MUSIC_AUDIO_MP3) { drmp3_uninit((drmp3 *)music.ctxData); RL_FREE(music.ctxData); }
#endif
#if defined(SUPPORT_FILEFORMAT_XM)
        else if (music.ctxType == MUSIC_MODULE_XM) jar_xm_free_context((jar_xm_context_t *)music.ctxData);
#endif
#if defined(SUPPORT_FILEFORMAT_MOD)
        else if (music.ctxType == MUSIC_MODULE_MOD) { jar_mod_unload((jar_mod_context_t *)music.ctxData); RL_FREE(music.ctxData); }
#endif
    }
}

// Start music playing (open stream)
void PlayMusicStream(Music music)
{
    if (music.stream.buffer != NULL)
    {
        // For music streams, we need to make sure we maintain the frame cursor position
        // This is a hack for this section of code in UpdateMusicStream()
        // NOTE: In case window is minimized, music stream is stopped, just make sure to
        // play again on window restore: if (IsMusicStreamPlaying(music)) PlayMusicStream(music);
        ma_uint32 frameCursorPos = music.stream.buffer->frameCursorPos;
        PlayAudioStream(music.stream);  // WARNING: This resets the cursor position.
        music.stream.buffer->frameCursorPos = frameCursorPos;
    }
}

// Pause music playing
void PauseMusicStream(Music music)
{
    PauseAudioStream(music.stream);
}

// Resume music playing
void ResumeMusicStream(Music music)
{
    ResumeAudioStream(music.stream);
}

// Stop music playing (close stream)
void StopMusicStream(Music music)
{
    StopAudioStream(music.stream);

    switch (music.ctxType)
    {
#if defined(SUPPORT_FILEFORMAT_WAV)
        case MUSIC_AUDIO_WAV: drwav_seek_to_pcm_frame((drwav *)music.ctxData, 0); break;
#endif
#if defined(SUPPORT_FILEFORMAT_OGG)
        case MUSIC_AUDIO_OGG: stb_vorbis_seek_start((stb_vorbis *)music.ctxData); break;
#endif
#if defined(SUPPORT_FILEFORMAT_FLAC)
        case MUSIC_AUDIO_FLAC: drflac_seek_to_pcm_frame((drflac *)music.ctxData, 0); break;
#endif
#if defined(SUPPORT_FILEFORMAT_MP3)
        case MUSIC_AUDIO_MP3: drmp3_seek_to_pcm_frame((drmp3 *)music.ctxData, 0); break;
#endif
#if defined(SUPPORT_FILEFORMAT_XM)
        case MUSIC_MODULE_XM: jar_xm_reset((jar_xm_context_t *)music.ctxData); break;
#endif
#if defined(SUPPORT_FILEFORMAT_MOD)
        case MUSIC_MODULE_MOD: jar_mod_seek_start((jar_mod_context_t *)music.ctxData); break;
#endif
        default: break;
    }
}

// Seek music to a certain position (in seconds)
void SeekMusicStream(Music music, float position)
{
    // Seeking is not supported in module formats
    if ((music.ctxType == MUSIC_MODULE_XM) || (music.ctxType == MUSIC_MODULE_MOD)) return;

    unsigned int positionInFrames = (unsigned int)(position*music.stream.sampleRate);

    switch (music.ctxType)
    {
#if defined(SUPPORT_FILEFORMAT_WAV)
        case MUSIC_AUDIO_WAV: drwav_seek_to_pcm_frame((drwav *)music.ctxData, positionInFrames); break;
#endif
#if defined(SUPPORT_FILEFORMAT_OGG)
        case MUSIC_AUDIO_OGG: stb_vorbis_seek_frame((stb_vorbis *)music.ctxData, positionInFrames); break;
#endif
#if defined(SUPPORT_FILEFORMAT_FLAC)
        case MUSIC_AUDIO_FLAC: drflac_seek_to_pcm_frame((drflac *)music.ctxData, positionInFrames); break;
#endif
#if defined(SUPPORT_FILEFORMAT_MP3)
        case MUSIC_AUDIO_MP3: drmp3_seek_to_pcm_frame((drmp3 *)music.ctxData, positionInFrames); break;
#endif
        default: break;
    }

    music.stream.buffer->framesProcessed = positionInFrames;
}

// Update (re-fill) music buffers if data already processed
void UpdateMusicStream(Music music)
{
    if (music.stream.buffer == NULL) return;

    bool streamEnding = false;
    unsigned int subBufferSizeInFrames = music.stream.buffer->sizeInFrames/2;

    // NOTE: Using dynamic allocation because it could require more than 16KB
    void *pcm = RL_CALLOC(subBufferSizeInFrames*music.stream.channels*music.stream.sampleSize/8, 1);

    int frameCountToStream = 0;    // Total size of data in frames to be streamed

    // TODO: Get the framesLeft using framesProcessed... but first, get total frames processed correctly...
    //ma_uint32 frameSizeInBytes = ma_get_bytes_per_sample(music.stream.buffer->dsp.formatConverterIn.config.formatIn)*music.stream.buffer->dsp.formatConverterIn.config.channels;
    unsigned int framesLeft = music.frameCount - music.stream.buffer->framesProcessed;

    while (IsAudioStreamProcessed(music.stream))
    {
        if (framesLeft >= subBufferSizeInFrames) frameCountToStream = subBufferSizeInFrames;
        else frameCountToStream = framesLeft;

        switch (music.ctxType)
        {
        #if defined(SUPPORT_FILEFORMAT_WAV)
            case MUSIC_AUDIO_WAV:
            {
                // NOTE: Returns the number of samples to process (not required)
                if (music.stream.sampleSize == 16) drwav_read_pcm_frames_s16((drwav *)music.ctxData, frameCountToStream, (short *)pcm);
                else if (music.stream.sampleSize == 32) drwav_read_pcm_frames_f32((drwav *)music.ctxData, frameCountToStream, (float *)pcm);

            } break;
        #endif
        #if defined(SUPPORT_FILEFORMAT_OGG)
            case MUSIC_AUDIO_OGG:
            {
                // NOTE: Returns the number of samples to process (be careful! we ask for number of shorts!)
                stb_vorbis_get_samples_short_interleaved((stb_vorbis *)music.ctxData, music.stream.channels, (short *)pcm, frameCountToStream*music.stream.channels);

            } break;
        #endif
        #if defined(SUPPORT_FILEFORMAT_FLAC)
            case MUSIC_AUDIO_FLAC:
            {
                // NOTE: Returns the number of samples to process (not required)
                drflac_read_pcm_frames_s16((drflac *)music.ctxData, frameCountToStream*music.stream.channels, (short *)pcm);

            } break;
        #endif
        #if defined(SUPPORT_FILEFORMAT_MP3)
            case MUSIC_AUDIO_MP3:
            {
                drmp3_read_pcm_frames_f32((drmp3 *)music.ctxData, frameCountToStream, (float *)pcm);

            } break;
        #endif
        #if defined(SUPPORT_FILEFORMAT_XM)
            case MUSIC_MODULE_XM:
            {
                // NOTE: Internally we consider 2 channels generation, so sampleCount/2
                if (AUDIO_DEVICE_FORMAT == ma_format_f32) jar_xm_generate_samples((jar_xm_context_t *)music.ctxData, (float *)pcm, frameCountToStream);
                else if (AUDIO_DEVICE_FORMAT == ma_format_s16) jar_xm_generate_samples_16bit((jar_xm_context_t *)music.ctxData, (short *)pcm, frameCountToStream);
                else if (AUDIO_DEVICE_FORMAT == ma_format_u8) jar_xm_generate_samples_8bit((jar_xm_context_t *)music.ctxData, (char *)pcm, frameCountToStream);

            } break;
        #endif
        #if defined(SUPPORT_FILEFORMAT_MOD)
            case MUSIC_MODULE_MOD:
            {
                // NOTE: 3rd parameter (nbsample) specify the number of stereo 16bits samples you want, so sampleCount/2
                jar_mod_fillbuffer((jar_mod_context_t *)music.ctxData, (short *)pcm, frameCountToStream, 0);
            } break;
        #endif
            default: break;
        }

        UpdateAudioStream(music.stream, pcm, frameCountToStream);

        framesLeft -= frameCountToStream;

        if (framesLeft <= 0)
        {
            streamEnding = true;
            break;
        }
    }

    // Free allocated pcm data
    RL_FREE(pcm);

    // Reset audio stream for looping
    if (streamEnding)
    {
        StopMusicStream(music);                     // Stop music (and reset)
        if (music.looping) PlayMusicStream(music);  // Play again
    }
    else
    {
        // NOTE: In case window is minimized, music stream is stopped,
        // just make sure to play again on window restore
        if (IsMusicStreamPlaying(music)) PlayMusicStream(music);
    }
}

// Check if any music is playing
bool IsMusicStreamPlaying(Music music)
{
    return IsAudioStreamPlaying(music.stream);
}

// Set volume for music
void SetMusicVolume(Music music, float volume)
{
    SetAudioStreamVolume(music.stream, volume);
}

// Set pitch for music
void SetMusicPitch(Music music, float pitch)
{
    SetAudioBufferPitch(music.stream.buffer, pitch);
}

// Set pan for a music
void SetMusicPan(Music music, float pan)
{
    SetAudioBufferPan(music.stream.buffer, pan);
}

// Get music time length (in seconds)
float GetMusicTimeLength(Music music)
{
    float totalSeconds = 0.0f;

    totalSeconds = (float)music.frameCount/music.stream.sampleRate;

    return totalSeconds;
}

// Get current music time played (in seconds)
float GetMusicTimePlayed(Music music)
{
    float secondsPlayed = 0.0f;
    if (music.stream.buffer != NULL)
    {
    #if defined(SUPPORT_FILEFORMAT_XM)
        if (music.ctxType == MUSIC_MODULE_XM)
        {
            uint64_t framesPlayed = 0;

            jar_xm_get_position(music.ctxData, NULL, NULL, NULL, &framesPlayed);
            secondsPlayed = (float)framesPlayed/music.stream.sampleRate;
        }
        else
    #endif
        {
            //ma_uint32 frameSizeInBytes = ma_get_bytes_per_sample(music.stream.buffer->dsp.formatConverterIn.config.formatIn)*music.stream.buffer->dsp.formatConverterIn.config.channels;
            unsigned int framesPlayed = music.stream.buffer->framesProcessed;
            secondsPlayed = (float)framesPlayed/music.stream.sampleRate;
        }
    }

    return secondsPlayed;
}

// Load audio stream (to stream audio pcm data)
AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize, unsigned int channels)
{
    AudioStream stream = { 0 };

    stream.sampleRate = sampleRate;
    stream.sampleSize = sampleSize;
    stream.channels = channels;

    ma_format formatIn = ((stream.sampleSize == 8)? ma_format_u8 : ((stream.sampleSize == 16)? ma_format_s16 : ma_format_f32));

    // The size of a streaming buffer must be at least double the size of a period
    unsigned int periodSize = AUDIO.System.device.playback.internalPeriodSizeInFrames;

    // If the buffer is not set, compute one that would give us a buffer good enough for a decent frame rate
    unsigned int subBufferSize = (AUDIO.Buffer.defaultSize == 0)? AUDIO.System.device.sampleRate/30 : AUDIO.Buffer.defaultSize;

    if (subBufferSize < periodSize) subBufferSize = periodSize;

    // Create a double audio buffer of defined size
    stream.buffer = LoadAudioBuffer(formatIn, stream.channels, stream.sampleRate, subBufferSize*2, AUDIO_BUFFER_USAGE_STREAM);

    if (stream.buffer != NULL)
    {
        stream.buffer->looping = true;    // Always loop for streaming buffers
        TRACELOG(LOG_INFO, "STREAM: Initialized successfully (%i Hz, %i bit, %s)", stream.sampleRate, stream.sampleSize, (stream.channels == 1)? "Mono" : "Stereo");
    }
    else TRACELOG(LOG_WARNING, "STREAM: Failed to load audio buffer, stream could not be created");

    return stream;
}

// Unload audio stream and free memory
void UnloadAudioStream(AudioStream stream)
{
    UnloadAudioBuffer(stream.buffer);

    TRACELOG(LOG_INFO, "STREAM: Unloaded audio stream data from RAM");
}

// Update audio stream buffers with data
// NOTE 1: Only updates one buffer of the stream source: unqueue -> update -> queue
// NOTE 2: To unqueue a buffer it needs to be processed: IsAudioStreamProcessed()
void UpdateAudioStream(AudioStream stream, const void *data, int frameCount)
{
    if (stream.buffer != NULL)
    {
        if (stream.buffer->isSubBufferProcessed[0] || stream.buffer->isSubBufferProcessed[1])
        {
            ma_uint32 subBufferToUpdate = 0;

            if (stream.buffer->isSubBufferProcessed[0] && stream.buffer->isSubBufferProcessed[1])
            {
                // Both buffers are available for updating.
                // Update the first one and make sure the cursor is moved back to the front.
                subBufferToUpdate = 0;
                stream.buffer->frameCursorPos = 0;
            }
            else
            {
                // Just update whichever sub-buffer is processed.
                subBufferToUpdate = (stream.buffer->isSubBufferProcessed[0])? 0 : 1;
            }

            ma_uint32 subBufferSizeInFrames = stream.buffer->sizeInFrames/2;
            unsigned char *subBuffer = stream.buffer->data + ((subBufferSizeInFrames*stream.channels*(stream.sampleSize/8))*subBufferToUpdate);

            // TODO: Get total frames processed on this buffer... DOES NOT WORK.
            stream.buffer->framesProcessed += subBufferSizeInFrames;

            // Does this API expect a whole buffer to be updated in one go?
            // Assuming so, but if not will need to change this logic.
            if (subBufferSizeInFrames >= (ma_uint32)frameCount)
            {
                ma_uint32 framesToWrite = subBufferSizeInFrames;

                if (framesToWrite > (ma_uint32)frameCount) framesToWrite = (ma_uint32)frameCount;

                ma_uint32 bytesToWrite = framesToWrite*stream.channels*(stream.sampleSize/8);
                memcpy(subBuffer, data, bytesToWrite);

                // Any leftover frames should be filled with zeros.
                ma_uint32 leftoverFrameCount = subBufferSizeInFrames - framesToWrite;

                if (leftoverFrameCount > 0) memset(subBuffer + bytesToWrite, 0, leftoverFrameCount*stream.channels*(stream.sampleSize/8));

                stream.buffer->isSubBufferProcessed[subBufferToUpdate] = false;
            }
            else TRACELOG(LOG_WARNING, "STREAM: Attempting to write too many frames to buffer");
        }
        else TRACELOG(LOG_WARNING, "STREAM: Buffer not available for updating");
    }
}

// Check if any audio stream buffers requires refill
bool IsAudioStreamProcessed(AudioStream stream)
{
    if (stream.buffer == NULL) return false;

    return (stream.buffer->isSubBufferProcessed[0] || stream.buffer->isSubBufferProcessed[1]);
}

// Play audio stream
void PlayAudioStream(AudioStream stream)
{
    PlayAudioBuffer(stream.buffer);
}

// Play audio stream
void PauseAudioStream(AudioStream stream)
{
    PauseAudioBuffer(stream.buffer);
}

// Resume audio stream playing
void ResumeAudioStream(AudioStream stream)
{
    ResumeAudioBuffer(stream.buffer);
}

// Check if audio stream is playing.
bool IsAudioStreamPlaying(AudioStream stream)
{
    return IsAudioBufferPlaying(stream.buffer);
}

// Stop audio stream
void StopAudioStream(AudioStream stream)
{
    StopAudioBuffer(stream.buffer);
}

// Set volume for audio stream (1.0 is max level)
void SetAudioStreamVolume(AudioStream stream, float volume)
{
    SetAudioBufferVolume(stream.buffer, volume);
}

// Set pitch for audio stream (1.0 is base level)
void SetAudioStreamPitch(AudioStream stream, float pitch)
{
    SetAudioBufferPitch(stream.buffer, pitch);
}

// Set pan for audio stream
void SetAudioStreamPan(AudioStream stream, float pan)
{
    SetAudioBufferPan(stream.buffer, pan);
}

// Default size for new audio streams
void SetAudioStreamBufferSizeDefault(int size)
{
    AUDIO.Buffer.defaultSize = size;
}

// Audio thread callback to request new data
void SetAudioStreamCallback(AudioStream stream, AudioCallback callback)
{
    if (stream.buffer != NULL) stream.buffer->callback = callback;
}

// Add processor to audio stream. Contrary to buffers, the order of processors is important.
// The new processor must be added at the end. As there aren't supposed to be a lot of processors attached to
// a given stream, we iterate through the list to find the end. That way we don't need a pointer to the last element.
void AttachAudioStreamProcessor(AudioStream stream, AudioCallback process)
{
    ma_mutex_lock(&AUDIO.System.lock);

    rAudioProcessor *processor = (rAudioProcessor *)RL_CALLOC(1, sizeof(rAudioProcessor));
    processor->process = process;

    rAudioProcessor *last = stream.buffer->processor;

    while (last && last->next)
    {
        last = last->next;
    }
    if (last)
    {
        processor->prev = last;
        last->next = processor;
    }
    else stream.buffer->processor = processor;

    ma_mutex_unlock(&AUDIO.System.lock);
}

void DetachAudioStreamProcessor(AudioStream stream, AudioCallback process)
{
    ma_mutex_lock(&AUDIO.System.lock);

    rAudioProcessor *processor = stream.buffer->processor;

    while (processor)
    {
        rAudioProcessor *next = processor->next;
        rAudioProcessor *prev = processor->prev;

        if (processor->process == process)
        {
            if (stream.buffer->processor == processor) stream.buffer->processor = next;
            if (prev) prev->next = next;
            if (next) next->prev = prev;

            RL_FREE(processor);
        }

        processor = next;
    }

    ma_mutex_unlock(&AUDIO.System.lock);
}

//----------------------------------------------------------------------------------
// Module specific Functions Definition
//----------------------------------------------------------------------------------

// Log callback function
static void OnLog(void *pUserData, ma_uint32 level, const char *pMessage)
{
    TRACELOG(LOG_WARNING, "miniaudio: %s", pMessage);   // All log messages from miniaudio are errors
}

// Reads audio data from an AudioBuffer object in internal format.
static ma_uint32 ReadAudioBufferFramesInInternalFormat(AudioBuffer *audioBuffer, void *framesOut, ma_uint32 frameCount)
{
    // Using audio buffer callback
    if (audioBuffer->callback)
    {
        audioBuffer->callback(framesOut, frameCount);
        audioBuffer->framesProcessed += frameCount;

        return frameCount;
    }

    ma_uint32 subBufferSizeInFrames = (audioBuffer->sizeInFrames > 1)? audioBuffer->sizeInFrames/2 : audioBuffer->sizeInFrames;
    ma_uint32 currentSubBufferIndex = audioBuffer->frameCursorPos/subBufferSizeInFrames;

    if (currentSubBufferIndex > 1) return 0;

    // Another thread can update the processed state of buffers so
    // we just take a copy here to try and avoid potential synchronization problems
    bool isSubBufferProcessed[2] = { 0 };
    isSubBufferProcessed[0] = audioBuffer->isSubBufferProcessed[0];
    isSubBufferProcessed[1] = audioBuffer->isSubBufferProcessed[1];

    ma_uint32 frameSizeInBytes = ma_get_bytes_per_frame(audioBuffer->converter.formatIn, audioBuffer->converter.channelsIn);

    // Fill out every frame until we find a buffer that's marked as processed. Then fill the remainder with 0
    ma_uint32 framesRead = 0;
    while (1)
    {
        // We break from this loop differently depending on the buffer's usage
        //  - For static buffers, we simply fill as much data as we can
        //  - For streaming buffers we only fill the halves of the buffer that are processed
        //    Unprocessed halves must keep their audio data in-tact
        if (audioBuffer->usage == AUDIO_BUFFER_USAGE_STATIC)
        {
            if (framesRead >= frameCount) break;
        }
        else
        {
            if (isSubBufferProcessed[currentSubBufferIndex]) break;
        }

        ma_uint32 totalFramesRemaining = (frameCount - framesRead);
        if (totalFramesRemaining == 0) break;

        ma_uint32 framesRemainingInOutputBuffer;
        if (audioBuffer->usage == AUDIO_BUFFER_USAGE_STATIC)
        {
            framesRemainingInOutputBuffer = audioBuffer->sizeInFrames - audioBuffer->frameCursorPos;
        }
        else
        {
            ma_uint32 firstFrameIndexOfThisSubBuffer = subBufferSizeInFrames*currentSubBufferIndex;
            framesRemainingInOutputBuffer = subBufferSizeInFrames - (audioBuffer->frameCursorPos - firstFrameIndexOfThisSubBuffer);
        }

        ma_uint32 framesToRead = totalFramesRemaining;
        if (framesToRead > framesRemainingInOutputBuffer) framesToRead = framesRemainingInOutputBuffer;

        memcpy((unsigned char *)framesOut + (framesRead*frameSizeInBytes), audioBuffer->data + (audioBuffer->frameCursorPos*frameSizeInBytes), framesToRead*frameSizeInBytes);
        audioBuffer->frameCursorPos = (audioBuffer->frameCursorPos + framesToRead)%audioBuffer->sizeInFrames;
        framesRead += framesToRead;

        // If we've read to the end of the buffer, mark it as processed
        if (framesToRead == framesRemainingInOutputBuffer)
        {
            audioBuffer->isSubBufferProcessed[currentSubBufferIndex] = true;
            isSubBufferProcessed[currentSubBufferIndex] = true;

            currentSubBufferIndex = (currentSubBufferIndex + 1)%2;

            // We need to break from this loop if we're not looping
            if (!audioBuffer->looping)
            {
                StopAudioBuffer(audioBuffer);
                break;
            }
        }
    }

    // Zero-fill excess
    ma_uint32 totalFramesRemaining = (frameCount - framesRead);
    if (totalFramesRemaining > 0)
    {
        memset((unsigned char *)framesOut + (framesRead*frameSizeInBytes), 0, totalFramesRemaining*frameSizeInBytes);

        // For static buffers we can fill the remaining frames with silence for safety, but we don't want
        // to report those frames as "read". The reason for this is that the caller uses the return value
        // to know whether or not a non-looping sound has finished playback.
        if (audioBuffer->usage != AUDIO_BUFFER_USAGE_STATIC) framesRead += totalFramesRemaining;
    }

    return framesRead;
}

// Reads audio data from an AudioBuffer object in device format. Returned data will be in a format appropriate for mixing.
static ma_uint32 ReadAudioBufferFramesInMixingFormat(AudioBuffer *audioBuffer, float *framesOut, ma_uint32 frameCount)
{
    // What's going on here is that we're continuously converting data from the AudioBuffer's internal format to the mixing format, which
    // should be defined by the output format of the data converter. We do this until frameCount frames have been output. The important
    // detail to remember here is that we never, ever attempt to read more input data than is required for the specified number of output
    // frames. This can be achieved with ma_data_converter_get_required_input_frame_count().
    ma_uint8 inputBuffer[4096] = { 0 };
    ma_uint32 inputBufferFrameCap = sizeof(inputBuffer)/ma_get_bytes_per_frame(audioBuffer->converter.formatIn, audioBuffer->converter.channelsIn);

    ma_uint32 totalOutputFramesProcessed = 0;
    while (totalOutputFramesProcessed < frameCount)
    {
        ma_uint64 outputFramesToProcessThisIteration = frameCount - totalOutputFramesProcessed;
        ma_uint64 inputFramesToProcessThisIteration = 0;

        ma_result result = ma_data_converter_get_required_input_frame_count(&audioBuffer->converter, outputFramesToProcessThisIteration, &inputFramesToProcessThisIteration);
        if (inputFramesToProcessThisIteration > inputBufferFrameCap)
        {
            inputFramesToProcessThisIteration = inputBufferFrameCap;
        }

        float *runningFramesOut = framesOut + (totalOutputFramesProcessed*audioBuffer->converter.channelsOut);

        /* At this point we can convert the data to our mixing format. */
        ma_uint64 inputFramesProcessedThisIteration = ReadAudioBufferFramesInInternalFormat(audioBuffer, inputBuffer, (ma_uint32)inputFramesToProcessThisIteration);    /* Safe cast. */
        ma_uint64 outputFramesProcessedThisIteration = outputFramesToProcessThisIteration;
        ma_data_converter_process_pcm_frames(&audioBuffer->converter, inputBuffer, &inputFramesProcessedThisIteration, runningFramesOut, &outputFramesProcessedThisIteration);

        totalOutputFramesProcessed += (ma_uint32)outputFramesProcessedThisIteration; /* Safe cast. */

        if (inputFramesProcessedThisIteration < inputFramesToProcessThisIteration)
        {
            break;  /* Ran out of input data. */
        }

        /* This should never be hit, but will add it here for safety. Ensures we get out of the loop when no input nor output frames are processed. */
        if (inputFramesProcessedThisIteration == 0 && outputFramesProcessedThisIteration == 0)
        {
            break;
        }
    }

    return totalOutputFramesProcessed;
}


// Sending audio data to device callback function
// This function will be called when miniaudio needs more data
// NOTE: All the mixing takes place here
static void OnSendAudioDataToDevice(ma_device *pDevice, void *pFramesOut, const void *pFramesInput, ma_uint32 frameCount)
{
    (void)pDevice;

    // Mixing is basically just an accumulation, we need to initialize the output buffer to 0
    memset(pFramesOut, 0, frameCount*pDevice->playback.channels*ma_get_bytes_per_sample(pDevice->playback.format));

    // Using a mutex here for thread-safety which makes things not real-time
    // This is unlikely to be necessary for this project, but may want to consider how you might want to avoid this
    ma_mutex_lock(&AUDIO.System.lock);
    {
        for (AudioBuffer *audioBuffer = AUDIO.Buffer.first; audioBuffer != NULL; audioBuffer = audioBuffer->next)
        {
            // Ignore stopped or paused sounds
            if (!audioBuffer->playing || audioBuffer->paused) continue;

            ma_uint32 framesRead = 0;

            while (1)
            {
                if (framesRead >= frameCount) break;

                // Just read as much data as we can from the stream
                ma_uint32 framesToRead = (frameCount - framesRead);

                while (framesToRead > 0)
                {
                    float tempBuffer[1024] = { 0 }; // Frames for stereo

                    ma_uint32 framesToReadRightNow = framesToRead;
                    if (framesToReadRightNow > sizeof(tempBuffer)/sizeof(tempBuffer[0])/AUDIO_DEVICE_CHANNELS)
                    {
                        framesToReadRightNow = sizeof(tempBuffer)/sizeof(tempBuffer[0])/AUDIO_DEVICE_CHANNELS;
                    }

                    ma_uint32 framesJustRead = ReadAudioBufferFramesInMixingFormat(audioBuffer, tempBuffer, framesToReadRightNow);
                    if (framesJustRead > 0)
                    {
                        float *framesOut = (float *)pFramesOut + (framesRead*AUDIO.System.device.playback.channels);
                        float *framesIn = tempBuffer;

                        // Apply processors chain if defined
                        rAudioProcessor *processor = audioBuffer->processor;
                        while (processor)
                        {
                            processor->process(framesIn, framesJustRead);
                            processor = processor->next;
                        }

                        MixAudioFrames(framesOut, framesIn, framesJustRead, audioBuffer);

                        framesToRead -= framesJustRead;
                        framesRead += framesJustRead;
                    }

                    if (!audioBuffer->playing)
                    {
                        framesRead = frameCount;
                        break;
                    }

                    // If we weren't able to read all the frames we requested, break
                    if (framesJustRead < framesToReadRightNow)
                    {
                        if (!audioBuffer->looping)
                        {
                            StopAudioBuffer(audioBuffer);
                            break;
                        }
                        else
                        {
                            // Should never get here, but just for safety,
                            // move the cursor position back to the start and continue the loop
                            audioBuffer->frameCursorPos = 0;
                            continue;
                        }
                    }
                }

                // If for some reason we weren't able to read every frame we'll need to break from the loop
                // Not doing this could theoretically put us into an infinite loop
                if (framesToRead > 0) break;
            }
        }
    }

    ma_mutex_unlock(&AUDIO.System.lock);
}

// Main mixing function, pretty simple in this project, just an accumulation
// NOTE: framesOut is both an input and an output, it is initially filled with zeros outside of this function
static void MixAudioFrames(float *framesOut, const float *framesIn, ma_uint32 frameCount, AudioBuffer *buffer)
{
    const float localVolume = buffer->volume;
    const ma_uint32 channels = AUDIO.System.device.playback.channels;

    if (channels == 2)  // We consider panning
    {
        const float left = buffer->pan;
        const float right = 1.0f - left;

        // Fast sine approximation in [0..1] for pan law: y = 0.5f*x*(3 - x*x);
        const float levels[2] = { localVolume*0.5f*left*(3.0f - left*left), localVolume*0.5f*right*(3.0f - right*right) };

        float *frameOut = framesOut;
        const float *frameIn = framesIn;

        for (ma_uint32 frame = 0; frame < frameCount; frame++)
        {
            frameOut[0] += (frameIn[0]*levels[0]);
            frameOut[1] += (frameIn[1]*levels[1]);

            frameOut += 2;
            frameIn += 2;
        }
    }
    else  // We do not consider panning
    {
        for (ma_uint32 frame = 0; frame < frameCount; frame++)
        {
            for (ma_uint32 c = 0; c < channels; c++)
            {
                float *frameOut = framesOut + (frame*channels);
                const float *frameIn = framesIn + (frame*channels);

                // Output accumulates input multiplied by volume to provided output (usually 0)
                frameOut[c] += (frameIn[c]*localVolume);
            }
        }
    }
}



#undef AudioBuffer

//*****************************************************************************************************************************************************************************************

#if defined(SUPPORT_FILEFORMAT_OBJ) || defined(SUPPORT_FILEFORMAT_MTL)
    #define TINYOBJ_MALLOC RL_MALLOC
    #define TINYOBJ_CALLOC RL_CALLOC
    #define TINYOBJ_REALLOC RL_REALLOC
    #define TINYOBJ_FREE RL_FREE

    #define TINYOBJ_LOADER_C_IMPLEMENTATION
    #include "external/tinyobj_loader_c.h"      // OBJ/MTL file formats loading
#endif

#if defined(SUPPORT_FILEFORMAT_GLTF)
    #define CGLTF_MALLOC RL_MALLOC
    #define CGLTF_FREE RL_FREE

    #define CGLTF_IMPLEMENTATION
    #include "external/cgltf.h"         // glTF file format loading
#endif

#if defined(SUPPORT_FILEFORMAT_VOX)
    #define VOX_MALLOC RL_MALLOC
    #define VOX_CALLOC RL_CALLOC
    #define VOX_REALLOC RL_REALLOC
    #define VOX_FREE RL_FREE

    #define VOX_LOADER_IMPLEMENTATION
    #include "external/vox_loader.h"    // VOX file format loading (MagikaVoxel)
#endif

#if defined(SUPPORT_MESH_GENERATION)
    #define PAR_MALLOC(T, N) ((T*)RL_MALLOC(N*sizeof(T)))
    #define PAR_CALLOC(T, N) ((T*)RL_CALLOC(N*sizeof(T), 1))
    #define PAR_REALLOC(T, BUF, N) ((T*)RL_REALLOC(BUF, sizeof(T)*(N)))
    #define PAR_FREE RL_FREE

    #define PAR_SHAPES_IMPLEMENTATION
    #include "external/par_shapes.h"    // Shapes 3d parametric generation
#endif

#if defined(_WIN32)
    #include <direct.h>     // Required for: _chdir() [Used in LoadOBJ()]
    #define CHDIR _chdir
#else
    #include <unistd.h>     // Required for: chdir() (POSIX) [Used in LoadOBJ()]
    #define CHDIR chdir
#endif

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef MAX_MATERIAL_MAPS
    #define MAX_MATERIAL_MAPS       12    // Maximum number of maps supported
#endif
#ifndef MAX_MESH_VERTEX_BUFFERS
    #define MAX_MESH_VERTEX_BUFFERS  7    // Maximum vertex buffers (VBO) per mesh
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
// ...

//----------------------------------------------------------------------------------
// Module specific Functions Declaration
//----------------------------------------------------------------------------------
#if defined(SUPPORT_FILEFORMAT_OBJ)
static Model LoadOBJ(const char *fileName);     // Load OBJ mesh data
#endif
#if defined(SUPPORT_FILEFORMAT_IQM)
static Model LoadIQM(const char *fileName);     // Load IQM mesh data
static ModelAnimation *LoadModelAnimationsIQM(const char *fileName, unsigned int *animCount);    // Load IQM animation data
#endif
#if defined(SUPPORT_FILEFORMAT_GLTF)
static Model LoadGLTF(const char *fileName);    // Load GLTF mesh data
//static ModelAnimation *LoadModelAnimationGLTF(const char *fileName, unsigned int *animCount);    // Load GLTF animation data
#endif
#if defined(SUPPORT_FILEFORMAT_VOX)
static Model LoadVOX(const char *filename);     // Load VOX mesh data
#endif

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Draw a line in 3D world space
void DrawLine3D(Vector3 startPos, Vector3 endPos, Color color)
{
    // WARNING: Be careful with internal buffer vertex alignment
    // when using RL_LINES or RL_TRIANGLES, data is aligned to fit
    // lines-triangles-quads in the same indexed buffers!!!
    rlCheckRenderBatchLimit(8);

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(startPos.x, startPos.y, startPos.z);
        rlVertex3f(endPos.x, endPos.y, endPos.z);
    rlEnd();
}

// Draw a point in 3D space, actually a small line
void DrawPoint3D(Vector3 position, Color color)
{
    rlCheckRenderBatchLimit(8);

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex3f(0.0f, 0.0f, 0.0f);
            rlVertex3f(0.0f, 0.0f, 0.1f);
        rlEnd();
    rlPopMatrix();
}

// Draw a circle in 3D world space
void DrawCircle3D(Vector3 center, float radius, Vector3 rotationAxis, float rotationAngle, Color color)
{
    rlCheckRenderBatchLimit(2*36);

    rlPushMatrix();
        rlTranslatef(center.x, center.y, center.z);
        rlRotatef(rotationAngle, rotationAxis.x, rotationAxis.y, rotationAxis.z);

        rlBegin(RL_LINES);
            for (int i = 0; i < 360; i += 10)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);

                rlVertex3f(sinf(DEG2RAD*i)*radius, cosf(DEG2RAD*i)*radius, 0.0f);
                rlVertex3f(sinf(DEG2RAD*(i + 10))*radius, cosf(DEG2RAD*(i + 10))*radius, 0.0f);
            }
        rlEnd();
    rlPopMatrix();
}

// Draw a color-filled triangle (vertex in counter-clockwise order!)
void DrawTriangle3D(Vector3 v1, Vector3 v2, Vector3 v3, Color color)
{
    rlCheckRenderBatchLimit(3);

    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(v1.x, v1.y, v1.z);
        rlVertex3f(v2.x, v2.y, v2.z);
        rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
}

// Draw a triangle strip defined by points
void DrawTriangleStrip3D(Vector3 *points, int pointCount, Color color)
{
    if (pointCount >= 3)
    {
        rlCheckRenderBatchLimit(3*(pointCount - 2));

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 2; i < pointCount; i++)
            {
                if ((i%2) == 0)
                {
                    rlVertex3f(points[i].x, points[i].y, points[i].z);
                    rlVertex3f(points[i - 2].x, points[i - 2].y, points[i - 2].z);
                    rlVertex3f(points[i - 1].x, points[i - 1].y, points[i - 1].z);
                }
                else
                {
                    rlVertex3f(points[i].x, points[i].y, points[i].z);
                    rlVertex3f(points[i - 1].x, points[i - 1].y, points[i - 1].z);
                    rlVertex3f(points[i - 2].x, points[i - 2].y, points[i - 2].z);
                }
            }
        rlEnd();
    }
}

// Draw cube
// NOTE: Cube position is the center position
void DrawCube(Vector3 position, float width, float height, float length, Color color)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    rlCheckRenderBatchLimit(36);

    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> rotate -> translate)
        rlTranslatef(position.x, position.y, position.z);
        //rlRotatef(45, 0, 1, 0);
        //rlScalef(1.0f, 1.0f, 1.0f);   // NOTE: Vertices are directly scaled on definition

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            // Front face
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left

            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right

            // Back face
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Left
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right

            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left

            // Top face
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Bottom Left
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Bottom Right

            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Bottom Right

            // Bottom face
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Top Left
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left

            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Top Right
            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Top Left

            // Right face
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Left

            rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Left
            rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Left

            // Left face
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
            rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Right

            rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left
            rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
            rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right
        rlEnd();
    rlPopMatrix();
}

// Draw cube (Vector version)
void DrawCubeV(Vector3 position, Vector3 size, Color color)
{
    DrawCube(position, size.x, size.y, size.z, color);
}

// Draw cube wires
void DrawCubeWires(Vector3 position, float width, float height, float length, Color color)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    rlCheckRenderBatchLimit(36);

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            // Front face -----------------------------------------------------
            // Bottom line
            rlVertex3f(x-width/2, y-height/2, z+length/2);  // Bottom left
            rlVertex3f(x+width/2, y-height/2, z+length/2);  // Bottom right

            // Left line
            rlVertex3f(x+width/2, y-height/2, z+length/2);  // Bottom right
            rlVertex3f(x+width/2, y+height/2, z+length/2);  // Top right

            // Top line
            rlVertex3f(x+width/2, y+height/2, z+length/2);  // Top right
            rlVertex3f(x-width/2, y+height/2, z+length/2);  // Top left

            // Right line
            rlVertex3f(x-width/2, y+height/2, z+length/2);  // Top left
            rlVertex3f(x-width/2, y-height/2, z+length/2);  // Bottom left

            // Back face ------------------------------------------------------
            // Bottom line
            rlVertex3f(x-width/2, y-height/2, z-length/2);  // Bottom left
            rlVertex3f(x+width/2, y-height/2, z-length/2);  // Bottom right

            // Left line
            rlVertex3f(x+width/2, y-height/2, z-length/2);  // Bottom right
            rlVertex3f(x+width/2, y+height/2, z-length/2);  // Top right

            // Top line
            rlVertex3f(x+width/2, y+height/2, z-length/2);  // Top right
            rlVertex3f(x-width/2, y+height/2, z-length/2);  // Top left

            // Right line
            rlVertex3f(x-width/2, y+height/2, z-length/2);  // Top left
            rlVertex3f(x-width/2, y-height/2, z-length/2);  // Bottom left

            // Top face -------------------------------------------------------
            // Left line
            rlVertex3f(x-width/2, y+height/2, z+length/2);  // Top left front
            rlVertex3f(x-width/2, y+height/2, z-length/2);  // Top left back

            // Right line
            rlVertex3f(x+width/2, y+height/2, z+length/2);  // Top right front
            rlVertex3f(x+width/2, y+height/2, z-length/2);  // Top right back

            // Bottom face  ---------------------------------------------------
            // Left line
            rlVertex3f(x-width/2, y-height/2, z+length/2);  // Top left front
            rlVertex3f(x-width/2, y-height/2, z-length/2);  // Top left back

            // Right line
            rlVertex3f(x+width/2, y-height/2, z+length/2);  // Top right front
            rlVertex3f(x+width/2, y-height/2, z-length/2);  // Top right back
        rlEnd();
    rlPopMatrix();
}

// Draw cube wires (vector version)
void DrawCubeWiresV(Vector3 position, Vector3 size, Color color)
{
    DrawCubeWires(position, size.x, size.y, size.z, color);
}

// Draw cube
// NOTE: Cube position is the center position
void DrawCubeTexture(Texture2D texture, Vector3 position, float width, float height, float length, Color color)
{
    float x = position.x;
    float y = position.y;
    float z = position.z;

    rlCheckRenderBatchLimit(36);

    rlSetTexture(texture.id);

    //rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> rotate -> translate)
        //rlTranslatef(2.0f, 0.0f, 0.0f);
        //rlRotatef(45, 0, 1, 0);
        //rlScalef(2.0f, 2.0f, 2.0f);

        rlBegin(RL_QUADS);
            rlColor4ub(color.r, color.g, color.b, color.a);
            // Front Face
            rlNormal3f(0.0f, 0.0f, 1.0f);                  // Normal Pointing Towards Viewer
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left Of The Texture and Quad
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Left Of The Texture and Quad
            // Back Face
            rlNormal3f(0.0f, 0.0f, - 1.0f);                  // Normal Pointing Away From Viewer
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Left Of The Texture and Quad
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Left Of The Texture and Quad
            // Top Face
            rlNormal3f(0.0f, 1.0f, 0.0f);                  // Normal Pointing Up
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left Of The Texture and Quad
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - width/2, y + height/2, z + length/2);  // Bottom Left Of The Texture and Quad
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + width/2, y + height/2, z + length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right Of The Texture and Quad
            // Bottom Face
            rlNormal3f(0.0f, - 1.0f, 0.0f);                  // Normal Pointing Down
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x - width/2, y - height/2, z - length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x + width/2, y - height/2, z - length/2);  // Top Left Of The Texture and Quad
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Left Of The Texture and Quad
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Right Of The Texture and Quad
            // Right face
            rlNormal3f(1.0f, 0.0f, 0.0f);                  // Normal Pointing Right
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z + length/2);  // Top Left Of The Texture and Quad
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Left Of The Texture and Quad
            // Left Face
            rlNormal3f( - 1.0f, 0.0f, 0.0f);                  // Normal Pointing Left
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Left Of The Texture and Quad
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z + length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Left Of The Texture and Quad
        rlEnd();
    //rlPopMatrix();

    rlSetTexture(0);
}

// Draw cube with texture piece applied to all faces
void DrawCubeTextureRec(Texture2D texture, Rectangle source, Vector3 position, float width, float height, float length, Color color)
{
    float x = position.x;
    float y = position.y;
    float z = position.z;
    float texWidth = (float)texture.width;
    float texHeight = (float)texture.height;

    rlCheckRenderBatchLimit(36);

    rlSetTexture(texture.id);

    rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // Front face
        rlNormal3f(0.0f, 0.0f, 1.0f);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y - height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y - height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y + height/2, z + length/2);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y + height/2, z + length/2);

        // Back face
        rlNormal3f(0.0f, 0.0f, - 1.0f);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y - height/2, z - length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y + height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y + height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y - height/2, z - length/2);

        // Top face
        rlNormal3f(0.0f, 1.0f, 0.0f);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y + height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y + height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y + height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y + height/2, z - length/2);

        // Bottom face
        rlNormal3f(0.0f, - 1.0f, 0.0f);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y - height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y - height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y - height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y - height/2, z + length/2);

        // Right face
        rlNormal3f(1.0f, 0.0f, 0.0f);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y - height/2, z - length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y + height/2, z - length/2);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x + width/2, y + height/2, z + length/2);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x + width/2, y - height/2, z + length/2);

        // Left face
        rlNormal3f( - 1.0f, 0.0f, 0.0f);
        rlTexCoord2f(source.x/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y - height/2, z - length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, (source.y + source.height)/texHeight);
        rlVertex3f(x - width/2, y - height/2, z + length/2);
        rlTexCoord2f((source.x + source.width)/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y + height/2, z + length/2);
        rlTexCoord2f(source.x/texWidth, source.y/texHeight);
        rlVertex3f(x - width/2, y + height/2, z - length/2);

    rlEnd();

    rlSetTexture(0);
}

// Draw sphere
void DrawSphere(Vector3 centerPos, float radius, Color color)
{
    DrawSphereEx(centerPos, radius, 16, 16, color);
}

// Draw sphere with extended parameters
void DrawSphereEx(Vector3 centerPos, float radius, int rings, int slices, Color color)
{
    int numVertex = (rings + 2)*slices*6;
    rlCheckRenderBatchLimit(numVertex);

    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> translate)
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(radius, radius, radius);

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 0; i < (rings + 2); i++)
            {
                for (int j = 0; j < slices; j++)
                {
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*j/slices)));

                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                }
            }
        rlEnd();
    rlPopMatrix();
}

// Draw sphere wires
void DrawSphereWires(Vector3 centerPos, float radius, int rings, int slices, Color color)
{
    int numVertex = (rings + 2)*slices*6;
    rlCheckRenderBatchLimit(numVertex);

    rlPushMatrix();
        // NOTE: Transformation is applied in inverse order (scale -> translate)
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(radius, radius, radius);

        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 0; i < (rings + 2); i++)
            {
                for (int j = 0; j < slices; j++)
                {
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));

                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*j/slices)));

                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*j/slices)));
                    rlVertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
                               sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
                               cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
                }
            }
        rlEnd();
    rlPopMatrix();
}

// Draw a cylinder
// NOTE: It could be also used for pyramid and cone
void DrawCylinder(Vector3 position, float radiusTop, float radiusBottom, float height, int sides, Color color)
{
    if (sides < 3) sides = 3;

    int numVertex = sides*6;
    rlCheckRenderBatchLimit(numVertex);

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            if (radiusTop > 0)
            {
                // Draw Body -------------------------------------------------------------------------------------
                for (int i = 0; i < 360; i += 360/sides)
                {
                    rlVertex3f(sinf(DEG2RAD*i)*radiusBottom, 0, cosf(DEG2RAD*i)*radiusBottom); //Bottom Left
                    rlVertex3f(sinf(DEG2RAD*(i + 360.0f/sides))*radiusBottom, 0, cosf(DEG2RAD*(i + 360.0f/sides))*radiusBottom); //Bottom Right
                    rlVertex3f(sinf(DEG2RAD*(i + 360.0f/sides))*radiusTop, height, cosf(DEG2RAD*(i + 360.0f/sides))*radiusTop); //Top Right

                    rlVertex3f(sinf(DEG2RAD*i)*radiusTop, height, cosf(DEG2RAD*i)*radiusTop); //Top Left
                    rlVertex3f(sinf(DEG2RAD*i)*radiusBottom, 0, cosf(DEG2RAD*i)*radiusBottom); //Bottom Left
                    rlVertex3f(sinf(DEG2RAD*(i + 360.0f/sides))*radiusTop, height, cosf(DEG2RAD*(i + 360.0f/sides))*radiusTop); //Top Right
                }

                // Draw Cap --------------------------------------------------------------------------------------
                for (int i = 0; i < 360; i += 360/sides)
                {
                    rlVertex3f(0, height, 0);
                    rlVertex3f(sinf(DEG2RAD*i)*radiusTop, height, cosf(DEG2RAD*i)*radiusTop);
                    rlVertex3f(sinf(DEG2RAD*(i + 360.0f/sides))*radiusTop, height, cosf(DEG2RAD*(i + 360.0f/sides))*radiusTop);
                }
            }
            else
            {
                // Draw Cone -------------------------------------------------------------------------------------
                for (int i = 0; i < 360; i += 360/sides)
                {
                    rlVertex3f(0, height, 0);
                    rlVertex3f(sinf(DEG2RAD*i)*radiusBottom, 0, cosf(DEG2RAD*i)*radiusBottom);
                    rlVertex3f(sinf(DEG2RAD*(i + 360.0f/sides))*radiusBottom, 0, cosf(DEG2RAD*(i + 360.0f/sides))*radiusBottom);
                }
            }

            // Draw Base -----------------------------------------------------------------------------------------
            for (int i = 0; i < 360; i += 360/sides)
            {
                rlVertex3f(0, 0, 0);
                rlVertex3f(sinf(DEG2RAD*(i + 360.0f/sides))*radiusBottom, 0, cosf(DEG2RAD*(i + 360.0f/sides))*radiusBottom);
                rlVertex3f(sinf(DEG2RAD*i)*radiusBottom, 0, cosf(DEG2RAD*i)*radiusBottom);
            }
        rlEnd();
    rlPopMatrix();
}

// Draw a cylinder with base at startPos and top at endPos
// NOTE: It could be also used for pyramid and cone
void DrawCylinderEx(Vector3 startPos, Vector3 endPos, float startRadius, float endRadius, int sides, Color color)
{
    if (sides < 3) sides = 3;

    int numVertex = sides*6;
    rlCheckRenderBatchLimit(numVertex);

    Vector3 direction = { endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };
    if ((direction.x == 0) && (direction.y == 0) && (direction.z == 0)) return;

    // Construct a basis of the base and the top face:
    Vector3 b1 = Vector3Normalize(Vector3Perpendicular(direction));
    Vector3 b2 = Vector3Normalize(Vector3CrossProduct(b1, direction));

    float baseAngle = (2.0f*PI)/sides;

    rlBegin(RL_TRIANGLES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        for (int i = 0; i < sides; i++) {
            // compute the four vertices
            float s1 = sinf(baseAngle*(i + 0))*startRadius;
            float c1 = cosf(baseAngle*(i + 0))*startRadius;
            Vector3 w1 = { startPos.x + s1*b1.x + c1*b2.x, startPos.y + s1*b1.y + c1*b2.y, startPos.z + s1*b1.z + c1*b2.z };
            float s2 = sinf(baseAngle*(i + 1))*startRadius;
            float c2 = cosf(baseAngle*(i + 1))*startRadius;
            Vector3 w2 = { startPos.x + s2*b1.x + c2*b2.x, startPos.y + s2*b1.y + c2*b2.y, startPos.z + s2*b1.z + c2*b2.z };
            float s3 = sinf(baseAngle*(i + 0))*endRadius;
            float c3 = cosf(baseAngle*(i + 0))*endRadius;
            Vector3 w3 = { endPos.x + s3*b1.x + c3*b2.x, endPos.y + s3*b1.y + c3*b2.y, endPos.z + s3*b1.z + c3*b2.z };
            float s4 = sinf(baseAngle*(i + 1))*endRadius;
            float c4 = cosf(baseAngle*(i + 1))*endRadius;
            Vector3 w4 = { endPos.x + s4*b1.x + c4*b2.x, endPos.y + s4*b1.y + c4*b2.y, endPos.z + s4*b1.z + c4*b2.z };

            if (startRadius > 0) {                              //
                rlVertex3f(startPos.x, startPos.y, startPos.z); // |
                rlVertex3f(w2.x, w2.y, w2.z);                   // T0
                rlVertex3f(w1.x, w1.y, w1.z);                   // |
            }                                                   //
                                                                //          w2 x.-----------x startPos
            rlVertex3f(w1.x, w1.y, w1.z);                       // |           |\'.  T0    /
            rlVertex3f(w2.x, w2.y, w2.z);                       // T1          | \ '.     /
            rlVertex3f(w3.x, w3.y, w3.z);                       // |           |T \  '.  /
                                                                //             | 2 \ T 'x w1
            rlVertex3f(w2.x, w2.y, w2.z);                       // |        w4 x.---\-1-|---x endPos
            rlVertex3f(w4.x, w4.y, w4.z);                       // T2            '.  \  |T3/
            rlVertex3f(w3.x, w3.y, w3.z);                       // |               '. \ | /
                                                                //                   '.\|/
            if (endRadius > 0) {                                //                     'x w3
                rlVertex3f(endPos.x, endPos.y, endPos.z);       // |
                rlVertex3f(w3.x, w3.y, w3.z);                   // T3
                rlVertex3f(w4.x, w4.y, w4.z);                   // |
            }                                                   //
        }
    rlEnd();
}

// Draw a wired cylinder
// NOTE: It could be also used for pyramid and cone
void DrawCylinderWires(Vector3 position, float radiusTop, float radiusBottom, float height, int sides, Color color)
{
    if (sides < 3) sides = 3;

    int numVertex = sides*8;
    rlCheckRenderBatchLimit(numVertex);

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);

        rlBegin(RL_LINES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 0; i < 360; i += 360/sides)
            {
                rlVertex3f(sinf(DEG2RAD*i)*radiusBottom, 0, cosf(DEG2RAD*i)*radiusBottom);
                rlVertex3f(sinf(DEG2RAD*(i + 360.0f/sides))*radiusBottom, 0, cosf(DEG2RAD*(i + 360.0f/sides))*radiusBottom);

                rlVertex3f(sinf(DEG2RAD*(i + 360.0f/sides))*radiusBottom, 0, cosf(DEG2RAD*(i + 360.0f/sides))*radiusBottom);
                rlVertex3f(sinf(DEG2RAD*(i + 360.0f/sides))*radiusTop, height, cosf(DEG2RAD*(i + 360.0f/sides))*radiusTop);

                rlVertex3f(sinf(DEG2RAD*(i + 360.0f/sides))*radiusTop, height, cosf(DEG2RAD*(i + 360.0f/sides))*radiusTop);
                rlVertex3f(sinf(DEG2RAD*i)*radiusTop, height, cosf(DEG2RAD*i)*radiusTop);

                rlVertex3f(sinf(DEG2RAD*i)*radiusTop, height, cosf(DEG2RAD*i)*radiusTop);
                rlVertex3f(sinf(DEG2RAD*i)*radiusBottom, 0, cosf(DEG2RAD*i)*radiusBottom);
            }
        rlEnd();
    rlPopMatrix();
}


// Draw a wired cylinder with base at startPos and top at endPos
// NOTE: It could be also used for pyramid and cone
void DrawCylinderWiresEx(Vector3 startPos, Vector3 endPos, float startRadius, float endRadius, int sides, Color color)
{
    if (sides < 3) sides = 3;

    int numVertex = sides*6;
    rlCheckRenderBatchLimit(numVertex);

    Vector3 direction = { endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };
    if ((direction.x == 0) && (direction.y == 0) && (direction.z == 0))return;

    // Construct a basis of the base and the top face:
    Vector3 b1 = Vector3Normalize(Vector3Perpendicular(direction));
    Vector3 b2 = Vector3Normalize(Vector3CrossProduct(b1, direction));

    float baseAngle = (2.0f*PI)/sides;

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        for (int i = 0; i < sides; i++) {
            // compute the four vertices
            float s1 = sinf(baseAngle*(i + 0))*startRadius;
            float c1 = cosf(baseAngle*(i + 0))*startRadius;
            Vector3 w1 = { startPos.x + s1*b1.x + c1*b2.x, startPos.y + s1*b1.y + c1*b2.y, startPos.z + s1*b1.z + c1*b2.z };
            float s2 = sinf(baseAngle*(i + 1))*startRadius;
            float c2 = cosf(baseAngle*(i + 1))*startRadius;
            Vector3 w2 = { startPos.x + s2*b1.x + c2*b2.x, startPos.y + s2*b1.y + c2*b2.y, startPos.z + s2*b1.z + c2*b2.z };
            float s3 = sinf(baseAngle*(i + 0))*endRadius;
            float c3 = cosf(baseAngle*(i + 0))*endRadius;
            Vector3 w3 = { endPos.x + s3*b1.x + c3*b2.x, endPos.y + s3*b1.y + c3*b2.y, endPos.z + s3*b1.z + c3*b2.z };
            float s4 = sinf(baseAngle*(i + 1))*endRadius;
            float c4 = cosf(baseAngle*(i + 1))*endRadius;
            Vector3 w4 = { endPos.x + s4*b1.x + c4*b2.x, endPos.y + s4*b1.y + c4*b2.y, endPos.z + s4*b1.z + c4*b2.z };

            rlVertex3f(w1.x, w1.y, w1.z);
            rlVertex3f(w2.x, w2.y, w2.z);

            rlVertex3f(w1.x, w1.y, w1.z);
            rlVertex3f(w3.x, w3.y, w3.z);

            rlVertex3f(w3.x, w3.y, w3.z);
            rlVertex3f(w4.x, w4.y, w4.z);
        }
    rlEnd();
}


// Draw a plane
void DrawPlane(Vector3 centerPos, Vector2 size, Color color)
{
    rlCheckRenderBatchLimit(4);

    // NOTE: Plane is always created on XZ ground
    rlPushMatrix();
        rlTranslatef(centerPos.x, centerPos.y, centerPos.z);
        rlScalef(size.x, 1.0f, size.y);

        rlBegin(RL_QUADS);
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlNormal3f(0.0f, 1.0f, 0.0f);

            rlVertex3f(-0.5f, 0.0f, -0.5f);
            rlVertex3f(-0.5f, 0.0f, 0.5f);
            rlVertex3f(0.5f, 0.0f, 0.5f);
            rlVertex3f(0.5f, 0.0f, -0.5f);
        rlEnd();
    rlPopMatrix();
}

// Draw a ray line
void DrawRay(Ray ray, Color color)
{
    float scale = 10000;

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlVertex3f(ray.position.x, ray.position.y, ray.position.z);
        rlVertex3f(ray.position.x + ray.direction.x*scale, ray.position.y + ray.direction.y*scale, ray.position.z + ray.direction.z*scale);
    rlEnd();
}

// Draw a grid centered at (0, 0, 0)
void DrawGrid(int slices, float spacing)
{
    int halfSlices = slices/2;

    rlCheckRenderBatchLimit((slices + 2)*4);

    rlBegin(RL_LINES);
        for (int i = -halfSlices; i <= halfSlices; i++)
        {
            if (i == 0)
            {
                rlColor3f(0.5f, 0.5f, 0.5f);
                rlColor3f(0.5f, 0.5f, 0.5f);
                rlColor3f(0.5f, 0.5f, 0.5f);
                rlColor3f(0.5f, 0.5f, 0.5f);
            }
            else
            {
                rlColor3f(0.75f, 0.75f, 0.75f);
                rlColor3f(0.75f, 0.75f, 0.75f);
                rlColor3f(0.75f, 0.75f, 0.75f);
                rlColor3f(0.75f, 0.75f, 0.75f);
            }

            rlVertex3f((float)i*spacing, 0.0f, (float)-halfSlices*spacing);
            rlVertex3f((float)i*spacing, 0.0f, (float)halfSlices*spacing);

            rlVertex3f((float)-halfSlices*spacing, 0.0f, (float)i*spacing);
            rlVertex3f((float)halfSlices*spacing, 0.0f, (float)i*spacing);
        }
    rlEnd();
}

// Load model from files (mesh and material)
Model LoadModel(const char *fileName)
{
    Model model = { 0 };

#if defined(SUPPORT_FILEFORMAT_OBJ)
    if (IsFileExtension(fileName, ".obj")) model = LoadOBJ(fileName);
#endif
#if defined(SUPPORT_FILEFORMAT_IQM)
    if (IsFileExtension(fileName, ".iqm")) model = LoadIQM(fileName);
#endif
#if defined(SUPPORT_FILEFORMAT_GLTF)
    if (IsFileExtension(fileName, ".gltf") || IsFileExtension(fileName, ".glb")) model = LoadGLTF(fileName);
#endif
#if defined(SUPPORT_FILEFORMAT_VOX)
    if (IsFileExtension(fileName, ".vox")) model = LoadVOX(fileName);
#endif

    // Make sure model transform is set to identity matrix!
    model.transform = MatrixIdentity();

    if (model.meshCount == 0)
    {
        model.meshCount = 1;
        model.meshes = (Mesh *)RL_CALLOC(model.meshCount, sizeof(Mesh));
#if defined(SUPPORT_MESH_GENERATION)
        TRACELOG(LOG_WARNING, "MESH: [%s] Failed to load mesh data, default to cube mesh", fileName);
        model.meshes[0] = GenMeshCube(1.0f, 1.0f, 1.0f);
#else
        TRACELOG(LOG_WARNING, "MESH: [%s] Failed to load mesh data", fileName);
#endif
    }
    else
    {
        // Upload vertex data to GPU (static mesh)
        for (int i = 0; i < model.meshCount; i++) UploadMesh(&model.meshes[i], false);
    }

    if (model.materialCount == 0)
    {
        TRACELOG(LOG_WARNING, "MATERIAL: [%s] Failed to load material data, default to white material", fileName);

        model.materialCount = 1;
        model.materials = (Material *)RL_CALLOC(model.materialCount, sizeof(Material));
        model.materials[0] = LoadMaterialDefault();

        if (model.meshMaterial == NULL) model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));
    }

    return model;
}

// Load model from generated mesh
// WARNING: A shallow copy of mesh is generated, passed by value,
// as long as struct contains pointers to data and some values, we get a copy
// of mesh pointing to same data as original version... be careful!
Model LoadModelFromMesh(Mesh mesh)
{
    Model model = { 0 };

    model.transform = MatrixIdentity();

    model.meshCount = 1;
    model.meshes = (Mesh *)RL_CALLOC(model.meshCount, sizeof(Mesh));
    model.meshes[0] = mesh;

    model.materialCount = 1;
    model.materials = (Material *)RL_CALLOC(model.materialCount, sizeof(Material));
    model.materials[0] = LoadMaterialDefault();

    model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));
    model.meshMaterial[0] = 0;  // First material index

    return model;
}

// Unload model (meshes/materials) from memory (RAM and/or VRAM)
// NOTE: This function takes care of all model elements, for a detailed control
// over them, use UnloadMesh() and UnloadMaterial()
void UnloadModel(Model model)
{
    // Unload meshes
    for (int i = 0; i < model.meshCount; i++) UnloadMesh(model.meshes[i]);

    // Unload materials maps
    // NOTE: As the user could be sharing shaders and textures between models,
    // we don't unload the material but just free it's maps,
    // the user is responsible for freeing models shaders and textures
    for (int i = 0; i < model.materialCount; i++) RL_FREE(model.materials[i].maps);

    // Unload arrays
    RL_FREE(model.meshes);
    RL_FREE(model.materials);
    RL_FREE(model.meshMaterial);

    // Unload animation data
    RL_FREE(model.bones);
    RL_FREE(model.bindPose);

    TRACELOG(LOG_INFO, "MODEL: Unloaded model (and meshes) from RAM and VRAM");
}

// Unload model (but not meshes) from memory (RAM and/or VRAM)
void UnloadModelKeepMeshes(Model model)
{
    // Unload materials maps
    // NOTE: As the user could be sharing shaders and textures between models,
    // we don't unload the material but just free it's maps,
    // the user is responsible for freeing models shaders and textures
    for (int i = 0; i < model.materialCount; i++) RL_FREE(model.materials[i].maps);

    // Unload arrays
    RL_FREE(model.meshes);
    RL_FREE(model.materials);
    RL_FREE(model.meshMaterial);

    // Unload animation data
    RL_FREE(model.bones);
    RL_FREE(model.bindPose);

    TRACELOG(LOG_INFO, "MODEL: Unloaded model (but not meshes) from RAM and VRAM");
}

// Compute model bounding box limits (considers all meshes)
BoundingBox GetModelBoundingBox(Model model)
{
    BoundingBox bounds = { 0 };

    if (model.meshCount > 0)
    {
        Vector3 temp = { 0 };
        bounds = GetMeshBoundingBox(model.meshes[0]);

        for (int i = 1; i < model.meshCount; i++)
        {
            BoundingBox tempBounds = GetMeshBoundingBox(model.meshes[i]);

            temp.x = (bounds.min.x < tempBounds.min.x)? bounds.min.x : tempBounds.min.x;
            temp.y = (bounds.min.y < tempBounds.min.y)? bounds.min.y : tempBounds.min.y;
            temp.z = (bounds.min.z < tempBounds.min.z)? bounds.min.z : tempBounds.min.z;
            bounds.min = temp;

            temp.x = (bounds.max.x > tempBounds.max.x)? bounds.max.x : tempBounds.max.x;
            temp.y = (bounds.max.y > tempBounds.max.y)? bounds.max.y : tempBounds.max.y;
            temp.z = (bounds.max.z > tempBounds.max.z)? bounds.max.z : tempBounds.max.z;
            bounds.max = temp;
        }
    }

    return bounds;
}

// Upload vertex data into a VAO (if supported) and VBO
void UploadMesh(Mesh *mesh, bool dynamic)
{
    if (mesh->vaoId > 0)
    {
        // Check if mesh has already been loaded in GPU
        TRACELOG(LOG_WARNING, "VAO: [ID %i] Trying to re-load an already loaded mesh", mesh->vaoId);
        return;
    }

    mesh->vboId = (unsigned int *)RL_CALLOC(MAX_MESH_VERTEX_BUFFERS, sizeof(unsigned int));

    mesh->vaoId = 0;        // Vertex Array Object
    mesh->vboId[0] = 0;     // Vertex buffer: positions
    mesh->vboId[1] = 0;     // Vertex buffer: texcoords
    mesh->vboId[2] = 0;     // Vertex buffer: normals
    mesh->vboId[3] = 0;     // Vertex buffer: colors
    mesh->vboId[4] = 0;     // Vertex buffer: tangents
    mesh->vboId[5] = 0;     // Vertex buffer: texcoords2
    mesh->vboId[6] = 0;     // Vertex buffer: indices

#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    mesh->vaoId = rlLoadVertexArray();
    rlEnableVertexArray(mesh->vaoId);

    // NOTE: Attributes must be uploaded considering default locations points

    // Enable vertex attributes: position (shader-location = 0)
    void *vertices = mesh->animVertices != NULL ? mesh->animVertices : mesh->vertices;
    mesh->vboId[0] = rlLoadVertexBuffer(vertices, mesh->vertexCount*3*sizeof(float), dynamic);
    rlSetVertexAttribute(0, 3, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(0);

    // Enable vertex attributes: texcoords (shader-location = 1)
    mesh->vboId[1] = rlLoadVertexBuffer(mesh->texcoords, mesh->vertexCount*2*sizeof(float), dynamic);
    rlSetVertexAttribute(1, 2, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(1);

    if (mesh->normals != NULL)
    {
        // Enable vertex attributes: normals (shader-location = 2)
        void *normals = mesh->animNormals != NULL ? mesh->animNormals : mesh->normals;
        mesh->vboId[2] = rlLoadVertexBuffer(normals, mesh->vertexCount*3*sizeof(float), dynamic);
        rlSetVertexAttribute(2, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(2);
    }
    else
    {
        // Default color vertex attribute set to WHITE
        float value[3] = { 1.0f, 1.0f, 1.0f };
        rlSetVertexAttributeDefault(2, value, SHADER_ATTRIB_VEC3, 3);
        rlDisableVertexAttribute(2);
    }

    if (mesh->colors != NULL)
    {
        // Enable vertex attribute: color (shader-location = 3)
        mesh->vboId[3] = rlLoadVertexBuffer(mesh->colors, mesh->vertexCount*4*sizeof(unsigned char), dynamic);
        rlSetVertexAttribute(3, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
        rlEnableVertexAttribute(3);
    }
    else
    {
        // Default color vertex attribute set to WHITE
        float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        rlSetVertexAttributeDefault(3, value, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(3);
    }

    if (mesh->tangents != NULL)
    {
        // Enable vertex attribute: tangent (shader-location = 4)
        mesh->vboId[4] = rlLoadVertexBuffer(mesh->tangents, mesh->vertexCount*4*sizeof(float), dynamic);
        rlSetVertexAttribute(4, 4, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(4);
    }
    else
    {
        // Default tangents vertex attribute
        float value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        rlSetVertexAttributeDefault(4, value, SHADER_ATTRIB_VEC4, 4);
        rlDisableVertexAttribute(4);
    }

    if (mesh->texcoords2 != NULL)
    {
        // Enable vertex attribute: texcoord2 (shader-location = 5)
        mesh->vboId[5] = rlLoadVertexBuffer(mesh->texcoords2, mesh->vertexCount*2*sizeof(float), dynamic);
        rlSetVertexAttribute(5, 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(5);
    }
    else
    {
        // Default texcoord2 vertex attribute
        float value[2] = { 0.0f, 0.0f };
        rlSetVertexAttributeDefault(5, value, SHADER_ATTRIB_VEC2, 2);
        rlDisableVertexAttribute(5);
    }

    if (mesh->indices != NULL)
    {
        mesh->vboId[6] = rlLoadVertexBufferElement(mesh->indices, mesh->triangleCount*3*sizeof(unsigned short), dynamic);
    }

    if (mesh->vaoId > 0) TRACELOG(LOG_INFO, "VAO: [ID %i] Mesh uploaded successfully to VRAM (GPU)", mesh->vaoId);
    else TRACELOG(LOG_INFO, "VBO: Mesh uploaded successfully to VRAM (GPU)");

    rlDisableVertexArray();
#endif
}

// Update mesh vertex data in GPU for a specific buffer index
void UpdateMeshBuffer(Mesh mesh, int index, const void *data, int dataSize, int offset)
{
    rlUpdateVertexBuffer(mesh.vboId[index], data, dataSize, offset);
}

// Draw a 3d mesh with material and transform
void DrawMesh(Mesh mesh, Material material, Matrix transform)
{
#if defined(GRAPHICS_API_OPENGL_11)
    #define GL_VERTEX_ARRAY         0x8074
    #define GL_NORMAL_ARRAY         0x8075
    #define GL_COLOR_ARRAY          0x8076
    #define GL_TEXTURE_COORD_ARRAY  0x8078

    rlEnableTexture(material.maps[MATERIAL_MAP_DIFFUSE].texture.id);

    rlEnableStatePointer(GL_VERTEX_ARRAY, mesh.vertices);
    rlEnableStatePointer(GL_TEXTURE_COORD_ARRAY, mesh.texcoords);
    rlEnableStatePointer(GL_NORMAL_ARRAY, mesh.normals);
    rlEnableStatePointer(GL_COLOR_ARRAY, mesh.colors);

    rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(transform));
        rlColor4ub(material.maps[MATERIAL_MAP_DIFFUSE].color.r,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.g,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.b,
                   material.maps[MATERIAL_MAP_DIFFUSE].color.a);

        if (mesh.indices != NULL) rlDrawVertexArrayElements(0, mesh.triangleCount*3, mesh.indices);
        else rlDrawVertexArray(0, mesh.vertexCount);
    rlPopMatrix();

    rlDisableStatePointer(GL_VERTEX_ARRAY);
    rlDisableStatePointer(GL_TEXTURE_COORD_ARRAY);
    rlDisableStatePointer(GL_NORMAL_ARRAY);
    rlDisableStatePointer(GL_COLOR_ARRAY);

    rlDisableTexture();
#endif

#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    // Bind shader program
    rlEnableShader(material.shader.id);

    // Send required data to shader (matrices, values)
    //-----------------------------------------------------
    // Upload to shader material.colDiffuse
    if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1)
    {
        float values[4] = {
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.r/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.g/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.b/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Upload to shader material.colSpecular (if location available)
    if (material.shader.locs[SHADER_LOC_COLOR_SPECULAR] != -1)
    {
        float values[4] = {
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.r/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.g/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.b/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_SPECULAR], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Get a copy of current matrices to work with,
    // just in case stereo render is required and we need to modify them
    // NOTE: At this point the modelview matrix just contains the view matrix (camera)
    // That's because BeginMode3D() sets it and there is no model-drawing function
    // that modifies it, all use rlPushMatrix() and rlPopMatrix()
    Matrix matModel = MatrixIdentity();
    Matrix matView = rlGetMatrixModelview();
    Matrix matModelView = MatrixIdentity();
    Matrix matProjection = rlGetMatrixProjection();

    // Upload view and projection matrices (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_VIEW], matView);
    if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_PROJECTION], matProjection);

    // Model transformation matrix is send to shader uniform location: SHADER_LOC_MATRIX_MODEL
    if (material.shader.locs[SHADER_LOC_MATRIX_MODEL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MODEL], transform);

    // Accumulate several model transformations:
    //    transform: model transformation provided (includes DrawModel() params combined with model.transform)
    //    rlGetMatrixTransform(): rlgl internal transform matrix due to push/pop matrix stack
    matModel = MatrixMultiply(transform, rlGetMatrixTransform());

    // Get model-view matrix
    matModelView = MatrixMultiply(matModel, matView);

    // Upload model normal matrix (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(matModel)));
    //-----------------------------------------------------

    // Bind active texture maps (if available)
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Enable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlEnableTextureCubemap(material.maps[i].texture.id);
            else rlEnableTexture(material.maps[i].texture.id);

            rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
        }
    }

    // Try binding vertex array objects (VAO)
    // or use VBOs if not possible
    if (!rlEnableVertexArray(mesh.vaoId))
    {
        // Bind mesh VBO data: vertex position (shader-location = 0)
        rlEnableVertexBuffer(mesh.vboId[0]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

        // Bind mesh VBO data: vertex texcoords (shader-location = 1)
        rlEnableVertexBuffer(mesh.vboId[1]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);

        if (material.shader.locs[SHADER_LOC_VERTEX_NORMAL] != -1)
        {
            // Bind mesh VBO data: vertex normals (shader-location = 2)
            rlEnableVertexBuffer(mesh.vboId[2]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL], 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL]);
        }

        // Bind mesh VBO data: vertex colors (shader-location = 3, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_COLOR] != -1)
        {
            if (mesh.vboId[3] != 0)
            {
                rlEnableVertexBuffer(mesh.vboId[3]);
                rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
            else
            {
                // Set default value for unused attribute
                // NOTE: Required when using default shader and no VAO support
                float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(material.shader.locs[SHADER_LOC_VERTEX_COLOR], value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
        }

        // Bind mesh VBO data: vertex tangents (shader-location = 4, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TANGENT] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[4]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT]);
        }

        // Bind mesh VBO data: vertex texcoords2 (shader-location = 5, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[5]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02], 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02]);
        }

        if (mesh.indices != NULL) rlEnableVertexBufferElement(mesh.vboId[6]);
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++)
    {
        // Calculate model-view-projection matrix (MVP)
        Matrix matModelViewProjection = MatrixIdentity();
        if (eyeCount == 1) matModelViewProjection = MatrixMultiply(matModelView, matProjection);
        else
        {
            // Setup current eye viewport (half screen width)
            rlViewport(eye*rlGetFramebufferWidth()/2, 0, rlGetFramebufferWidth()/2, rlGetFramebufferHeight());
            matModelViewProjection = MatrixMultiply(MatrixMultiply(matModelView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

        // Draw mesh
        if (mesh.indices != NULL) rlDrawVertexArrayElements(0, mesh.triangleCount*3, 0);
        else rlDrawVertexArray(0, mesh.vertexCount);
    }

    // Unbind all binded texture maps
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        // Select current shader texture slot
        rlActiveTextureSlot(i);

        // Disable texture for active slot
        if ((i == MATERIAL_MAP_IRRADIANCE) ||
            (i == MATERIAL_MAP_PREFILTER) ||
            (i == MATERIAL_MAP_CUBEMAP)) rlDisableTextureCubemap();
        else rlDisableTexture();
    }

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Disable shader program
    rlDisableShader();

    // Restore rlgl internal modelview and projection matrices
    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
#endif
}

// Draw multiple mesh instances with material and different transforms
void DrawMeshInstanced(Mesh mesh, Material material, const Matrix *transforms, int instances)
{
#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
    // Instancing required variables
    float16 *instanceTransforms = NULL;
    unsigned int instancesVboId = 0;

    // Bind shader program
    rlEnableShader(material.shader.id);

    // Send required data to shader (matrices, values)
    //-----------------------------------------------------
    // Upload to shader material.colDiffuse
    if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1)
    {
        float values[4] = {
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.r/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.g/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.b/255.0f,
            (float)material.maps[MATERIAL_MAP_DIFFUSE].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Upload to shader material.colSpecular (if location available)
    if (material.shader.locs[SHADER_LOC_COLOR_SPECULAR] != -1)
    {
        float values[4] = {
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.r/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.g/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.b/255.0f,
            (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.a/255.0f
        };

        rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_SPECULAR], values, SHADER_UNIFORM_VEC4, 1);
    }

    // Get a copy of current matrices to work with,
    // just in case stereo render is required and we need to modify them
    // NOTE: At this point the modelview matrix just contains the view matrix (camera)
    // That's because BeginMode3D() sets it and there is no model-drawing function
    // that modifies it, all use rlPushMatrix() and rlPopMatrix()
    Matrix matModel = MatrixIdentity();
    Matrix matView = rlGetMatrixModelview();
    Matrix matModelView = MatrixIdentity();
    Matrix matProjection = rlGetMatrixProjection();

    // Upload view and projection matrices (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_VIEW], matView);
    if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_PROJECTION], matProjection);

    // Create instances buffer
    instanceTransforms = (float16 *)RL_MALLOC(instances*sizeof(float16));

    // Fill buffer with instances transformations as float16 arrays
    for (int i = 0; i < instances; i++) instanceTransforms[i] = MatrixToFloatV(transforms[i]);

    // Enable mesh VAO to attach new buffer
    rlEnableVertexArray(mesh.vaoId);

    // This could alternatively use a static VBO and either glMapBuffer() or glBufferSubData().
    // It isn't clear which would be reliably faster in all cases and on all platforms,
    // anecdotally glMapBuffer() seems very slow (syncs) while glBufferSubData() seems
    // no faster, since we're transferring all the transform matrices anyway
    instancesVboId = rlLoadVertexBuffer(instanceTransforms, instances*sizeof(float16), false);

    // Instances transformation matrices are send to shader attribute location: SHADER_LOC_MATRIX_MODEL
    for (unsigned int i = 0; i < 4; i++)
    {
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_MATRIX_MODEL] + i);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_MATRIX_MODEL] + i, 4, RL_FLOAT, 0, sizeof(Matrix), (void *)(i*sizeof(Vector4)));
        rlSetVertexAttributeDivisor(material.shader.locs[SHADER_LOC_MATRIX_MODEL] + i, 1);
    }

    rlDisableVertexBuffer();
    rlDisableVertexArray();

    // Accumulate internal matrix transform (push/pop) and view matrix
    // NOTE: In this case, model instance transformation must be computed in the shader
    matModelView = MatrixMultiply(rlGetMatrixTransform(), matView);

    // Upload model normal matrix (if locations available)
    if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1) rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(matModel)));
    //-----------------------------------------------------

    // Bind active texture maps (if available)
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        if (material.maps[i].texture.id > 0)
        {
            // Select current shader texture slot
            rlActiveTextureSlot(i);

            // Enable texture for active slot
            if ((i == MATERIAL_MAP_IRRADIANCE) ||
                (i == MATERIAL_MAP_PREFILTER) ||
                (i == MATERIAL_MAP_CUBEMAP)) rlEnableTextureCubemap(material.maps[i].texture.id);
            else rlEnableTexture(material.maps[i].texture.id);

            rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
        }
    }

    // Try binding vertex array objects (VAO)
    // or use VBOs if not possible
    if (!rlEnableVertexArray(mesh.vaoId))
    {
        // Bind mesh VBO data: vertex position (shader-location = 0)
        rlEnableVertexBuffer(mesh.vboId[0]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

        // Bind mesh VBO data: vertex texcoords (shader-location = 1)
        rlEnableVertexBuffer(mesh.vboId[1]);
        rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01], 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01]);

        if (material.shader.locs[SHADER_LOC_VERTEX_NORMAL] != -1)
        {
            // Bind mesh VBO data: vertex normals (shader-location = 2)
            rlEnableVertexBuffer(mesh.vboId[2]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL], 3, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_NORMAL]);
        }

        // Bind mesh VBO data: vertex colors (shader-location = 3, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_COLOR] != -1)
        {
            if (mesh.vboId[3] != 0)
            {
                rlEnableVertexBuffer(mesh.vboId[3]);
                rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR], 4, RL_UNSIGNED_BYTE, 1, 0, 0);
                rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
            else
            {
                // Set default value for unused attribute
                // NOTE: Required when using default shader and no VAO support
                float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                rlSetVertexAttributeDefault(material.shader.locs[SHADER_LOC_VERTEX_COLOR], value, SHADER_ATTRIB_VEC4, 4);
                rlDisableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_COLOR]);
            }
        }

        // Bind mesh VBO data: vertex tangents (shader-location = 4, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TANGENT] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[4]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT], 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TANGENT]);
        }

        // Bind mesh VBO data: vertex texcoords2 (shader-location = 5, if available)
        if (material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] != -1)
        {
            rlEnableVertexBuffer(mesh.vboId[5]);
            rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02], 2, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_TEXCOORD02]);
        }

        if (mesh.indices != NULL) rlEnableVertexBufferElement(mesh.vboId[6]);
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++)
    {
        // Calculate model-view-projection matrix (MVP)
        Matrix matModelViewProjection = MatrixIdentity();
        if (eyeCount == 1) matModelViewProjection = MatrixMultiply(matModelView, matProjection);
        else
        {
            // Setup current eye viewport (half screen width)
            rlViewport(eye*rlGetFramebufferWidth()/2, 0, rlGetFramebufferWidth()/2, rlGetFramebufferHeight());
            matModelViewProjection = MatrixMultiply(MatrixMultiply(matModelView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

        // Draw mesh instanced
        if (mesh.indices != NULL) rlDrawVertexArrayElementsInstanced(0, mesh.triangleCount*3, 0, instances);
        else rlDrawVertexArrayInstanced(0, mesh.vertexCount, instances);
    }

    // Unbind all binded texture maps
    for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
    {
        // Select current shader texture slot
        rlActiveTextureSlot(i);

        // Disable texture for active slot
        if ((i == MATERIAL_MAP_IRRADIANCE) ||
            (i == MATERIAL_MAP_PREFILTER) ||
            (i == MATERIAL_MAP_CUBEMAP)) rlDisableTextureCubemap();
        else rlDisableTexture();
    }

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Disable shader program
    rlDisableShader();

    // Remove instance transforms buffer
    rlUnloadVertexBuffer(instancesVboId);
    RL_FREE(instanceTransforms);
#endif
}

// Unload mesh from memory (RAM and VRAM)
void UnloadMesh(Mesh mesh)
{
    // Unload rlgl mesh vboId data
    rlUnloadVertexArray(mesh.vaoId);

    if (mesh.vboId != NULL) for (int i = 0; i < MAX_MESH_VERTEX_BUFFERS; i++) rlUnloadVertexBuffer(mesh.vboId[i]);
    RL_FREE(mesh.vboId);

    RL_FREE(mesh.vertices);
    RL_FREE(mesh.texcoords);
    RL_FREE(mesh.normals);
    RL_FREE(mesh.colors);
    RL_FREE(mesh.tangents);
    RL_FREE(mesh.texcoords2);
    RL_FREE(mesh.indices);

    RL_FREE(mesh.animVertices);
    RL_FREE(mesh.animNormals);
    RL_FREE(mesh.boneWeights);
    RL_FREE(mesh.boneIds);
}

// Export mesh data to file
bool ExportMesh(Mesh mesh, const char *fileName)
{
    bool success = false;

    if (IsFileExtension(fileName, ".obj"))
    {
        // Estimated data size, it should be enough...
        int dataSize = mesh.vertexCount*(int)strlen("v 0000.00f 0000.00f 0000.00f") +
                       mesh.vertexCount*(int)strlen("vt 0.000f 0.00f") +
                       mesh.vertexCount*(int)strlen("vn 0.000f 0.00f 0.00f") +
                       mesh.triangleCount*(int)strlen("f 00000/00000/00000 00000/00000/00000 00000/00000/00000");

        // NOTE: Text data buffer size is estimated considering mesh data size
        char *txtData = (char *)RL_CALLOC(dataSize*2 + 2000, sizeof(char));

        int byteCount = 0;
        byteCount += sprintf(txtData + byteCount, "# //////////////////////////////////////////////////////////////////////////////////\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# // rMeshOBJ exporter v1.0 - Mesh exported as triangle faces and not optimized   //\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# // more info and bugs-report:  github.com/raysan5/raylib                        //\n");
        byteCount += sprintf(txtData + byteCount, "# // feedback and support:       ray[at]raylib.com                                //\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# // Copyright (c) 2018-2022 Ramon Santamaria (@raysan5)                          //\n");
        byteCount += sprintf(txtData + byteCount, "# //                                                                              //\n");
        byteCount += sprintf(txtData + byteCount, "# //////////////////////////////////////////////////////////////////////////////////\n\n");
        byteCount += sprintf(txtData + byteCount, "# Vertex Count:     %i\n", mesh.vertexCount);
        byteCount += sprintf(txtData + byteCount, "# Triangle Count:   %i\n\n", mesh.triangleCount);

        byteCount += sprintf(txtData + byteCount, "g mesh\n");

        for (int i = 0, v = 0; i < mesh.vertexCount; i++, v += 3)
        {
            byteCount += sprintf(txtData + byteCount, "v %.2f %.2f %.2f\n", mesh.vertices[v], mesh.vertices[v + 1], mesh.vertices[v + 2]);
        }

        for (int i = 0, v = 0; i < mesh.vertexCount; i++, v += 2)
        {
            byteCount += sprintf(txtData + byteCount, "vt %.3f %.3f\n", mesh.texcoords[v], mesh.texcoords[v + 1]);
        }

        for (int i = 0, v = 0; i < mesh.vertexCount; i++, v += 3)
        {
            byteCount += sprintf(txtData + byteCount, "vn %.3f %.3f %.3f\n", mesh.normals[v], mesh.normals[v + 1], mesh.normals[v + 2]);
        }

        if (mesh.indices != NULL)
        {
            for (int i = 0, v = 0; i < mesh.triangleCount; i++, v += 3)
            {
                byteCount += sprintf(txtData + byteCount, "f %i/%i/%i %i/%i/%i %i/%i/%i\n",
                    mesh.indices[v] + 1, mesh.indices[v] + 1, mesh.indices[v] + 1,
                    mesh.indices[v + 1] + 1, mesh.indices[v + 1] + 1, mesh.indices[v + 1] + 1,
                    mesh.indices[v + 2] + 1, mesh.indices[v + 2] + 1, mesh.indices[v + 2] + 1);
            }
        }
        else
        {
            for (int i = 0, v = 1; i < mesh.triangleCount; i++, v += 3)
            {
                byteCount += sprintf(txtData + byteCount, "f %i/%i/%i %i/%i/%i %i/%i/%i\n", v, v, v, v + 1, v + 1, v + 1, v + 2, v + 2, v + 2);
            }
        }

        byteCount += sprintf(txtData + byteCount, "\n");

        // NOTE: Text data length exported is determined by '\0' (NULL) character
        success = SaveFileText(fileName, txtData);

        RL_FREE(txtData);
    }
    else if (IsFileExtension(fileName, ".raw"))
    {
        // TODO: Support additional file formats to export mesh vertex data
    }

    return success;
}

// Load materials from model file
Material *LoadMaterials(const char *fileName, int *materialCount)
{
    Material *materials = NULL;
    unsigned int count = 0;

    // TODO: Support IQM and GLTF for materials parsing

#if defined(SUPPORT_FILEFORMAT_MTL)
    if (IsFileExtension(fileName, ".mtl"))
    {
        tinyobj_material_t *mats = NULL;

        int result = tinyobj_parse_mtl_file(&mats, &count, fileName);
        if (result != TINYOBJ_SUCCESS) TRACELOG(LOG_WARNING, "MATERIAL: [%s] Failed to parse materials file", fileName);

        // TODO: Process materials to return

        tinyobj_materials_free(mats, count);
    }
#else
    TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to load material file", fileName);
#endif

    // Set materials shader to default (DIFFUSE, SPECULAR, NORMAL)
    if (materials != NULL)
    {
        for (unsigned int i = 0; i < count; i++)
        {
            materials[i].shader.id = rlGetShaderIdDefault();
            materials[i].shader.locs = rlGetShaderLocsDefault();
        }
    }

    *materialCount = count;
    return materials;
}

// Load default material (Supports: DIFFUSE, SPECULAR, NORMAL maps)
Material LoadMaterialDefault(void)
{
    Material material = { 0 };
    material.maps = (MaterialMap *)RL_CALLOC(MAX_MATERIAL_MAPS, sizeof(MaterialMap));

    // Using rlgl default shader
    material.shader.id = rlGetShaderIdDefault();
    material.shader.locs = rlGetShaderLocsDefault();

    // Using rlgl default texture (1x1 pixel, UNCOMPRESSED_R8G8B8A8, 1 mipmap)
    material.maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    //material.maps[MATERIAL_MAP_NORMAL].texture;         // NOTE: By default, not set
    //material.maps[MATERIAL_MAP_SPECULAR].texture;       // NOTE: By default, not set

    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;    // Diffuse color
    material.maps[MATERIAL_MAP_SPECULAR].color = WHITE;   // Specular color

    return material;
}

// Unload material from memory
void UnloadMaterial(Material material)
{
    // Unload material shader (avoid unloading default shader, managed by raylib)
    if (material.shader.id != rlGetShaderIdDefault()) UnloadShader(material.shader);

    // Unload loaded texture maps (avoid unloading default texture, managed by raylib)
    if (material.maps != NULL)
    {
        for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
        {
            if (material.maps[i].texture.id != rlGetTextureIdDefault()) rlUnloadTexture(material.maps[i].texture.id);
        }
    }

    RL_FREE(material.maps);
}

// Set texture for a material map type (MATERIAL_MAP_DIFFUSE, MATERIAL_MAP_SPECULAR...)
// NOTE: Previous texture should be manually unloaded
void SetMaterialTexture(Material *material, int mapType, Texture2D texture)
{
    material->maps[mapType].texture = texture;
}

// Set the material for a mesh
void SetModelMeshMaterial(Model *model, int meshId, int materialId)
{
    if (meshId >= model->meshCount) TRACELOG(LOG_WARNING, "MESH: Id greater than mesh count");
    else if (materialId >= model->materialCount) TRACELOG(LOG_WARNING, "MATERIAL: Id greater than material count");
    else  model->meshMaterial[meshId] = materialId;
}

// Load model animations from file
ModelAnimation *LoadModelAnimations(const char *fileName, unsigned int *animCount)
{
    ModelAnimation *animations = NULL;

#if defined(SUPPORT_FILEFORMAT_IQM)
    if (IsFileExtension(fileName, ".iqm")) animations = LoadModelAnimationsIQM(fileName, animCount);
#endif
#if defined(SUPPORT_FILEFORMAT_GLTF)
    //if (IsFileExtension(fileName, ".gltf;.glb")) animations = LoadModelAnimationGLTF(fileName, animCount);
#endif

    return animations;
}

// Update model animated vertex data (positions and normals) for a given frame
// NOTE: Updated data is uploaded to GPU
void UpdateModelAnimation(Model model, ModelAnimation anim, int frame)
{
    if ((anim.frameCount > 0) && (anim.bones != NULL) && (anim.framePoses != NULL))
    {
        if (frame >= anim.frameCount) frame = frame%anim.frameCount;

        for (int m = 0; m < model.meshCount; m++)
        {
            Mesh mesh = model.meshes[m];
            if (mesh.boneIds == NULL || mesh.boneWeights == NULL)
            {
                TRACELOG(LOG_WARNING, "MODEL: UpdateModelAnimation Mesh %i has no connection to bones",m);
                continue;
            }

            bool updated = false; // set to true when anim vertex information is updated
            Vector3 animVertex = { 0 };
            Vector3 animNormal = { 0 };

            Vector3 inTranslation = { 0 };
            Quaternion inRotation = { 0 };
            // Vector3 inScale = { 0 };

            Vector3 outTranslation = { 0 };
            Quaternion outRotation = { 0 };
            Vector3 outScale = { 0 };

            int boneId = 0;
            int boneCounter = 0;
            float boneWeight = 0.0;

            const int vValues = mesh.vertexCount*3;
            for (int vCounter = 0; vCounter < vValues; vCounter+=3)
            {
                mesh.animVertices[vCounter] = 0;
                mesh.animVertices[vCounter + 1] = 0;
                mesh.animVertices[vCounter + 2] = 0;

                if (mesh.animNormals!=NULL)
                {
                    mesh.animNormals[vCounter] = 0;
                    mesh.animNormals[vCounter + 1] = 0;
                    mesh.animNormals[vCounter + 2] = 0;
                }

                // Iterates over 4 bones per vertex
                for (int j = 0; j < 4; j++, boneCounter++)
                {
                    boneWeight = mesh.boneWeights[boneCounter];
                    // early stop when no transformation will be applied
                    if (boneWeight == 0.0f)
                    {
                        continue;
                    }
                    boneId = mesh.boneIds[boneCounter];
                    //int boneIdParent = model.bones[boneId].parent;
                    inTranslation = model.bindPose[boneId].translation;
                    inRotation = model.bindPose[boneId].rotation;
                    // inScale = model.bindPose[boneId].scale;
                    outTranslation = anim.framePoses[frame][boneId].translation;
                    outRotation = anim.framePoses[frame][boneId].rotation;
                    outScale = anim.framePoses[frame][boneId].scale;

                    // Vertices processing
                    // NOTE: We use meshes.vertices (default vertex position) to calculate meshes.animVertices (animated vertex position)
                    animVertex = (Vector3){ mesh.vertices[vCounter], mesh.vertices[vCounter + 1], mesh.vertices[vCounter + 2] };
                    animVertex = Vector3Multiply(animVertex, outScale);
                    animVertex = Vector3Subtract(animVertex, inTranslation);
                    animVertex = Vector3RotateByQuaternion(animVertex, QuaternionMultiply(outRotation, QuaternionInvert(inRotation)));
                    animVertex = Vector3Add(animVertex, outTranslation);
//                     animVertex = Vector3Transform(animVertex, model.transform);
                    mesh.animVertices[vCounter] += animVertex.x*boneWeight;
                    mesh.animVertices[vCounter + 1] += animVertex.y*boneWeight;
                    mesh.animVertices[vCounter + 2] += animVertex.z*boneWeight;
                    updated = true;

                    // Normals processing
                    // NOTE: We use meshes.baseNormals (default normal) to calculate meshes.normals (animated normals)
                    if (mesh.normals != NULL)
                    {
                        animNormal = (Vector3){ mesh.normals[vCounter], mesh.normals[vCounter + 1], mesh.normals[vCounter + 2] };
                        animNormal = Vector3RotateByQuaternion(animNormal, QuaternionMultiply(outRotation, QuaternionInvert(inRotation)));
                        mesh.animNormals[vCounter] += animNormal.x*boneWeight;
                        mesh.animNormals[vCounter + 1] += animNormal.y*boneWeight;
                        mesh.animNormals[vCounter + 2] += animNormal.z*boneWeight;
                    }
                }
            }

            // Upload new vertex data to GPU for model drawing
            // Only update data when values changed.
            if (updated){
                rlUpdateVertexBuffer(mesh.vboId[0], mesh.animVertices, mesh.vertexCount*3*sizeof(float), 0);    // Update vertex position
                rlUpdateVertexBuffer(mesh.vboId[2], mesh.animNormals, mesh.vertexCount*3*sizeof(float), 0);     // Update vertex normals
            }
        }
    }
}

// Unload animation array data
void UnloadModelAnimations(ModelAnimation *animations, unsigned int count)
{
    for (unsigned int i = 0; i < count; i++) UnloadModelAnimation(animations[i]);
    RL_FREE(animations);
}

// Unload animation data
void UnloadModelAnimation(ModelAnimation anim)
{
    for (int i = 0; i < anim.frameCount; i++) RL_FREE(anim.framePoses[i]);

    RL_FREE(anim.bones);
    RL_FREE(anim.framePoses);
}

// Check model animation skeleton match
// NOTE: Only number of bones and parent connections are checked
bool IsModelAnimationValid(Model model, ModelAnimation anim)
{
    int result = true;

    if (model.boneCount != anim.boneCount) result = false;
    else
    {
        for (int i = 0; i < model.boneCount; i++)
        {
            if (model.bones[i].parent != anim.bones[i].parent) { result = false; break; }
        }
    }

    return result;
}

#if defined(SUPPORT_MESH_GENERATION)
// Generate polygonal mesh
Mesh GenMeshPoly(int sides, float radius)
{
    Mesh mesh = { 0 };

    if (sides < 3) return mesh;

    int vertexCount = sides*3;

    // Vertices definition
    Vector3 *vertices = (Vector3 *)RL_MALLOC(vertexCount*sizeof(Vector3));

    float d = 0.0f, dStep = 360.0f/sides;
    for (int v = 0; v < vertexCount; v += 3)
    {
        vertices[v] = (Vector3){ 0.0f, 0.0f, 0.0f };
        vertices[v + 1] = (Vector3){ sinf(DEG2RAD*d)*radius, 0.0f, cosf(DEG2RAD*d)*radius };
        vertices[v + 2] = (Vector3){sinf(DEG2RAD*(d+dStep))*radius, 0.0f, cosf(DEG2RAD*(d+dStep))*radius };
        d += dStep;
    }

    // Normals definition
    Vector3 *normals = (Vector3 *)RL_MALLOC(vertexCount*sizeof(Vector3));
    for (int n = 0; n < vertexCount; n++) normals[n] = (Vector3){ 0.0f, 1.0f, 0.0f };   // Vector3.up;

    // TexCoords definition
    Vector2 *texcoords = (Vector2 *)RL_MALLOC(vertexCount*sizeof(Vector2));
    for (int n = 0; n < vertexCount; n++) texcoords[n] = (Vector2){ 0.0f, 0.0f };

    mesh.vertexCount = vertexCount;
    mesh.triangleCount = sides;
    mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount*2*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));

    // Mesh vertices position array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.vertices[3*i] = vertices[i].x;
        mesh.vertices[3*i + 1] = vertices[i].y;
        mesh.vertices[3*i + 2] = vertices[i].z;
    }

    // Mesh texcoords array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.texcoords[2*i] = texcoords[i].x;
        mesh.texcoords[2*i + 1] = texcoords[i].y;
    }

    // Mesh normals array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.normals[3*i] = normals[i].x;
        mesh.normals[3*i + 1] = normals[i].y;
        mesh.normals[3*i + 2] = normals[i].z;
    }

    RL_FREE(vertices);
    RL_FREE(normals);
    RL_FREE(texcoords);

    // Upload vertex data to GPU (static mesh)
    // NOTE: mesh.vboId array is allocated inside UploadMesh()
    UploadMesh(&mesh, false);

    return mesh;
}

// Generate plane mesh (with subdivisions)
Mesh GenMeshPlane(float width, float length, int resX, int resZ)
{
    Mesh mesh = { 0 };

#define CUSTOM_MESH_GEN_PLANE
#if defined(CUSTOM_MESH_GEN_PLANE)
    resX++;
    resZ++;

    // Vertices definition
    int vertexCount = resX*resZ; // vertices get reused for the faces

    Vector3 *vertices = (Vector3 *)RL_MALLOC(vertexCount*sizeof(Vector3));
    for (int z = 0; z < resZ; z++)
    {
        // [-length/2, length/2]
        float zPos = ((float)z/(resZ - 1) - 0.5f)*length;
        for (int x = 0; x < resX; x++)
        {
            // [-width/2, width/2]
            float xPos = ((float)x/(resX - 1) - 0.5f)*width;
            vertices[x + z*resX] = (Vector3){ xPos, 0.0f, zPos };
        }
    }

    // Normals definition
    Vector3 *normals = (Vector3 *)RL_MALLOC(vertexCount*sizeof(Vector3));
    for (int n = 0; n < vertexCount; n++) normals[n] = (Vector3){ 0.0f, 1.0f, 0.0f };   // Vector3.up;

    // TexCoords definition
    Vector2 *texcoords = (Vector2 *)RL_MALLOC(vertexCount*sizeof(Vector2));
    for (int v = 0; v < resZ; v++)
    {
        for (int u = 0; u < resX; u++)
        {
            texcoords[u + v*resX] = (Vector2){ (float)u/(resX - 1), (float)v/(resZ - 1) };
        }
    }

    // Triangles definition (indices)
    int numFaces = (resX - 1)*(resZ - 1);
    int *triangles = (int *)RL_MALLOC(numFaces*6*sizeof(int));
    int t = 0;
    for (int face = 0; face < numFaces; face++)
    {
        // Retrieve lower left corner from face ind
        int i = face % (resX - 1) + (face/(resZ - 1)*resX);

        triangles[t++] = i + resX;
        triangles[t++] = i + 1;
        triangles[t++] = i;

        triangles[t++] = i + resX;
        triangles[t++] = i + resX + 1;
        triangles[t++] = i + 1;
    }

    mesh.vertexCount = vertexCount;
    mesh.triangleCount = numFaces*2;
    mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount*2*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.indices = (unsigned short *)RL_MALLOC(mesh.triangleCount*3*sizeof(unsigned short));

    // Mesh vertices position array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.vertices[3*i] = vertices[i].x;
        mesh.vertices[3*i + 1] = vertices[i].y;
        mesh.vertices[3*i + 2] = vertices[i].z;
    }

    // Mesh texcoords array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.texcoords[2*i] = texcoords[i].x;
        mesh.texcoords[2*i + 1] = texcoords[i].y;
    }

    // Mesh normals array
    for (int i = 0; i < mesh.vertexCount; i++)
    {
        mesh.normals[3*i] = normals[i].x;
        mesh.normals[3*i + 1] = normals[i].y;
        mesh.normals[3*i + 2] = normals[i].z;
    }

    // Mesh indices array initialization
    for (int i = 0; i < mesh.triangleCount*3; i++) mesh.indices[i] = triangles[i];

    RL_FREE(vertices);
    RL_FREE(normals);
    RL_FREE(texcoords);
    RL_FREE(triangles);

#else       // Use par_shapes library to generate plane mesh

    par_shapes_mesh *plane = par_shapes_create_plane(resX, resZ);   // No normals/texcoords generated!!!
    par_shapes_scale(plane, width, length, 1.0f);
    par_shapes_rotate(plane, -PI/2.0f, (float[]){ 1, 0, 0 });
    par_shapes_translate(plane, -width/2, 0.0f, length/2);

    mesh.vertices = (float *)RL_MALLOC(plane->ntriangles*3*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(plane->ntriangles*3*2*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(plane->ntriangles*3*3*sizeof(float));

    mesh.vertexCount = plane->ntriangles*3;
    mesh.triangleCount = plane->ntriangles;

    for (int k = 0; k < mesh.vertexCount; k++)
    {
        mesh.vertices[k*3] = plane->points[plane->triangles[k]*3];
        mesh.vertices[k*3 + 1] = plane->points[plane->triangles[k]*3 + 1];
        mesh.vertices[k*3 + 2] = plane->points[plane->triangles[k]*3 + 2];

        mesh.normals[k*3] = plane->normals[plane->triangles[k]*3];
        mesh.normals[k*3 + 1] = plane->normals[plane->triangles[k]*3 + 1];
        mesh.normals[k*3 + 2] = plane->normals[plane->triangles[k]*3 + 2];

        mesh.texcoords[k*2] = plane->tcoords[plane->triangles[k]*2];
        mesh.texcoords[k*2 + 1] = plane->tcoords[plane->triangles[k]*2 + 1];
    }

    par_shapes_free_mesh(plane);
#endif

    // Upload vertex data to GPU (static mesh)
    UploadMesh(&mesh, false);

    return mesh;
}

// Generated cuboid mesh
Mesh GenMeshCube(float width, float height, float length)
{
    Mesh mesh = { 0 };

#define CUSTOM_MESH_GEN_CUBE
#if defined(CUSTOM_MESH_GEN_CUBE)
    float vertices[] = {
        -width/2, -height/2, length/2,
        width/2, -height/2, length/2,
        width/2, height/2, length/2,
        -width/2, height/2, length/2,
        -width/2, -height/2, -length/2,
        -width/2, height/2, -length/2,
        width/2, height/2, -length/2,
        width/2, -height/2, -length/2,
        -width/2, height/2, -length/2,
        -width/2, height/2, length/2,
        width/2, height/2, length/2,
        width/2, height/2, -length/2,
        -width/2, -height/2, -length/2,
        width/2, -height/2, -length/2,
        width/2, -height/2, length/2,
        -width/2, -height/2, length/2,
        width/2, -height/2, -length/2,
        width/2, height/2, -length/2,
        width/2, height/2, length/2,
        width/2, -height/2, length/2,
        -width/2, -height/2, -length/2,
        -width/2, -height/2, length/2,
        -width/2, height/2, length/2,
        -width/2, height/2, -length/2
    };

    float texcoords[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

    float normals[] = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 0.0f,-1.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        0.0f,-1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f
    };

    mesh.vertices = (float *)RL_MALLOC(24*3*sizeof(float));
    memcpy(mesh.vertices, vertices, 24*3*sizeof(float));

    mesh.texcoords = (float *)RL_MALLOC(24*2*sizeof(float));
    memcpy(mesh.texcoords, texcoords, 24*2*sizeof(float));

    mesh.normals = (float *)RL_MALLOC(24*3*sizeof(float));
    memcpy(mesh.normals, normals, 24*3*sizeof(float));

    mesh.indices = (unsigned short *)RL_MALLOC(36*sizeof(unsigned short));

    int k = 0;

    // Indices can be initialized right now
    for (int i = 0; i < 36; i += 6)
    {
        mesh.indices[i] = 4*k;
        mesh.indices[i + 1] = 4*k + 1;
        mesh.indices[i + 2] = 4*k + 2;
        mesh.indices[i + 3] = 4*k;
        mesh.indices[i + 4] = 4*k + 2;
        mesh.indices[i + 5] = 4*k + 3;

        k++;
    }

    mesh.vertexCount = 24;
    mesh.triangleCount = 12;

#else               // Use par_shapes library to generate cube mesh
/*
// Platonic solids:
par_shapes_mesh* par_shapes_create_tetrahedron();       // 4 sides polyhedron (pyramid)
par_shapes_mesh* par_shapes_create_cube();              // 6 sides polyhedron (cube)
par_shapes_mesh* par_shapes_create_octahedron();        // 8 sides polyhedron (dyamond)
par_shapes_mesh* par_shapes_create_dodecahedron();      // 12 sides polyhedron
par_shapes_mesh* par_shapes_create_icosahedron();       // 20 sides polyhedron
*/
    // Platonic solid generation: cube (6 sides)
    // NOTE: No normals/texcoords generated by default
    par_shapes_mesh *cube = par_shapes_create_cube();
    cube->tcoords = PAR_MALLOC(float, 2*cube->npoints);
    for (int i = 0; i < 2*cube->npoints; i++) cube->tcoords[i] = 0.0f;
    par_shapes_scale(cube, width, height, length);
    par_shapes_translate(cube, -width/2, 0.0f, -length/2);
    par_shapes_compute_normals(cube);

    mesh.vertices = (float *)RL_MALLOC(cube->ntriangles*3*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(cube->ntriangles*3*2*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(cube->ntriangles*3*3*sizeof(float));

    mesh.vertexCount = cube->ntriangles*3;
    mesh.triangleCount = cube->ntriangles;

    for (int k = 0; k < mesh.vertexCount; k++)
    {
        mesh.vertices[k*3] = cube->points[cube->triangles[k]*3];
        mesh.vertices[k*3 + 1] = cube->points[cube->triangles[k]*3 + 1];
        mesh.vertices[k*3 + 2] = cube->points[cube->triangles[k]*3 + 2];

        mesh.normals[k*3] = cube->normals[cube->triangles[k]*3];
        mesh.normals[k*3 + 1] = cube->normals[cube->triangles[k]*3 + 1];
        mesh.normals[k*3 + 2] = cube->normals[cube->triangles[k]*3 + 2];

        mesh.texcoords[k*2] = cube->tcoords[cube->triangles[k]*2];
        mesh.texcoords[k*2 + 1] = cube->tcoords[cube->triangles[k]*2 + 1];
    }

    par_shapes_free_mesh(cube);
#endif

    // Upload vertex data to GPU (static mesh)
    UploadMesh(&mesh, false);

    return mesh;
}

// Generate sphere mesh (standard sphere)
Mesh GenMeshSphere(float radius, int rings, int slices)
{
    Mesh mesh = { 0 };

    if ((rings >= 3) && (slices >= 3))
    {
        par_shapes_mesh *sphere = par_shapes_create_parametric_sphere(slices, rings);
        par_shapes_scale(sphere, radius, radius, radius);
        // NOTE: Soft normals are computed internally

        mesh.vertices = (float *)RL_MALLOC(sphere->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(sphere->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(sphere->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = sphere->ntriangles*3;
        mesh.triangleCount = sphere->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = sphere->points[sphere->triangles[k]*3];
            mesh.vertices[k*3 + 1] = sphere->points[sphere->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = sphere->points[sphere->triangles[k]*3 + 2];

            mesh.normals[k*3] = sphere->normals[sphere->triangles[k]*3];
            mesh.normals[k*3 + 1] = sphere->normals[sphere->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = sphere->normals[sphere->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = sphere->tcoords[sphere->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = sphere->tcoords[sphere->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(sphere);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: sphere");

    return mesh;
}

// Generate hemi-sphere mesh (half sphere, no bottom cap)
Mesh GenMeshHemiSphere(float radius, int rings, int slices)
{
    Mesh mesh = { 0 };

    if ((rings >= 3) && (slices >= 3))
    {
        if (radius < 0.0f) radius = 0.0f;

        par_shapes_mesh *sphere = par_shapes_create_hemisphere(slices, rings);
        par_shapes_scale(sphere, radius, radius, radius);
        // NOTE: Soft normals are computed internally

        mesh.vertices = (float *)RL_MALLOC(sphere->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(sphere->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(sphere->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = sphere->ntriangles*3;
        mesh.triangleCount = sphere->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = sphere->points[sphere->triangles[k]*3];
            mesh.vertices[k*3 + 1] = sphere->points[sphere->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = sphere->points[sphere->triangles[k]*3 + 2];

            mesh.normals[k*3] = sphere->normals[sphere->triangles[k]*3];
            mesh.normals[k*3 + 1] = sphere->normals[sphere->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = sphere->normals[sphere->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = sphere->tcoords[sphere->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = sphere->tcoords[sphere->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(sphere);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: hemisphere");

    return mesh;
}

// Generate cylinder mesh
Mesh GenMeshCylinder(float radius, float height, int slices)
{
    Mesh mesh = { 0 };

    if (slices >= 3)
    {
        // Instance a cylinder that sits on the Z=0 plane using the given tessellation
        // levels across the UV domain.  Think of "slices" like a number of pizza
        // slices, and "stacks" like a number of stacked rings.
        // Height and radius are both 1.0, but they can easily be changed with par_shapes_scale
        par_shapes_mesh *cylinder = par_shapes_create_cylinder(slices, 8);
        par_shapes_scale(cylinder, radius, radius, height);
        par_shapes_rotate(cylinder, -PI/2.0f, (float[]){ 1, 0, 0 });

        // Generate an orientable disk shape (top cap)
        par_shapes_mesh *capTop = par_shapes_create_disk(radius, slices, (float[]){ 0, 0, 0 }, (float[]){ 0, 0, 1 });
        capTop->tcoords = PAR_MALLOC(float, 2*capTop->npoints);
        for (int i = 0; i < 2*capTop->npoints; i++) capTop->tcoords[i] = 0.0f;
        par_shapes_rotate(capTop, -PI/2.0f, (float[]){ 1, 0, 0 });
        par_shapes_rotate(capTop, 90*DEG2RAD, (float[]){ 0, 1, 0 });
        par_shapes_translate(capTop, 0, height, 0);

        // Generate an orientable disk shape (bottom cap)
        par_shapes_mesh *capBottom = par_shapes_create_disk(radius, slices, (float[]){ 0, 0, 0 }, (float[]){ 0, 0, -1 });
        capBottom->tcoords = PAR_MALLOC(float, 2*capBottom->npoints);
        for (int i = 0; i < 2*capBottom->npoints; i++) capBottom->tcoords[i] = 0.95f;
        par_shapes_rotate(capBottom, PI/2.0f, (float[]){ 1, 0, 0 });
        par_shapes_rotate(capBottom, -90*DEG2RAD, (float[]){ 0, 1, 0 });

        par_shapes_merge_and_free(cylinder, capTop);
        par_shapes_merge_and_free(cylinder, capBottom);

        mesh.vertices = (float *)RL_MALLOC(cylinder->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(cylinder->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(cylinder->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = cylinder->ntriangles*3;
        mesh.triangleCount = cylinder->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = cylinder->points[cylinder->triangles[k]*3];
            mesh.vertices[k*3 + 1] = cylinder->points[cylinder->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = cylinder->points[cylinder->triangles[k]*3 + 2];

            mesh.normals[k*3] = cylinder->normals[cylinder->triangles[k]*3];
            mesh.normals[k*3 + 1] = cylinder->normals[cylinder->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = cylinder->normals[cylinder->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = cylinder->tcoords[cylinder->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = cylinder->tcoords[cylinder->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(cylinder);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: cylinder");

    return mesh;
}

// Generate cone/pyramid mesh
Mesh GenMeshCone(float radius, float height, int slices)
{
    Mesh mesh = { 0 };

    if (slices >= 3)
    {
        // Instance a cone that sits on the Z=0 plane using the given tessellation
        // levels across the UV domain.  Think of "slices" like a number of pizza
        // slices, and "stacks" like a number of stacked rings.
        // Height and radius are both 1.0, but they can easily be changed with par_shapes_scale
        par_shapes_mesh *cone = par_shapes_create_cone(slices, 8);
        par_shapes_scale(cone, radius, radius, height);
        par_shapes_rotate(cone, -PI/2.0f, (float[]){ 1, 0, 0 });
        par_shapes_rotate(cone, PI/2.0f, (float[]){ 0, 1, 0 });

        // Generate an orientable disk shape (bottom cap)
        par_shapes_mesh *capBottom = par_shapes_create_disk(radius, slices, (float[]){ 0, 0, 0 }, (float[]){ 0, 0, -1 });
        capBottom->tcoords = PAR_MALLOC(float, 2*capBottom->npoints);
        for (int i = 0; i < 2*capBottom->npoints; i++) capBottom->tcoords[i] = 0.95f;
        par_shapes_rotate(capBottom, PI/2.0f, (float[]){ 1, 0, 0 });

        par_shapes_merge_and_free(cone, capBottom);

        mesh.vertices = (float *)RL_MALLOC(cone->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(cone->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(cone->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = cone->ntriangles*3;
        mesh.triangleCount = cone->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = cone->points[cone->triangles[k]*3];
            mesh.vertices[k*3 + 1] = cone->points[cone->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = cone->points[cone->triangles[k]*3 + 2];

            mesh.normals[k*3] = cone->normals[cone->triangles[k]*3];
            mesh.normals[k*3 + 1] = cone->normals[cone->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = cone->normals[cone->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = cone->tcoords[cone->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = cone->tcoords[cone->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(cone);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: cone");

    return mesh;
}

// Generate torus mesh
Mesh GenMeshTorus(float radius, float size, int radSeg, int sides)
{
    Mesh mesh = { 0 };

    if ((sides >= 3) && (radSeg >= 3))
    {
        if (radius > 1.0f) radius = 1.0f;
        else if (radius < 0.1f) radius = 0.1f;

        // Create a donut that sits on the Z=0 plane with the specified inner radius
        // The outer radius can be controlled with par_shapes_scale
        par_shapes_mesh *torus = par_shapes_create_torus(radSeg, sides, radius);
        par_shapes_scale(torus, size/2, size/2, size/2);

        mesh.vertices = (float *)RL_MALLOC(torus->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(torus->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(torus->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = torus->ntriangles*3;
        mesh.triangleCount = torus->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = torus->points[torus->triangles[k]*3];
            mesh.vertices[k*3 + 1] = torus->points[torus->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = torus->points[torus->triangles[k]*3 + 2];

            mesh.normals[k*3] = torus->normals[torus->triangles[k]*3];
            mesh.normals[k*3 + 1] = torus->normals[torus->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = torus->normals[torus->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = torus->tcoords[torus->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = torus->tcoords[torus->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(torus);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: torus");

    return mesh;
}

// Generate trefoil knot mesh
Mesh GenMeshKnot(float radius, float size, int radSeg, int sides)
{
    Mesh mesh = { 0 };

    if ((sides >= 3) && (radSeg >= 3))
    {
        if (radius > 3.0f) radius = 3.0f;
        else if (radius < 0.5f) radius = 0.5f;

        par_shapes_mesh *knot = par_shapes_create_trefoil_knot(radSeg, sides, radius);
        par_shapes_scale(knot, size, size, size);

        mesh.vertices = (float *)RL_MALLOC(knot->ntriangles*3*3*sizeof(float));
        mesh.texcoords = (float *)RL_MALLOC(knot->ntriangles*3*2*sizeof(float));
        mesh.normals = (float *)RL_MALLOC(knot->ntriangles*3*3*sizeof(float));

        mesh.vertexCount = knot->ntriangles*3;
        mesh.triangleCount = knot->ntriangles;

        for (int k = 0; k < mesh.vertexCount; k++)
        {
            mesh.vertices[k*3] = knot->points[knot->triangles[k]*3];
            mesh.vertices[k*3 + 1] = knot->points[knot->triangles[k]*3 + 1];
            mesh.vertices[k*3 + 2] = knot->points[knot->triangles[k]*3 + 2];

            mesh.normals[k*3] = knot->normals[knot->triangles[k]*3];
            mesh.normals[k*3 + 1] = knot->normals[knot->triangles[k]*3 + 1];
            mesh.normals[k*3 + 2] = knot->normals[knot->triangles[k]*3 + 2];

            mesh.texcoords[k*2] = knot->tcoords[knot->triangles[k]*2];
            mesh.texcoords[k*2 + 1] = knot->tcoords[knot->triangles[k]*2 + 1];
        }

        par_shapes_free_mesh(knot);

        // Upload vertex data to GPU (static mesh)
        UploadMesh(&mesh, false);
    }
    else TRACELOG(LOG_WARNING, "MESH: Failed to generate mesh: knot");

    return mesh;
}

// Generate a mesh from heightmap
// NOTE: Vertex data is uploaded to GPU
Mesh GenMeshHeightmap(Image heightmap, Vector3 size)
{
    #define GRAY_VALUE(c) ((c.r+c.g+c.b)/3)

    Mesh mesh = { 0 };

    int mapX = heightmap.width;
    int mapZ = heightmap.height;

    Color *pixels = LoadImageColors(heightmap);

    // NOTE: One vertex per pixel
    mesh.triangleCount = (mapX-1)*(mapZ-1)*2;    // One quad every four pixels

    mesh.vertexCount = mesh.triangleCount*3;

    mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount*2*sizeof(float));
    mesh.colors = NULL;

    int vCounter = 0;       // Used to count vertices float by float
    int tcCounter = 0;      // Used to count texcoords float by float
    int nCounter = 0;       // Used to count normals float by float

    int trisCounter = 0;

    Vector3 scaleFactor = { size.x/mapX, size.y/255.0f, size.z/mapZ };

    Vector3 vA = { 0 };
    Vector3 vB = { 0 };
    Vector3 vC = { 0 };
    Vector3 vN = { 0 };

    for (int z = 0; z < mapZ-1; z++)
    {
        for (int x = 0; x < mapX-1; x++)
        {
            // Fill vertices array with data
            //----------------------------------------------------------

            // one triangle - 3 vertex
            mesh.vertices[vCounter] = (float)x*scaleFactor.x;
            mesh.vertices[vCounter + 1] = (float)GRAY_VALUE(pixels[x + z*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 2] = (float)z*scaleFactor.z;

            mesh.vertices[vCounter + 3] = (float)x*scaleFactor.x;
            mesh.vertices[vCounter + 4] = (float)GRAY_VALUE(pixels[x + (z + 1)*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 5] = (float)(z + 1)*scaleFactor.z;

            mesh.vertices[vCounter + 6] = (float)(x + 1)*scaleFactor.x;
            mesh.vertices[vCounter + 7] = (float)GRAY_VALUE(pixels[(x + 1) + z*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 8] = (float)z*scaleFactor.z;

            // another triangle - 3 vertex
            mesh.vertices[vCounter + 9] = mesh.vertices[vCounter + 6];
            mesh.vertices[vCounter + 10] = mesh.vertices[vCounter + 7];
            mesh.vertices[vCounter + 11] = mesh.vertices[vCounter + 8];

            mesh.vertices[vCounter + 12] = mesh.vertices[vCounter + 3];
            mesh.vertices[vCounter + 13] = mesh.vertices[vCounter + 4];
            mesh.vertices[vCounter + 14] = mesh.vertices[vCounter + 5];

            mesh.vertices[vCounter + 15] = (float)(x + 1)*scaleFactor.x;
            mesh.vertices[vCounter + 16] = (float)GRAY_VALUE(pixels[(x + 1) + (z + 1)*mapX])*scaleFactor.y;
            mesh.vertices[vCounter + 17] = (float)(z + 1)*scaleFactor.z;
            vCounter += 18;     // 6 vertex, 18 floats

            // Fill texcoords array with data
            //--------------------------------------------------------------
            mesh.texcoords[tcCounter] = (float)x/(mapX - 1);
            mesh.texcoords[tcCounter + 1] = (float)z/(mapZ - 1);

            mesh.texcoords[tcCounter + 2] = (float)x/(mapX - 1);
            mesh.texcoords[tcCounter + 3] = (float)(z + 1)/(mapZ - 1);

            mesh.texcoords[tcCounter + 4] = (float)(x + 1)/(mapX - 1);
            mesh.texcoords[tcCounter + 5] = (float)z/(mapZ - 1);

            mesh.texcoords[tcCounter + 6] = mesh.texcoords[tcCounter + 4];
            mesh.texcoords[tcCounter + 7] = mesh.texcoords[tcCounter + 5];

            mesh.texcoords[tcCounter + 8] = mesh.texcoords[tcCounter + 2];
            mesh.texcoords[tcCounter + 9] = mesh.texcoords[tcCounter + 3];

            mesh.texcoords[tcCounter + 10] = (float)(x + 1)/(mapX - 1);
            mesh.texcoords[tcCounter + 11] = (float)(z + 1)/(mapZ - 1);
            tcCounter += 12;    // 6 texcoords, 12 floats

            // Fill normals array with data
            //--------------------------------------------------------------
            for (int i = 0; i < 18; i += 9)
            {
                vA.x = mesh.vertices[nCounter + i];
                vA.y = mesh.vertices[nCounter + i + 1];
                vA.z = mesh.vertices[nCounter + i + 2];

                vB.x = mesh.vertices[nCounter + i + 3];
                vB.y = mesh.vertices[nCounter + i + 4];
                vB.z = mesh.vertices[nCounter + i + 5];

                vC.x = mesh.vertices[nCounter + i + 6];
                vC.y = mesh.vertices[nCounter + i + 7];
                vC.z = mesh.vertices[nCounter + i + 8];

                vN = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(vB, vA), Vector3Subtract(vC, vA)));

                mesh.normals[nCounter + i] = vN.x;
                mesh.normals[nCounter + i + 1] = vN.y;
                mesh.normals[nCounter + i + 2] = vN.z;

                mesh.normals[nCounter + i + 3] = vN.x;
                mesh.normals[nCounter + i + 4] = vN.y;
                mesh.normals[nCounter + i + 5] = vN.z;

                mesh.normals[nCounter + i + 6] = vN.x;
                mesh.normals[nCounter + i + 7] = vN.y;
                mesh.normals[nCounter + i + 8] = vN.z;
            }

            nCounter += 18;     // 6 vertex, 18 floats
            trisCounter += 2;
        }
    }

    UnloadImageColors(pixels);  // Unload pixels color data

    // Upload vertex data to GPU (static mesh)
    UploadMesh(&mesh, false);

    return mesh;
}

// Generate a cubes mesh from pixel data
// NOTE: Vertex data is uploaded to GPU
Mesh GenMeshCubicmap(Image cubicmap, Vector3 cubeSize)
{
    #define COLOR_EQUAL(col1, col2) ((col1.r == col2.r)&&(col1.g == col2.g)&&(col1.b == col2.b)&&(col1.a == col2.a))

    Mesh mesh = { 0 };

    Color *pixels = LoadImageColors(cubicmap);

    int mapWidth = cubicmap.width;
    int mapHeight = cubicmap.height;

    // NOTE: Max possible number of triangles numCubes*(12 triangles by cube)
    int maxTriangles = cubicmap.width*cubicmap.height*12;

    int vCounter = 0;       // Used to count vertices
    int tcCounter = 0;      // Used to count texcoords
    int nCounter = 0;       // Used to count normals

    float w = cubeSize.x;
    float h = cubeSize.z;
    float h2 = cubeSize.y;

    Vector3 *mapVertices = (Vector3 *)RL_MALLOC(maxTriangles*3*sizeof(Vector3));
    Vector2 *mapTexcoords = (Vector2 *)RL_MALLOC(maxTriangles*3*sizeof(Vector2));
    Vector3 *mapNormals = (Vector3 *)RL_MALLOC(maxTriangles*3*sizeof(Vector3));

    // Define the 6 normals of the cube, we will combine them accordingly later...
    Vector3 n1 = { 1.0f, 0.0f, 0.0f };
    Vector3 n2 = { -1.0f, 0.0f, 0.0f };
    Vector3 n3 = { 0.0f, 1.0f, 0.0f };
    Vector3 n4 = { 0.0f, -1.0f, 0.0f };
    Vector3 n5 = { 0.0f, 0.0f, -1.0f };
    Vector3 n6 = { 0.0f, 0.0f, 1.0f };

    // NOTE: We use texture rectangles to define different textures for top-bottom-front-back-right-left (6)
    typedef struct RectangleF {
        float x;
        float y;
        float width;
        float height;
    } RectangleF;

    RectangleF rightTexUV = { 0.0f, 0.0f, 0.5f, 0.5f };
    RectangleF leftTexUV = { 0.5f, 0.0f, 0.5f, 0.5f };
    RectangleF frontTexUV = { 0.0f, 0.0f, 0.5f, 0.5f };
    RectangleF backTexUV = { 0.5f, 0.0f, 0.5f, 0.5f };
    RectangleF topTexUV = { 0.0f, 0.5f, 0.5f, 0.5f };
    RectangleF bottomTexUV = { 0.5f, 0.5f, 0.5f, 0.5f };

    for (int z = 0; z < mapHeight; ++z)
    {
        for (int x = 0; x < mapWidth; ++x)
        {
            // Define the 8 vertex of the cube, we will combine them accordingly later...
            Vector3 v1 = { w*(x - 0.5f), h2, h*(z - 0.5f) };
            Vector3 v2 = { w*(x - 0.5f), h2, h*(z + 0.5f) };
            Vector3 v3 = { w*(x + 0.5f), h2, h*(z + 0.5f) };
            Vector3 v4 = { w*(x + 0.5f), h2, h*(z - 0.5f) };
            Vector3 v5 = { w*(x + 0.5f), 0, h*(z - 0.5f) };
            Vector3 v6 = { w*(x - 0.5f), 0, h*(z - 0.5f) };
            Vector3 v7 = { w*(x - 0.5f), 0, h*(z + 0.5f) };
            Vector3 v8 = { w*(x + 0.5f), 0, h*(z + 0.5f) };

            // We check pixel color to be WHITE -> draw full cube
            if (COLOR_EQUAL(pixels[z*cubicmap.width + x], WHITE))
            {
                // Define triangles and checking collateral cubes
                //------------------------------------------------

                // Define top triangles (2 tris, 6 vertex --> v1-v2-v3, v1-v3-v4)
                // WARNING: Not required for a WHITE cubes, created to allow seeing the map from outside
                mapVertices[vCounter] = v1;
                mapVertices[vCounter + 1] = v2;
                mapVertices[vCounter + 2] = v3;
                mapVertices[vCounter + 3] = v1;
                mapVertices[vCounter + 4] = v3;
                mapVertices[vCounter + 5] = v4;
                vCounter += 6;

                mapNormals[nCounter] = n3;
                mapNormals[nCounter + 1] = n3;
                mapNormals[nCounter + 2] = n3;
                mapNormals[nCounter + 3] = n3;
                mapNormals[nCounter + 4] = n3;
                mapNormals[nCounter + 5] = n3;
                nCounter += 6;

                mapTexcoords[tcCounter] = (Vector2){ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 1] = (Vector2){ topTexUV.x, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 2] = (Vector2){ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 3] = (Vector2){ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 4] = (Vector2){ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 5] = (Vector2){ topTexUV.x + topTexUV.width, topTexUV.y };
                tcCounter += 6;

                // Define bottom triangles (2 tris, 6 vertex --> v6-v8-v7, v6-v5-v8)
                mapVertices[vCounter] = v6;
                mapVertices[vCounter + 1] = v8;
                mapVertices[vCounter + 2] = v7;
                mapVertices[vCounter + 3] = v6;
                mapVertices[vCounter + 4] = v5;
                mapVertices[vCounter + 5] = v8;
                vCounter += 6;

                mapNormals[nCounter] = n4;
                mapNormals[nCounter + 1] = n4;
                mapNormals[nCounter + 2] = n4;
                mapNormals[nCounter + 3] = n4;
                mapNormals[nCounter + 4] = n4;
                mapNormals[nCounter + 5] = n4;
                nCounter += 6;

                mapTexcoords[tcCounter] = (Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 1] = (Vector2){ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 2] = (Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 3] = (Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 4] = (Vector2){ bottomTexUV.x, bottomTexUV.y };
                mapTexcoords[tcCounter + 5] = (Vector2){ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                tcCounter += 6;

                // Checking cube on bottom of current cube
                if (((z < cubicmap.height - 1) && COLOR_EQUAL(pixels[(z + 1)*cubicmap.width + x], BLACK)) || (z == cubicmap.height - 1))
                {
                    // Define front triangles (2 tris, 6 vertex) --> v2 v7 v3, v3 v7 v8
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v2;
                    mapVertices[vCounter + 1] = v7;
                    mapVertices[vCounter + 2] = v3;
                    mapVertices[vCounter + 3] = v3;
                    mapVertices[vCounter + 4] = v7;
                    mapVertices[vCounter + 5] = v8;
                    vCounter += 6;

                    mapNormals[nCounter] = n6;
                    mapNormals[nCounter + 1] = n6;
                    mapNormals[nCounter + 2] = n6;
                    mapNormals[nCounter + 3] = n6;
                    mapNormals[nCounter + 4] = n6;
                    mapNormals[nCounter + 5] = n6;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = (Vector2){ frontTexUV.x, frontTexUV.y };
                    mapTexcoords[tcCounter + 1] = (Vector2){ frontTexUV.x, frontTexUV.y + frontTexUV.height };
                    mapTexcoords[tcCounter + 2] = (Vector2){ frontTexUV.x + frontTexUV.width, frontTexUV.y };
                    mapTexcoords[tcCounter + 3] = (Vector2){ frontTexUV.x + frontTexUV.width, frontTexUV.y };
                    mapTexcoords[tcCounter + 4] = (Vector2){ frontTexUV.x, frontTexUV.y + frontTexUV.height };
                    mapTexcoords[tcCounter + 5] = (Vector2){ frontTexUV.x + frontTexUV.width, frontTexUV.y + frontTexUV.height };
                    tcCounter += 6;
                }

                // Checking cube on top of current cube
                if (((z > 0) && COLOR_EQUAL(pixels[(z - 1)*cubicmap.width + x], BLACK)) || (z == 0))
                {
                    // Define back triangles (2 tris, 6 vertex) --> v1 v5 v6, v1 v4 v5
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v1;
                    mapVertices[vCounter + 1] = v5;
                    mapVertices[vCounter + 2] = v6;
                    mapVertices[vCounter + 3] = v1;
                    mapVertices[vCounter + 4] = v4;
                    mapVertices[vCounter + 5] = v5;
                    vCounter += 6;

                    mapNormals[nCounter] = n5;
                    mapNormals[nCounter + 1] = n5;
                    mapNormals[nCounter + 2] = n5;
                    mapNormals[nCounter + 3] = n5;
                    mapNormals[nCounter + 4] = n5;
                    mapNormals[nCounter + 5] = n5;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = (Vector2){ backTexUV.x + backTexUV.width, backTexUV.y };
                    mapTexcoords[tcCounter + 1] = (Vector2){ backTexUV.x, backTexUV.y + backTexUV.height };
                    mapTexcoords[tcCounter + 2] = (Vector2){ backTexUV.x + backTexUV.width, backTexUV.y + backTexUV.height };
                    mapTexcoords[tcCounter + 3] = (Vector2){ backTexUV.x + backTexUV.width, backTexUV.y };
                    mapTexcoords[tcCounter + 4] = (Vector2){ backTexUV.x, backTexUV.y };
                    mapTexcoords[tcCounter + 5] = (Vector2){ backTexUV.x, backTexUV.y + backTexUV.height };
                    tcCounter += 6;
                }

                // Checking cube on right of current cube
                if (((x < cubicmap.width - 1) && COLOR_EQUAL(pixels[z*cubicmap.width + (x + 1)], BLACK)) || (x == cubicmap.width - 1))
                {
                    // Define right triangles (2 tris, 6 vertex) --> v3 v8 v4, v4 v8 v5
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v3;
                    mapVertices[vCounter + 1] = v8;
                    mapVertices[vCounter + 2] = v4;
                    mapVertices[vCounter + 3] = v4;
                    mapVertices[vCounter + 4] = v8;
                    mapVertices[vCounter + 5] = v5;
                    vCounter += 6;

                    mapNormals[nCounter] = n1;
                    mapNormals[nCounter + 1] = n1;
                    mapNormals[nCounter + 2] = n1;
                    mapNormals[nCounter + 3] = n1;
                    mapNormals[nCounter + 4] = n1;
                    mapNormals[nCounter + 5] = n1;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = (Vector2){ rightTexUV.x, rightTexUV.y };
                    mapTexcoords[tcCounter + 1] = (Vector2){ rightTexUV.x, rightTexUV.y + rightTexUV.height };
                    mapTexcoords[tcCounter + 2] = (Vector2){ rightTexUV.x + rightTexUV.width, rightTexUV.y };
                    mapTexcoords[tcCounter + 3] = (Vector2){ rightTexUV.x + rightTexUV.width, rightTexUV.y };
                    mapTexcoords[tcCounter + 4] = (Vector2){ rightTexUV.x, rightTexUV.y + rightTexUV.height };
                    mapTexcoords[tcCounter + 5] = (Vector2){ rightTexUV.x + rightTexUV.width, rightTexUV.y + rightTexUV.height };
                    tcCounter += 6;
                }

                // Checking cube on left of current cube
                if (((x > 0) && COLOR_EQUAL(pixels[z*cubicmap.width + (x - 1)], BLACK)) || (x == 0))
                {
                    // Define left triangles (2 tris, 6 vertex) --> v1 v7 v2, v1 v6 v7
                    // NOTE: Collateral occluded faces are not generated
                    mapVertices[vCounter] = v1;
                    mapVertices[vCounter + 1] = v7;
                    mapVertices[vCounter + 2] = v2;
                    mapVertices[vCounter + 3] = v1;
                    mapVertices[vCounter + 4] = v6;
                    mapVertices[vCounter + 5] = v7;
                    vCounter += 6;

                    mapNormals[nCounter] = n2;
                    mapNormals[nCounter + 1] = n2;
                    mapNormals[nCounter + 2] = n2;
                    mapNormals[nCounter + 3] = n2;
                    mapNormals[nCounter + 4] = n2;
                    mapNormals[nCounter + 5] = n2;
                    nCounter += 6;

                    mapTexcoords[tcCounter] = (Vector2){ leftTexUV.x, leftTexUV.y };
                    mapTexcoords[tcCounter + 1] = (Vector2){ leftTexUV.x + leftTexUV.width, leftTexUV.y + leftTexUV.height };
                    mapTexcoords[tcCounter + 2] = (Vector2){ leftTexUV.x + leftTexUV.width, leftTexUV.y };
                    mapTexcoords[tcCounter + 3] = (Vector2){ leftTexUV.x, leftTexUV.y };
                    mapTexcoords[tcCounter + 4] = (Vector2){ leftTexUV.x, leftTexUV.y + leftTexUV.height };
                    mapTexcoords[tcCounter + 5] = (Vector2){ leftTexUV.x + leftTexUV.width, leftTexUV.y + leftTexUV.height };
                    tcCounter += 6;
                }
            }
            // We check pixel color to be BLACK, we will only draw floor and roof
            else if (COLOR_EQUAL(pixels[z*cubicmap.width + x], BLACK))
            {
                // Define top triangles (2 tris, 6 vertex --> v1-v2-v3, v1-v3-v4)
                mapVertices[vCounter] = v1;
                mapVertices[vCounter + 1] = v3;
                mapVertices[vCounter + 2] = v2;
                mapVertices[vCounter + 3] = v1;
                mapVertices[vCounter + 4] = v4;
                mapVertices[vCounter + 5] = v3;
                vCounter += 6;

                mapNormals[nCounter] = n4;
                mapNormals[nCounter + 1] = n4;
                mapNormals[nCounter + 2] = n4;
                mapNormals[nCounter + 3] = n4;
                mapNormals[nCounter + 4] = n4;
                mapNormals[nCounter + 5] = n4;
                nCounter += 6;

                mapTexcoords[tcCounter] = (Vector2){ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 1] = (Vector2){ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 2] = (Vector2){ topTexUV.x, topTexUV.y + topTexUV.height };
                mapTexcoords[tcCounter + 3] = (Vector2){ topTexUV.x, topTexUV.y };
                mapTexcoords[tcCounter + 4] = (Vector2){ topTexUV.x + topTexUV.width, topTexUV.y };
                mapTexcoords[tcCounter + 5] = (Vector2){ topTexUV.x + topTexUV.width, topTexUV.y + topTexUV.height };
                tcCounter += 6;

                // Define bottom triangles (2 tris, 6 vertex --> v6-v8-v7, v6-v5-v8)
                mapVertices[vCounter] = v6;
                mapVertices[vCounter + 1] = v7;
                mapVertices[vCounter + 2] = v8;
                mapVertices[vCounter + 3] = v6;
                mapVertices[vCounter + 4] = v8;
                mapVertices[vCounter + 5] = v5;
                vCounter += 6;

                mapNormals[nCounter] = n3;
                mapNormals[nCounter + 1] = n3;
                mapNormals[nCounter + 2] = n3;
                mapNormals[nCounter + 3] = n3;
                mapNormals[nCounter + 4] = n3;
                mapNormals[nCounter + 5] = n3;
                nCounter += 6;

                mapTexcoords[tcCounter] = (Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 1] = (Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 2] = (Vector2){ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 3] = (Vector2){ bottomTexUV.x + bottomTexUV.width, bottomTexUV.y };
                mapTexcoords[tcCounter + 4] = (Vector2){ bottomTexUV.x, bottomTexUV.y + bottomTexUV.height };
                mapTexcoords[tcCounter + 5] = (Vector2){ bottomTexUV.x, bottomTexUV.y };
                tcCounter += 6;
            }
        }
    }

    // Move data from mapVertices temp arays to vertices float array
    mesh.vertexCount = vCounter;
    mesh.triangleCount = vCounter/3;

    mesh.vertices = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.normals = (float *)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = (float *)RL_MALLOC(mesh.vertexCount*2*sizeof(float));
    mesh.colors = NULL;

    int fCounter = 0;

    // Move vertices data
    for (int i = 0; i < vCounter; i++)
    {
        mesh.vertices[fCounter] = mapVertices[i].x;
        mesh.vertices[fCounter + 1] = mapVertices[i].y;
        mesh.vertices[fCounter + 2] = mapVertices[i].z;
        fCounter += 3;
    }

    fCounter = 0;

    // Move normals data
    for (int i = 0; i < nCounter; i++)
    {
        mesh.normals[fCounter] = mapNormals[i].x;
        mesh.normals[fCounter + 1] = mapNormals[i].y;
        mesh.normals[fCounter + 2] = mapNormals[i].z;
        fCounter += 3;
    }

    fCounter = 0;

    // Move texcoords data
    for (int i = 0; i < tcCounter; i++)
    {
        mesh.texcoords[fCounter] = mapTexcoords[i].x;
        mesh.texcoords[fCounter + 1] = mapTexcoords[i].y;
        fCounter += 2;
    }

    RL_FREE(mapVertices);
    RL_FREE(mapNormals);
    RL_FREE(mapTexcoords);

    UnloadImageColors(pixels);   // Unload pixels color data

    // Upload vertex data to GPU (static mesh)
    UploadMesh(&mesh, false);

    return mesh;
}
#endif      // SUPPORT_MESH_GENERATION

// Compute mesh bounding box limits
// NOTE: minVertex and maxVertex should be transformed by model transform matrix
BoundingBox GetMeshBoundingBox(Mesh mesh)
{
    // Get min and max vertex to construct bounds (AABB)
    Vector3 minVertex = { 0 };
    Vector3 maxVertex = { 0 };

    if (mesh.vertices != NULL)
    {
        minVertex = (Vector3){ mesh.vertices[0], mesh.vertices[1], mesh.vertices[2] };
        maxVertex = (Vector3){ mesh.vertices[0], mesh.vertices[1], mesh.vertices[2] };

        for (int i = 1; i < mesh.vertexCount; i++)
        {
            minVertex = Vector3Min(minVertex, (Vector3){ mesh.vertices[i*3], mesh.vertices[i*3 + 1], mesh.vertices[i*3 + 2] });
            maxVertex = Vector3Max(maxVertex, (Vector3){ mesh.vertices[i*3], mesh.vertices[i*3 + 1], mesh.vertices[i*3 + 2] });
        }
    }

    // Create the bounding box
    BoundingBox box = { 0 };
    box.min = minVertex;
    box.max = maxVertex;

    return box;
}

// Compute mesh tangents
// NOTE: To calculate mesh tangents and binormals we need mesh vertex positions and texture coordinates
// Implementation base don: https://answers.unity.com/questions/7789/calculating-tangents-vector4.html
void GenMeshTangents(Mesh *mesh)
{
    if (mesh->tangents == NULL) mesh->tangents = (float *)RL_MALLOC(mesh->vertexCount*4*sizeof(float));
    else
    {
        RL_FREE(mesh->tangents);
        mesh->tangents = (float *)RL_MALLOC(mesh->vertexCount*4*sizeof(float));
    }

    Vector3 *tan1 = (Vector3 *)RL_MALLOC(mesh->vertexCount*sizeof(Vector3));
    Vector3 *tan2 = (Vector3 *)RL_MALLOC(mesh->vertexCount*sizeof(Vector3));

    for (int i = 0; i < mesh->vertexCount; i += 3)
    {
        // Get triangle vertices
        Vector3 v1 = { mesh->vertices[(i + 0)*3 + 0], mesh->vertices[(i + 0)*3 + 1], mesh->vertices[(i + 0)*3 + 2] };
        Vector3 v2 = { mesh->vertices[(i + 1)*3 + 0], mesh->vertices[(i + 1)*3 + 1], mesh->vertices[(i + 1)*3 + 2] };
        Vector3 v3 = { mesh->vertices[(i + 2)*3 + 0], mesh->vertices[(i + 2)*3 + 1], mesh->vertices[(i + 2)*3 + 2] };

        // Get triangle texcoords
        Vector2 uv1 = { mesh->texcoords[(i + 0)*2 + 0], mesh->texcoords[(i + 0)*2 + 1] };
        Vector2 uv2 = { mesh->texcoords[(i + 1)*2 + 0], mesh->texcoords[(i + 1)*2 + 1] };
        Vector2 uv3 = { mesh->texcoords[(i + 2)*2 + 0], mesh->texcoords[(i + 2)*2 + 1] };

        float x1 = v2.x - v1.x;
        float y1 = v2.y - v1.y;
        float z1 = v2.z - v1.z;
        float x2 = v3.x - v1.x;
        float y2 = v3.y - v1.y;
        float z2 = v3.z - v1.z;

        float s1 = uv2.x - uv1.x;
        float t1 = uv2.y - uv1.y;
        float s2 = uv3.x - uv1.x;
        float t2 = uv3.y - uv1.y;

        float div = s1*t2 - s2*t1;
        float r = (div == 0.0f)? 0.0f : 1.0f/div;

        Vector3 sdir = { (t2*x1 - t1*x2)*r, (t2*y1 - t1*y2)*r, (t2*z1 - t1*z2)*r };
        Vector3 tdir = { (s1*x2 - s2*x1)*r, (s1*y2 - s2*y1)*r, (s1*z2 - s2*z1)*r };

        tan1[i + 0] = sdir;
        tan1[i + 1] = sdir;
        tan1[i + 2] = sdir;

        tan2[i + 0] = tdir;
        tan2[i + 1] = tdir;
        tan2[i + 2] = tdir;
    }

    // Compute tangents considering normals
    for (int i = 0; i < mesh->vertexCount; i++)
    {
        Vector3 normal = { mesh->normals[i*3 + 0], mesh->normals[i*3 + 1], mesh->normals[i*3 + 2] };
        Vector3 tangent = tan1[i];

        // TODO: Review, not sure if tangent computation is right, just used reference proposed maths...
#if defined(COMPUTE_TANGENTS_METHOD_01)
        Vector3 tmp = Vector3Subtract(tangent, Vector3Scale(normal, Vector3DotProduct(normal, tangent)));
        tmp = Vector3Normalize(tmp);
        mesh->tangents[i*4 + 0] = tmp.x;
        mesh->tangents[i*4 + 1] = tmp.y;
        mesh->tangents[i*4 + 2] = tmp.z;
        mesh->tangents[i*4 + 3] = 1.0f;
#else
        Vector3OrthoNormalize(&normal, &tangent);
        mesh->tangents[i*4 + 0] = tangent.x;
        mesh->tangents[i*4 + 1] = tangent.y;
        mesh->tangents[i*4 + 2] = tangent.z;
        mesh->tangents[i*4 + 3] = (Vector3DotProduct(Vector3CrossProduct(normal, tangent), tan2[i]) < 0.0f)? -1.0f : 1.0f;
#endif
    }

    RL_FREE(tan1);
    RL_FREE(tan2);

    if (mesh->vboId != NULL)
    {
        if (mesh->vboId[SHADER_LOC_VERTEX_TANGENT] != 0)
        {
            // Upate existing vertex buffer
            rlUpdateVertexBuffer(mesh->vboId[SHADER_LOC_VERTEX_TANGENT], mesh->tangents, mesh->vertexCount*4*sizeof(float), 0);
        }
        else
        {
            // Load a new tangent attributes buffer
            mesh->vboId[SHADER_LOC_VERTEX_TANGENT] = rlLoadVertexBuffer(mesh->tangents, mesh->vertexCount*4*sizeof(float), false);
        }

        rlEnableVertexArray(mesh->vaoId);
        rlSetVertexAttribute(4, 4, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(4);
        rlDisableVertexArray();
    }

    TRACELOG(LOG_INFO, "MESH: Tangents data computed and uploaded for provided mesh");
}

// Compute mesh binormals (aka bitangent)
void GenMeshBinormals(Mesh *mesh)
{
    for (int i = 0; i < mesh->vertexCount; i++)
    {
        //Vector3 normal = { mesh->normals[i*3 + 0], mesh->normals[i*3 + 1], mesh->normals[i*3 + 2] };
        //Vector3 tangent = { mesh->tangents[i*4 + 0], mesh->tangents[i*4 + 1], mesh->tangents[i*4 + 2] };
        //Vector3 binormal = Vector3Scale(Vector3CrossProduct(normal, tangent), mesh->tangents[i*4 + 3]);

        // TODO: Register computed binormal in mesh->binormal?
    }
}

// Draw a model (with texture if set)
void DrawModel(Model model, Vector3 position, float scale, Color tint)
{
    Vector3 vScale = { scale, scale, scale };
    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };

    DrawModelEx(model, position, rotationAxis, 0.0f, vScale, tint);
}

// Draw a model with extended parameters
void DrawModelEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint)
{
    // Calculate transformation matrix from function parameters
    // Get transform matrix (rotation -> scale -> translation)
    Matrix matScale = MatrixScale(scale.x, scale.y, scale.z);
    Matrix matRotation = MatrixRotate(rotationAxis, rotationAngle*DEG2RAD);
    Matrix matTranslation = MatrixTranslate(position.x, position.y, position.z);

    Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);

    // Combine model transformation matrix (model.transform) with matrix generated by function parameters (matTransform)
    model.transform = MatrixMultiply(model.transform, matTransform);

    for (int i = 0; i < model.meshCount; i++)
    {
        Color color = model.materials[model.meshMaterial[i]].maps[MATERIAL_MAP_DIFFUSE].color;

        Color colorTint = WHITE;
        colorTint.r = (unsigned char)((((float)color.r/255.0f)*((float)tint.r/255.0f))*255.0f);
        colorTint.g = (unsigned char)((((float)color.g/255.0f)*((float)tint.g/255.0f))*255.0f);
        colorTint.b = (unsigned char)((((float)color.b/255.0f)*((float)tint.b/255.0f))*255.0f);
        colorTint.a = (unsigned char)((((float)color.a/255.0f)*((float)tint.a/255.0f))*255.0f);

        model.materials[model.meshMaterial[i]].maps[MATERIAL_MAP_DIFFUSE].color = colorTint;
        DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], model.transform);
        model.materials[model.meshMaterial[i]].maps[MATERIAL_MAP_DIFFUSE].color = color;
    }
}

// Draw a model wires (with texture if set)
void DrawModelWires(Model model, Vector3 position, float scale, Color tint)
{
    rlEnableWireMode();

    DrawModel(model, position, scale, tint);

    rlDisableWireMode();
}

// Draw a model wires (with texture if set) with extended parameters
void DrawModelWiresEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint)
{
    rlEnableWireMode();

    DrawModelEx(model, position, rotationAxis, rotationAngle, scale, tint);

    rlDisableWireMode();
}

// Draw a billboard
void DrawBillboard(Camera camera, Texture2D texture, Vector3 position, float size, Color tint)
{
    Rectangle source = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };

    DrawBillboardRec(camera, texture, source, position, (Vector2){ size, size }, tint);
}

// Draw a billboard (part of a texture defined by a rectangle)
void DrawBillboardRec(Camera camera, Texture2D texture, Rectangle source, Vector3 position, Vector2 size, Color tint)
{
    // NOTE: Billboard locked on axis-Y
    Vector3 up = { 0.0f, 1.0f, 0.0f };

    DrawBillboardPro(camera, texture, source, position, up, size, Vector2Zero(), 0.0f, tint);
}

void DrawBillboardPro(Camera camera, Texture2D texture, Rectangle source, Vector3 position, Vector3 up, Vector2 size, Vector2 origin, float rotation, Color tint)
{
    // NOTE: Billboard size will maintain source rectangle aspect ratio, size will represent billboard width
    Vector2 sizeRatio = { size.y, size.x*(float)source.height/source.width };

    Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

    Vector3 right = { matView.m0, matView.m4, matView.m8 };
    //Vector3 up = { matView.m1, matView.m5, matView.m9 };

    Vector3 rightScaled = Vector3Scale(right, sizeRatio.x/2);
    Vector3 upScaled = Vector3Scale(up, sizeRatio.y/2);

    Vector3 p1 = Vector3Add(rightScaled, upScaled);
    Vector3 p2 = Vector3Subtract(rightScaled, upScaled);

    Vector3 topLeft = Vector3Scale(p2, -1);
    Vector3 topRight = p1;
    Vector3 bottomRight = p2;
    Vector3 bottomLeft = Vector3Scale(p1, -1);

    if (rotation != 0.0f)
    {
        float sinRotation = sinf(rotation*DEG2RAD);
        float cosRotation = cosf(rotation*DEG2RAD);

        // NOTE: (-1, 1) is the range where origin.x, origin.y is inside the texture
        float rotateAboutX = sizeRatio.x*origin.x/2;
        float rotateAboutY = sizeRatio.y*origin.y/2;

        float xtvalue, ytvalue;
        float rotatedX, rotatedY;

        xtvalue = Vector3DotProduct(right, topLeft) - rotateAboutX; // Project points to x and y coordinates on the billboard plane
        ytvalue = Vector3DotProduct(up, topLeft) - rotateAboutY;
        rotatedX = xtvalue*cosRotation - ytvalue*sinRotation + rotateAboutX; // Rotate about the point origin
        rotatedY = xtvalue*sinRotation + ytvalue*cosRotation + rotateAboutY;
        topLeft = Vector3Add(Vector3Scale(up, rotatedY), Vector3Scale(right, rotatedX)); // Translate back to cartesian coordinates

        xtvalue = Vector3DotProduct(right, topRight) - rotateAboutX;
        ytvalue = Vector3DotProduct(up, topRight) - rotateAboutY;
        rotatedX = xtvalue*cosRotation - ytvalue*sinRotation + rotateAboutX;
        rotatedY = xtvalue*sinRotation + ytvalue*cosRotation + rotateAboutY;
        topRight = Vector3Add(Vector3Scale(up, rotatedY), Vector3Scale(right, rotatedX));

        xtvalue = Vector3DotProduct(right, bottomRight) - rotateAboutX;
        ytvalue = Vector3DotProduct(up, bottomRight) - rotateAboutY;
        rotatedX = xtvalue*cosRotation - ytvalue*sinRotation + rotateAboutX;
        rotatedY = xtvalue*sinRotation + ytvalue*cosRotation + rotateAboutY;
        bottomRight = Vector3Add(Vector3Scale(up, rotatedY), Vector3Scale(right, rotatedX));

        xtvalue = Vector3DotProduct(right, bottomLeft)-rotateAboutX;
        ytvalue = Vector3DotProduct(up, bottomLeft)-rotateAboutY;
        rotatedX = xtvalue*cosRotation - ytvalue*sinRotation + rotateAboutX;
        rotatedY = xtvalue*sinRotation + ytvalue*cosRotation + rotateAboutY;
        bottomLeft = Vector3Add(Vector3Scale(up, rotatedY), Vector3Scale(right, rotatedX));
    }

    // Translate points to the draw center (position)
    topLeft = Vector3Add(topLeft, position);
    topRight = Vector3Add(topRight, position);
    bottomRight = Vector3Add(bottomRight, position);
    bottomLeft = Vector3Add(bottomLeft, position);

    rlCheckRenderBatchLimit(4);

    rlSetTexture(texture.id);

    rlBegin(RL_QUADS);
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);

        // Bottom-left corner for texture and quad
        rlTexCoord2f((float)source.x/texture.width, (float)source.y/texture.height);
        rlVertex3f(topLeft.x, topLeft.y, topLeft.z);

        // Top-left corner for texture and quad
        rlTexCoord2f((float)source.x/texture.width, (float)(source.y + source.height)/texture.height);
        rlVertex3f(bottomLeft.x, bottomLeft.y, bottomLeft.z);

        // Top-right corner for texture and quad
        rlTexCoord2f((float)(source.x + source.width)/texture.width, (float)(source.y + source.height)/texture.height);
        rlVertex3f(bottomRight.x, bottomRight.y, bottomRight.z);

        // Bottom-right corner for texture and quad
        rlTexCoord2f((float)(source.x + source.width)/texture.width, (float)source.y/texture.height);
        rlVertex3f(topRight.x, topRight.y, topRight.z);
    rlEnd();

    rlSetTexture(0);
}

// Draw a bounding box with wires
void DrawBoundingBox(BoundingBox box, Color color)
{
    Vector3 size = { 0 };

    size.x = fabsf(box.max.x - box.min.x);
    size.y = fabsf(box.max.y - box.min.y);
    size.z = fabsf(box.max.z - box.min.z);

    Vector3 center = { box.min.x + size.x/2.0f, box.min.y + size.y/2.0f, box.min.z + size.z/2.0f };

    DrawCubeWires(center, size.x, size.y, size.z, color);
}

// Check collision between two spheres
bool CheckCollisionSpheres(Vector3 center1, float radius1, Vector3 center2, float radius2)
{
    bool collision = false;

    // Simple way to check for collision, just checking distance between two points
    // Unfortunately, sqrtf() is a costly operation, so we avoid it with following solution
    /*
    float dx = center1.x - center2.x;      // X distance between centers
    float dy = center1.y - center2.y;      // Y distance between centers
    float dz = center1.z - center2.z;      // Z distance between centers

    float distance = sqrtf(dx*dx + dy*dy + dz*dz);  // Distance between centers

    if (distance <= (radius1 + radius2)) collision = true;
    */

    // Check for distances squared to avoid sqrtf()
    if (Vector3DotProduct(Vector3Subtract(center2, center1), Vector3Subtract(center2, center1)) <= (radius1 + radius2)*(radius1 + radius2)) collision = true;

    return collision;
}

// Check collision between two boxes
// NOTE: Boxes are defined by two points minimum and maximum
bool CheckCollisionBoxes(BoundingBox box1, BoundingBox box2)
{
    bool collision = true;

    if ((box1.max.x >= box2.min.x) && (box1.min.x <= box2.max.x))
    {
        if ((box1.max.y < box2.min.y) || (box1.min.y > box2.max.y)) collision = false;
        if ((box1.max.z < box2.min.z) || (box1.min.z > box2.max.z)) collision = false;
    }
    else collision = false;

    return collision;
}

// Check collision between box and sphere
bool CheckCollisionBoxSphere(BoundingBox box, Vector3 center, float radius)
{
    bool collision = false;

    float dmin = 0;

    if (center.x < box.min.x) dmin += powf(center.x - box.min.x, 2);
    else if (center.x > box.max.x) dmin += powf(center.x - box.max.x, 2);

    if (center.y < box.min.y) dmin += powf(center.y - box.min.y, 2);
    else if (center.y > box.max.y) dmin += powf(center.y - box.max.y, 2);

    if (center.z < box.min.z) dmin += powf(center.z - box.min.z, 2);
    else if (center.z > box.max.z) dmin += powf(center.z - box.max.z, 2);

    if (dmin <= (radius*radius)) collision = true;

    return collision;
}

// Get collision info between ray and sphere
RayCollision GetRayCollisionSphere(Ray ray, Vector3 center, float radius)
{
    RayCollision collision = { 0 };

    Vector3 raySpherePos = Vector3Subtract(center, ray.position);
    float vector = Vector3DotProduct(raySpherePos, ray.direction);
    float distance = Vector3Length(raySpherePos);
    float d = radius*radius - (distance*distance - vector*vector);

    collision.hit = d >= 0.0f;

    // Check if ray origin is inside the sphere to calculate the correct collision point
    if (distance < radius)
    {
        collision.distance = vector + sqrtf(d);

        // Calculate collision point
        collision.point = Vector3Add(ray.position, Vector3Scale(ray.direction, collision.distance));

        // Calculate collision normal (pointing outwards)
        collision.normal = Vector3Negate(Vector3Normalize(Vector3Subtract(collision.point, center)));
    }
    else
    {
        collision.distance = vector - sqrtf(d);

        // Calculate collision point
        collision.point = Vector3Add(ray.position, Vector3Scale(ray.direction, collision.distance));

        // Calculate collision normal (pointing inwards)
        collision.normal = Vector3Normalize(Vector3Subtract(collision.point, center));
    }

    return collision;
}

// Get collision info between ray and box
RayCollision GetRayCollisionBox(Ray ray, BoundingBox box)
{
    RayCollision collision = { 0 };

    // Note: If ray.position is inside the box, the distance is negative (as if the ray was reversed)
    // Reversing ray.direction will give use the correct result.
    bool insideBox = (ray.position.x > box.min.x) && (ray.position.x < box.max.x) &&
                     (ray.position.y > box.min.y) && (ray.position.y < box.max.y) &&
                     (ray.position.z > box.min.z) && (ray.position.z < box.max.z);

    if (insideBox) ray.direction = Vector3Negate(ray.direction);

    float t[11] = { 0 };

    t[8] = 1.0f/ray.direction.x;
    t[9] = 1.0f/ray.direction.y;
    t[10] = 1.0f/ray.direction.z;

    t[0] = (box.min.x - ray.position.x)*t[8];
    t[1] = (box.max.x - ray.position.x)*t[8];
    t[2] = (box.min.y - ray.position.y)*t[9];
    t[3] = (box.max.y - ray.position.y)*t[9];
    t[4] = (box.min.z - ray.position.z)*t[10];
    t[5] = (box.max.z - ray.position.z)*t[10];
    t[6] = (float)fmax(fmax(fmin(t[0], t[1]), fmin(t[2], t[3])), fmin(t[4], t[5]));
    t[7] = (float)fmin(fmin(fmax(t[0], t[1]), fmax(t[2], t[3])), fmax(t[4], t[5]));

    collision.hit = !((t[7] < 0) || (t[6] > t[7]));
    collision.distance = t[6];
    collision.point = Vector3Add(ray.position, Vector3Scale(ray.direction, collision.distance));

    // Get box center point
    collision.normal = Vector3Lerp(box.min, box.max, 0.5f);
    // Get vector center point->hit point
    collision.normal = Vector3Subtract(collision.point, collision.normal);
    // Scale vector to unit cube
    // NOTE: We use an additional .01 to fix numerical errors
    collision.normal = Vector3Scale(collision.normal, 2.01f);
    collision.normal = Vector3Divide(collision.normal, Vector3Subtract(box.max, box.min));
    // The relevant elemets of the vector are now slightly larger than 1.0f (or smaller than -1.0f)
    // and the others are somewhere between -1.0 and 1.0 casting to int is exactly our wanted normal!
    collision.normal.x = (float)((int)collision.normal.x);
    collision.normal.y = (float)((int)collision.normal.y);
    collision.normal.z = (float)((int)collision.normal.z);

    collision.normal = Vector3Normalize(collision.normal);

    if (insideBox)
    {
        // Reset ray.direction
        ray.direction = Vector3Negate(ray.direction);
        // Fix result
        collision.distance *= -1.0f;
        collision.normal = Vector3Negate(collision.normal);
    }

    return collision;
}

// Get collision info between ray and mesh
RayCollision GetRayCollisionMesh(Ray ray, Mesh mesh, Matrix transform)
{
    RayCollision collision = { 0 };

    // Check if mesh vertex data on CPU for testing
    if (mesh.vertices != NULL)
    {
        int triangleCount = mesh.triangleCount;

        // Test against all triangles in mesh
        for (int i = 0; i < triangleCount; i++)
        {
            Vector3 a, b, c;
            Vector3* vertdata = (Vector3*)mesh.vertices;

            if (mesh.indices)
            {
                a = vertdata[mesh.indices[i*3 + 0]];
                b = vertdata[mesh.indices[i*3 + 1]];
                c = vertdata[mesh.indices[i*3 + 2]];
            }
            else
            {
                a = vertdata[i*3 + 0];
                b = vertdata[i*3 + 1];
                c = vertdata[i*3 + 2];
            }

            a = Vector3Transform(a, transform);
            b = Vector3Transform(b, transform);
            c = Vector3Transform(c, transform);

            RayCollision triHitInfo = GetRayCollisionTriangle(ray, a, b, c);

            if (triHitInfo.hit)
            {
                // Save the closest hit triangle
                if ((!collision.hit) || (collision.distance > triHitInfo.distance)) collision = triHitInfo;
            }
        }
    }

    return collision;
}

// Get collision info between ray and triangle
// NOTE: The points are expected to be in counter-clockwise winding
// NOTE: Based on https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
RayCollision GetRayCollisionTriangle(Ray ray, Vector3 p1, Vector3 p2, Vector3 p3)
{
    #define EPSILON 0.000001f        // A small number

    RayCollision collision = { 0 };
    Vector3 edge1 = { 0 };
    Vector3 edge2 = { 0 };
    Vector3 p, q, tv;
    float det, invDet, u, v, t;

    // Find vectors for two edges sharing V1
    edge1 = Vector3Subtract(p2, p1);
    edge2 = Vector3Subtract(p3, p1);

    // Begin calculating determinant - also used to calculate u parameter
    p = Vector3CrossProduct(ray.direction, edge2);

    // If determinant is near zero, ray lies in plane of triangle or ray is parallel to plane of triangle
    det = Vector3DotProduct(edge1, p);

    // Avoid culling!
    if ((det > -EPSILON) && (det < EPSILON)) return collision;

    invDet = 1.0f/det;

    // Calculate distance from V1 to ray origin
    tv = Vector3Subtract(ray.position, p1);

    // Calculate u parameter and test bound
    u = Vector3DotProduct(tv, p)*invDet;

    // The intersection lies outside of the triangle
    if ((u < 0.0f) || (u > 1.0f)) return collision;

    // Prepare to test v parameter
    q = Vector3CrossProduct(tv, edge1);

    // Calculate V parameter and test bound
    v = Vector3DotProduct(ray.direction, q)*invDet;

    // The intersection lies outside of the triangle
    if ((v < 0.0f) || ((u + v) > 1.0f)) return collision;

    t = Vector3DotProduct(edge2, q)*invDet;

    if (t > EPSILON)
    {
        // Ray hit, get hit point and normal
        collision.hit = true;
        collision.distance = t;
        collision.normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
        collision.point = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    }

    return collision;
}

// Get collision info between ray and quad
// NOTE: The points are expected to be in counter-clockwise winding
RayCollision GetRayCollisionQuad(Ray ray, Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4)
{
    RayCollision collision = { 0 };

    collision = GetRayCollisionTriangle(ray, p1, p2, p4);

    if (!collision.hit) collision = GetRayCollisionTriangle(ray, p2, p3, p4);

    return collision;
}

//----------------------------------------------------------------------------------
// Module specific Functions Definition
//----------------------------------------------------------------------------------
#if defined(SUPPORT_FILEFORMAT_OBJ)
// Load OBJ mesh data
//
// Keep the following information in mind when reading this
//  - A mesh is created for every material present in the obj file
//  - the model.meshCount is therefore the materialCount returned from tinyobj
//  - the mesh is automatically triangulated by tinyobj
static Model LoadOBJ(const char *fileName)
{
    Model model = { 0 };

    tinyobj_attrib_t attrib = { 0 };
    tinyobj_shape_t *meshes = NULL;
    unsigned int meshCount = 0;

    tinyobj_material_t *materials = NULL;
    unsigned int materialCount = 0;

    char *fileText = LoadFileText(fileName);

    if (fileText != NULL)
    {
        unsigned int dataSize = (unsigned int)strlen(fileText);
        char currentDir[1024] = { 0 };
        strcpy(currentDir, GetWorkingDirectory());
        const char *workingDir = GetDirectoryPath(fileName);
        if (CHDIR(workingDir) != 0)
        {
            TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to change working directory", workingDir);
        }

        unsigned int flags = TINYOBJ_FLAG_TRIANGULATE;
        int ret = tinyobj_parse_obj(&attrib, &meshes, &meshCount, &materials, &materialCount, fileText, dataSize, flags);

        if (ret != TINYOBJ_SUCCESS) TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load OBJ data", fileName);
        else TRACELOG(LOG_INFO, "MODEL: [%s] OBJ data loaded successfully: %i meshes/%i materials", fileName, meshCount, materialCount);

        model.meshCount = materialCount;

        // Init model materials array
        if (materialCount > 0)
        {
            model.materialCount = materialCount;
            model.materials = (Material *)RL_CALLOC(model.materialCount, sizeof(Material));
            TraceLog(LOG_INFO, "MODEL: model has %i material meshes", materialCount);
        }
        else
        {
            model.meshCount = 1;
            TraceLog(LOG_INFO, "MODEL: No materials, putting all meshes in a default material");
        }

        model.meshes = (Mesh *)RL_CALLOC(model.meshCount, sizeof(Mesh));
        model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));

        // Count the faces for each material
        int *matFaces = RL_CALLOC(model.meshCount, sizeof(int));

        // iff no materials are present use all faces on one mesh
        if (materialCount > 0)
        {
            for (unsigned int fi = 0; fi < attrib.num_faces; fi++)
            {
                //tinyobj_vertex_index_t face = attrib.faces[fi];
                int idx = attrib.material_ids[fi];
                matFaces[idx]++;
            }

        }
        else
        {
            matFaces[0] = attrib.num_faces;
        }

        //--------------------------------------
        // Create the material meshes

        // Running counts/indexes for each material mesh as we are
        // building them at the same time
        int *vCount = RL_CALLOC(model.meshCount, sizeof(int));
        int *vtCount = RL_CALLOC(model.meshCount, sizeof(int));
        int *vnCount = RL_CALLOC(model.meshCount, sizeof(int));
        int *faceCount = RL_CALLOC(model.meshCount, sizeof(int));

        // Allocate space for each of the material meshes
        for (int mi = 0; mi < model.meshCount; mi++)
        {
            model.meshes[mi].vertexCount = matFaces[mi]*3;
            model.meshes[mi].triangleCount = matFaces[mi];
            model.meshes[mi].vertices = (float *)RL_CALLOC(model.meshes[mi].vertexCount*3, sizeof(float));
            model.meshes[mi].texcoords = (float *)RL_CALLOC(model.meshes[mi].vertexCount*2, sizeof(float));
            model.meshes[mi].normals = (float *)RL_CALLOC(model.meshes[mi].vertexCount*3, sizeof(float));
            model.meshMaterial[mi] = mi;
        }

        // Scan through the combined sub meshes and pick out each material mesh
        for (unsigned int af = 0; af < attrib.num_faces; af++)
        {
            int mm = attrib.material_ids[af];   // mesh material for this face
            if (mm == -1) { mm = 0; }           // no material object..

            // Get indices for the face
            tinyobj_vertex_index_t idx0 = attrib.faces[3*af + 0];
            tinyobj_vertex_index_t idx1 = attrib.faces[3*af + 1];
            tinyobj_vertex_index_t idx2 = attrib.faces[3*af + 2];

            // Fill vertices buffer (float) using vertex index of the face
            for (int v = 0; v < 3; v++) { model.meshes[mm].vertices[vCount[mm] + v] = attrib.vertices[idx0.v_idx*3 + v]; } vCount[mm] +=3;
            for (int v = 0; v < 3; v++) { model.meshes[mm].vertices[vCount[mm] + v] = attrib.vertices[idx1.v_idx*3 + v]; } vCount[mm] +=3;
            for (int v = 0; v < 3; v++) { model.meshes[mm].vertices[vCount[mm] + v] = attrib.vertices[idx2.v_idx*3 + v]; } vCount[mm] +=3;

            if (attrib.num_texcoords > 0)
            {
                // Fill texcoords buffer (float) using vertex index of the face
                // NOTE: Y-coordinate must be flipped upside-down to account for
                // raylib's upside down textures...
                model.meshes[mm].texcoords[vtCount[mm] + 0] = attrib.texcoords[idx0.vt_idx*2 + 0];
                model.meshes[mm].texcoords[vtCount[mm] + 1] = 1.0f - attrib.texcoords[idx0.vt_idx*2 + 1]; vtCount[mm] += 2;
                model.meshes[mm].texcoords[vtCount[mm] + 0] = attrib.texcoords[idx1.vt_idx*2 + 0];
                model.meshes[mm].texcoords[vtCount[mm] + 1] = 1.0f - attrib.texcoords[idx1.vt_idx*2 + 1]; vtCount[mm] += 2;
                model.meshes[mm].texcoords[vtCount[mm] + 0] = attrib.texcoords[idx2.vt_idx*2 + 0];
                model.meshes[mm].texcoords[vtCount[mm] + 1] = 1.0f - attrib.texcoords[idx2.vt_idx*2 + 1]; vtCount[mm] += 2;
            }

            if (attrib.num_normals > 0)
            {
                // Fill normals buffer (float) using vertex index of the face
                for (int v = 0; v < 3; v++) { model.meshes[mm].normals[vnCount[mm] + v] = attrib.normals[idx0.vn_idx*3 + v]; } vnCount[mm] +=3;
                for (int v = 0; v < 3; v++) { model.meshes[mm].normals[vnCount[mm] + v] = attrib.normals[idx1.vn_idx*3 + v]; } vnCount[mm] +=3;
                for (int v = 0; v < 3; v++) { model.meshes[mm].normals[vnCount[mm] + v] = attrib.normals[idx2.vn_idx*3 + v]; } vnCount[mm] +=3;
            }
        }

        // Init model materials
        for (unsigned int m = 0; m < materialCount; m++)
        {
            // Init material to default
            // NOTE: Uses default shader, which only supports MATERIAL_MAP_DIFFUSE
            model.materials[m] = LoadMaterialDefault();

            // Get default texture, in case no texture is defined
            // NOTE: rlgl default texture is a 1x1 pixel UNCOMPRESSED_R8G8B8A8
            model.materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

            if (materials[m].diffuse_texname != NULL) model.materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTexture(materials[m].diffuse_texname);  //char *diffuse_texname; // map_Kd

            model.materials[m].maps[MATERIAL_MAP_DIFFUSE].color = (Color){ (unsigned char)(materials[m].diffuse[0]*255.0f), (unsigned char)(materials[m].diffuse[1]*255.0f), (unsigned char)(materials[m].diffuse[2]*255.0f), 255 }; //float diffuse[3];
            model.materials[m].maps[MATERIAL_MAP_DIFFUSE].value = 0.0f;

            if (materials[m].specular_texname != NULL) model.materials[m].maps[MATERIAL_MAP_SPECULAR].texture = LoadTexture(materials[m].specular_texname);  //char *specular_texname; // map_Ks
            model.materials[m].maps[MATERIAL_MAP_SPECULAR].color = (Color){ (unsigned char)(materials[m].specular[0]*255.0f), (unsigned char)(materials[m].specular[1]*255.0f), (unsigned char)(materials[m].specular[2]*255.0f), 255 }; //float specular[3];
            model.materials[m].maps[MATERIAL_MAP_SPECULAR].value = 0.0f;

            if (materials[m].bump_texname != NULL) model.materials[m].maps[MATERIAL_MAP_NORMAL].texture = LoadTexture(materials[m].bump_texname);  //char *bump_texname; // map_bump, bump
            model.materials[m].maps[MATERIAL_MAP_NORMAL].color = WHITE;
            model.materials[m].maps[MATERIAL_MAP_NORMAL].value = materials[m].shininess;

            model.materials[m].maps[MATERIAL_MAP_EMISSION].color = (Color){ (unsigned char)(materials[m].emission[0]*255.0f), (unsigned char)(materials[m].emission[1]*255.0f), (unsigned char)(materials[m].emission[2]*255.0f), 255 }; //float emission[3];

            if (materials[m].displacement_texname != NULL) model.materials[m].maps[MATERIAL_MAP_HEIGHT].texture = LoadTexture(materials[m].displacement_texname);  //char *displacement_texname; // disp
        }

        tinyobj_attrib_free(&attrib);
        tinyobj_shapes_free(meshes, meshCount);
        tinyobj_materials_free(materials, materialCount);

        UnloadFileText(fileText);

        RL_FREE(matFaces);
        RL_FREE(vCount);
        RL_FREE(vtCount);
        RL_FREE(vnCount);
        RL_FREE(faceCount);

        if (CHDIR(currentDir) != 0)
        {
            TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to change working directory", currentDir);
        }
    }

    return model;
}
#endif

#if defined(SUPPORT_FILEFORMAT_IQM)
// Load IQM mesh data
static Model LoadIQM(const char *fileName)
{
    #define IQM_MAGIC     "INTERQUAKEMODEL" // IQM file magic number
    #define IQM_VERSION          2          // only IQM version 2 supported

    #define BONE_NAME_LENGTH    32          // BoneInfo name string length
    #define MESH_NAME_LENGTH    32          // Mesh name string length
    #define MATERIAL_NAME_LENGTH 32         // Material name string length

    unsigned int fileSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &fileSize);
    unsigned char *fileDataPtr = fileData;

    // IQM file structs
    //-----------------------------------------------------------------------------------
    typedef struct IQMHeader {
        char magic[16];
        unsigned int version;
        unsigned int filesize;
        unsigned int flags;
        unsigned int num_text, ofs_text;
        unsigned int num_meshes, ofs_meshes;
        unsigned int num_vertexarrays, num_vertexes, ofs_vertexarrays;
        unsigned int num_triangles, ofs_triangles, ofs_adjacency;
        unsigned int num_joints, ofs_joints;
        unsigned int num_poses, ofs_poses;
        unsigned int num_anims, ofs_anims;
        unsigned int num_frames, num_framechannels, ofs_frames, ofs_bounds;
        unsigned int num_comment, ofs_comment;
        unsigned int num_extensions, ofs_extensions;
    } IQMHeader;

    typedef struct IQMMesh {
        unsigned int name;
        unsigned int material;
        unsigned int first_vertex, num_vertexes;
        unsigned int first_triangle, num_triangles;
    } IQMMesh;

    typedef struct IQMTriangle {
        unsigned int vertex[3];
    } IQMTriangle;

    typedef struct IQMJoint {
        unsigned int name;
        int parent;
        float translate[3], rotate[4], scale[3];
    } IQMJoint;

    typedef struct IQMVertexArray {
        unsigned int type;
        unsigned int flags;
        unsigned int format;
        unsigned int size;
        unsigned int offset;
    } IQMVertexArray;

    // NOTE: Below IQM structures are not used but listed for reference
    /*
    typedef struct IQMAdjacency {
        unsigned int triangle[3];
    } IQMAdjacency;

    typedef struct IQMPose {
        int parent;
        unsigned int mask;
        float channeloffset[10];
        float channelscale[10];
    } IQMPose;

    typedef struct IQMAnim {
        unsigned int name;
        unsigned int first_frame, num_frames;
        float framerate;
        unsigned int flags;
    } IQMAnim;

    typedef struct IQMBounds {
        float bbmin[3], bbmax[3];
        float xyradius, radius;
    } IQMBounds;
    */
    //-----------------------------------------------------------------------------------

    // IQM vertex data types
    enum {
        IQM_POSITION     = 0,
        IQM_TEXCOORD     = 1,
        IQM_NORMAL       = 2,
        IQM_TANGENT      = 3,       // NOTE: Tangents unused by default
        IQM_BLENDINDEXES = 4,
        IQM_BLENDWEIGHTS = 5,
        IQM_COLOR        = 6,
        IQM_CUSTOM       = 0x10     // NOTE: Custom vertex values unused by default
    };

    Model model = { 0 };

    IQMMesh *imesh = NULL;
    IQMTriangle *tri = NULL;
    IQMVertexArray *va = NULL;
    IQMJoint *ijoint = NULL;

    float *vertex = NULL;
    float *normal = NULL;
    float *text = NULL;
    char *blendi = NULL;
    unsigned char *blendw = NULL;
    unsigned char *color = NULL;

    // In case file can not be read, return an empty model
    if (fileDataPtr == NULL) return model;

    // Read IQM header
    IQMHeader *iqmHeader = (IQMHeader *)fileDataPtr;

    if (memcmp(iqmHeader->magic, IQM_MAGIC, sizeof(IQM_MAGIC)) != 0)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] IQM file is not a valid model", fileName);
        return model;
    }

    if (iqmHeader->version != IQM_VERSION)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] IQM file version not supported (%i)", fileName, iqmHeader->version);
        return model;
    }

    //fileDataPtr += sizeof(IQMHeader);       // Move file data pointer

    // Meshes data processing
    imesh = RL_MALLOC(iqmHeader->num_meshes*sizeof(IQMMesh));
    //fseek(iqmFile, iqmHeader->ofs_meshes, SEEK_SET);
    //fread(imesh, sizeof(IQMMesh)*iqmHeader->num_meshes, 1, iqmFile);
    memcpy(imesh, fileDataPtr + iqmHeader->ofs_meshes, iqmHeader->num_meshes*sizeof(IQMMesh));

    model.meshCount = iqmHeader->num_meshes;
    model.meshes = RL_CALLOC(model.meshCount, sizeof(Mesh));

    model.materialCount = model.meshCount;
    model.materials = (Material *)RL_CALLOC(model.materialCount, sizeof(Material));
    model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));

    char name[MESH_NAME_LENGTH] = { 0 };
    char material[MATERIAL_NAME_LENGTH] = { 0 };

    for (int i = 0; i < model.meshCount; i++)
    {
        //fseek(iqmFile, iqmHeader->ofs_text + imesh[i].name, SEEK_SET);
        //fread(name, sizeof(char)*MESH_NAME_LENGTH, 1, iqmFile);
        memcpy(name, fileDataPtr + iqmHeader->ofs_text + imesh[i].name, MESH_NAME_LENGTH*sizeof(char));

        //fseek(iqmFile, iqmHeader->ofs_text + imesh[i].material, SEEK_SET);
        //fread(material, sizeof(char)*MATERIAL_NAME_LENGTH, 1, iqmFile);
        memcpy(material, fileDataPtr + iqmHeader->ofs_text + imesh[i].material, MATERIAL_NAME_LENGTH*sizeof(char));

        model.materials[i] = LoadMaterialDefault();

        TRACELOG(LOG_DEBUG, "MODEL: [%s] mesh name (%s), material (%s)", fileName, name, material);

        model.meshes[i].vertexCount = imesh[i].num_vertexes;

        model.meshes[i].vertices = RL_CALLOC(model.meshes[i].vertexCount*3, sizeof(float));       // Default vertex positions
        model.meshes[i].normals = RL_CALLOC(model.meshes[i].vertexCount*3, sizeof(float));        // Default vertex normals
        model.meshes[i].texcoords = RL_CALLOC(model.meshes[i].vertexCount*2, sizeof(float));      // Default vertex texcoords

        model.meshes[i].boneIds = RL_CALLOC(model.meshes[i].vertexCount*4, sizeof(unsigned char));  // Up-to 4 bones supported!
        model.meshes[i].boneWeights = RL_CALLOC(model.meshes[i].vertexCount*4, sizeof(float));      // Up-to 4 bones supported!

        model.meshes[i].triangleCount = imesh[i].num_triangles;
        model.meshes[i].indices = RL_CALLOC(model.meshes[i].triangleCount*3, sizeof(unsigned short));

        // Animated verted data, what we actually process for rendering
        // NOTE: Animated vertex should be re-uploaded to GPU (if not using GPU skinning)
        model.meshes[i].animVertices = RL_CALLOC(model.meshes[i].vertexCount*3, sizeof(float));
        model.meshes[i].animNormals = RL_CALLOC(model.meshes[i].vertexCount*3, sizeof(float));
    }

    // Triangles data processing
    tri = RL_MALLOC(iqmHeader->num_triangles*sizeof(IQMTriangle));
    //fseek(iqmFile, iqmHeader->ofs_triangles, SEEK_SET);
    //fread(tri, iqmHeader->num_triangles*sizeof(IQMTriangle), 1, iqmFile);
    memcpy(tri, fileDataPtr + iqmHeader->ofs_triangles, iqmHeader->num_triangles*sizeof(IQMTriangle));

    for (int m = 0; m < model.meshCount; m++)
    {
        int tcounter = 0;

        for (unsigned int i = imesh[m].first_triangle; i < (imesh[m].first_triangle + imesh[m].num_triangles); i++)
        {
            // IQM triangles indexes are stored in counter-clockwise, but raylib processes the index in linear order,
            // expecting they point to the counter-clockwise vertex triangle, so we need to reverse triangle indexes
            // NOTE: raylib renders vertex data in counter-clockwise order (standard convention) by default
            model.meshes[m].indices[tcounter + 2] = tri[i].vertex[0] - imesh[m].first_vertex;
            model.meshes[m].indices[tcounter + 1] = tri[i].vertex[1] - imesh[m].first_vertex;
            model.meshes[m].indices[tcounter] = tri[i].vertex[2] - imesh[m].first_vertex;
            tcounter += 3;
        }
    }

    // Vertex arrays data processing
    va = RL_MALLOC(iqmHeader->num_vertexarrays*sizeof(IQMVertexArray));
    //fseek(iqmFile, iqmHeader->ofs_vertexarrays, SEEK_SET);
    //fread(va, iqmHeader->num_vertexarrays*sizeof(IQMVertexArray), 1, iqmFile);
    memcpy(va, fileDataPtr + iqmHeader->ofs_vertexarrays, iqmHeader->num_vertexarrays*sizeof(IQMVertexArray));

    for (unsigned int i = 0; i < iqmHeader->num_vertexarrays; i++)
    {
        switch (va[i].type)
        {
            case IQM_POSITION:
            {
                vertex = RL_MALLOC(iqmHeader->num_vertexes*3*sizeof(float));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(vertex, iqmHeader->num_vertexes*3*sizeof(float), 1, iqmFile);
                memcpy(vertex, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*3*sizeof(float));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    int vCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*3; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*3; i++)
                    {
                        model.meshes[m].vertices[vCounter] = vertex[i];
                        model.meshes[m].animVertices[vCounter] = vertex[i];
                        vCounter++;
                    }
                }
            } break;
            case IQM_NORMAL:
            {
                normal = RL_MALLOC(iqmHeader->num_vertexes*3*sizeof(float));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(normal, iqmHeader->num_vertexes*3*sizeof(float), 1, iqmFile);
                memcpy(normal, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*3*sizeof(float));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    int vCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*3; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*3; i++)
                    {
                        model.meshes[m].normals[vCounter] = normal[i];
                        model.meshes[m].animNormals[vCounter] = normal[i];
                        vCounter++;
                    }
                }
            } break;
            case IQM_TEXCOORD:
            {
                text = RL_MALLOC(iqmHeader->num_vertexes*2*sizeof(float));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(text, iqmHeader->num_vertexes*2*sizeof(float), 1, iqmFile);
                memcpy(text, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*2*sizeof(float));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    int vCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*2; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*2; i++)
                    {
                        model.meshes[m].texcoords[vCounter] = text[i];
                        vCounter++;
                    }
                }
            } break;
            case IQM_BLENDINDEXES:
            {
                blendi = RL_MALLOC(iqmHeader->num_vertexes*4*sizeof(char));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(blendi, iqmHeader->num_vertexes*4*sizeof(char), 1, iqmFile);
                memcpy(blendi, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*4*sizeof(char));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    int boneCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*4; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*4; i++)
                    {
                        model.meshes[m].boneIds[boneCounter] = blendi[i];
                        boneCounter++;
                    }
                }
            } break;
            case IQM_BLENDWEIGHTS:
            {
                blendw = RL_MALLOC(iqmHeader->num_vertexes*4*sizeof(unsigned char));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(blendw, iqmHeader->num_vertexes*4*sizeof(unsigned char), 1, iqmFile);
                memcpy(blendw, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*4*sizeof(unsigned char));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    int boneCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*4; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*4; i++)
                    {
                        model.meshes[m].boneWeights[boneCounter] = blendw[i]/255.0f;
                        boneCounter++;
                    }
                }
            } break;
            case IQM_COLOR:
            {
                color = RL_MALLOC(iqmHeader->num_vertexes*4*sizeof(unsigned char));
                //fseek(iqmFile, va[i].offset, SEEK_SET);
                //fread(blendw, iqmHeader->num_vertexes*4*sizeof(unsigned char), 1, iqmFile);
                memcpy(color, fileDataPtr + va[i].offset, iqmHeader->num_vertexes*4*sizeof(unsigned char));

                for (unsigned int m = 0; m < iqmHeader->num_meshes; m++)
                {
                    model.meshes[m].colors = RL_CALLOC(model.meshes[m].vertexCount*4, sizeof(unsigned char));

                    int vCounter = 0;
                    for (unsigned int i = imesh[m].first_vertex*4; i < (imesh[m].first_vertex + imesh[m].num_vertexes)*4; i++)
                    {
                        model.meshes[m].colors[vCounter] = color[i];
                        vCounter++;
                    }
                }
            } break;
        }
    }

    // Bones (joints) data processing
    ijoint = RL_MALLOC(iqmHeader->num_joints*sizeof(IQMJoint));
    //fseek(iqmFile, iqmHeader->ofs_joints, SEEK_SET);
    //fread(ijoint, iqmHeader->num_joints*sizeof(IQMJoint), 1, iqmFile);
    memcpy(ijoint, fileDataPtr + iqmHeader->ofs_joints, iqmHeader->num_joints*sizeof(IQMJoint));

    model.boneCount = iqmHeader->num_joints;
    model.bones = RL_MALLOC(iqmHeader->num_joints*sizeof(BoneInfo));
    model.bindPose = RL_MALLOC(iqmHeader->num_joints*sizeof(Transform));

    for (unsigned int i = 0; i < iqmHeader->num_joints; i++)
    {
        // Bones
        model.bones[i].parent = ijoint[i].parent;
        //fseek(iqmFile, iqmHeader->ofs_text + ijoint[i].name, SEEK_SET);
        //fread(model.bones[i].name, BONE_NAME_LENGTH*sizeof(char), 1, iqmFile);
        memcpy(model.bones[i].name, fileDataPtr + iqmHeader->ofs_text + ijoint[i].name, BONE_NAME_LENGTH*sizeof(char));

        // Bind pose (base pose)
        model.bindPose[i].translation.x = ijoint[i].translate[0];
        model.bindPose[i].translation.y = ijoint[i].translate[1];
        model.bindPose[i].translation.z = ijoint[i].translate[2];

        model.bindPose[i].rotation.x = ijoint[i].rotate[0];
        model.bindPose[i].rotation.y = ijoint[i].rotate[1];
        model.bindPose[i].rotation.z = ijoint[i].rotate[2];
        model.bindPose[i].rotation.w = ijoint[i].rotate[3];

        model.bindPose[i].scale.x = ijoint[i].scale[0];
        model.bindPose[i].scale.y = ijoint[i].scale[1];
        model.bindPose[i].scale.z = ijoint[i].scale[2];
    }

    // Build bind pose from parent joints
    for (int i = 0; i < model.boneCount; i++)
    {
        if (model.bones[i].parent >= 0)
        {
            model.bindPose[i].rotation = QuaternionMultiply(model.bindPose[model.bones[i].parent].rotation, model.bindPose[i].rotation);
            model.bindPose[i].translation = Vector3RotateByQuaternion(model.bindPose[i].translation, model.bindPose[model.bones[i].parent].rotation);
            model.bindPose[i].translation = Vector3Add(model.bindPose[i].translation, model.bindPose[model.bones[i].parent].translation);
            model.bindPose[i].scale = Vector3Multiply(model.bindPose[i].scale, model.bindPose[model.bones[i].parent].scale);
        }
    }

    RL_FREE(fileData);

    RL_FREE(imesh);
    RL_FREE(tri);
    RL_FREE(va);
    RL_FREE(vertex);
    RL_FREE(normal);
    RL_FREE(text);
    RL_FREE(blendi);
    RL_FREE(blendw);
    RL_FREE(ijoint);

    return model;
}

// Load IQM animation data
static ModelAnimation *LoadModelAnimationsIQM(const char *fileName, unsigned int *animCount)
{
    #define IQM_MAGIC       "INTERQUAKEMODEL"   // IQM file magic number
    #define IQM_VERSION     2                   // only IQM version 2 supported

    unsigned int fileSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &fileSize);
    unsigned char *fileDataPtr = fileData;

    typedef struct IQMHeader {
        char magic[16];
        unsigned int version;
        unsigned int filesize;
        unsigned int flags;
        unsigned int num_text, ofs_text;
        unsigned int num_meshes, ofs_meshes;
        unsigned int num_vertexarrays, num_vertexes, ofs_vertexarrays;
        unsigned int num_triangles, ofs_triangles, ofs_adjacency;
        unsigned int num_joints, ofs_joints;
        unsigned int num_poses, ofs_poses;
        unsigned int num_anims, ofs_anims;
        unsigned int num_frames, num_framechannels, ofs_frames, ofs_bounds;
        unsigned int num_comment, ofs_comment;
        unsigned int num_extensions, ofs_extensions;
    } IQMHeader;

    typedef struct IQMPose {
        int parent;
        unsigned int mask;
        float channeloffset[10];
        float channelscale[10];
    } IQMPose;

    typedef struct IQMAnim {
        unsigned int name;
        unsigned int first_frame, num_frames;
        float framerate;
        unsigned int flags;
    } IQMAnim;

    // In case file can not be read, return an empty model
    if (fileDataPtr == NULL) return NULL;

    // Read IQM header
    IQMHeader *iqmHeader = (IQMHeader *)fileDataPtr;

    if (memcmp(iqmHeader->magic, IQM_MAGIC, sizeof(IQM_MAGIC)) != 0)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] IQM file is not a valid model", fileName);
        return NULL;
    }

    if (iqmHeader->version != IQM_VERSION)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] IQM file version not supported (%i)", fileName, iqmHeader->version);
        return NULL;
    }

    // Get bones data
    IQMPose *poses = RL_MALLOC(iqmHeader->num_poses*sizeof(IQMPose));
    //fseek(iqmFile, iqmHeader->ofs_poses, SEEK_SET);
    //fread(poses, iqmHeader->num_poses*sizeof(IQMPose), 1, iqmFile);
    memcpy(poses, fileDataPtr + iqmHeader->ofs_poses, iqmHeader->num_poses*sizeof(IQMPose));

    // Get animations data
    *animCount = iqmHeader->num_anims;
    IQMAnim *anim = RL_MALLOC(iqmHeader->num_anims*sizeof(IQMAnim));
    //fseek(iqmFile, iqmHeader->ofs_anims, SEEK_SET);
    //fread(anim, iqmHeader->num_anims*sizeof(IQMAnim), 1, iqmFile);
    memcpy(anim, fileDataPtr + iqmHeader->ofs_anims, iqmHeader->num_anims*sizeof(IQMAnim));

    ModelAnimation *animations = RL_MALLOC(iqmHeader->num_anims*sizeof(ModelAnimation));

    // frameposes
    unsigned short *framedata = RL_MALLOC(iqmHeader->num_frames*iqmHeader->num_framechannels*sizeof(unsigned short));
    //fseek(iqmFile, iqmHeader->ofs_frames, SEEK_SET);
    //fread(framedata, iqmHeader->num_frames*iqmHeader->num_framechannels*sizeof(unsigned short), 1, iqmFile);
    memcpy(framedata, fileDataPtr + iqmHeader->ofs_frames, iqmHeader->num_frames*iqmHeader->num_framechannels*sizeof(unsigned short));

    for (unsigned int a = 0; a < iqmHeader->num_anims; a++)
    {
        animations[a].frameCount = anim[a].num_frames;
        animations[a].boneCount = iqmHeader->num_poses;
        animations[a].bones = RL_MALLOC(iqmHeader->num_poses*sizeof(BoneInfo));
        animations[a].framePoses = RL_MALLOC(anim[a].num_frames*sizeof(Transform *));
        // animations[a].framerate = anim.framerate;     // TODO: Use framerate?

        for (unsigned int j = 0; j < iqmHeader->num_poses; j++)
        {
            strcpy(animations[a].bones[j].name, "ANIMJOINTNAME");
            animations[a].bones[j].parent = poses[j].parent;
        }

        for (unsigned int j = 0; j < anim[a].num_frames; j++) animations[a].framePoses[j] = RL_MALLOC(iqmHeader->num_poses*sizeof(Transform));

        int dcounter = anim[a].first_frame*iqmHeader->num_framechannels;

        for (unsigned int frame = 0; frame < anim[a].num_frames; frame++)
        {
            for (unsigned int i = 0; i < iqmHeader->num_poses; i++)
            {
                animations[a].framePoses[frame][i].translation.x = poses[i].channeloffset[0];

                if (poses[i].mask & 0x01)
                {
                    animations[a].framePoses[frame][i].translation.x += framedata[dcounter]*poses[i].channelscale[0];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].translation.y = poses[i].channeloffset[1];

                if (poses[i].mask & 0x02)
                {
                    animations[a].framePoses[frame][i].translation.y += framedata[dcounter]*poses[i].channelscale[1];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].translation.z = poses[i].channeloffset[2];

                if (poses[i].mask & 0x04)
                {
                    animations[a].framePoses[frame][i].translation.z += framedata[dcounter]*poses[i].channelscale[2];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.x = poses[i].channeloffset[3];

                if (poses[i].mask & 0x08)
                {
                    animations[a].framePoses[frame][i].rotation.x += framedata[dcounter]*poses[i].channelscale[3];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.y = poses[i].channeloffset[4];

                if (poses[i].mask & 0x10)
                {
                    animations[a].framePoses[frame][i].rotation.y += framedata[dcounter]*poses[i].channelscale[4];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.z = poses[i].channeloffset[5];

                if (poses[i].mask & 0x20)
                {
                    animations[a].framePoses[frame][i].rotation.z += framedata[dcounter]*poses[i].channelscale[5];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation.w = poses[i].channeloffset[6];

                if (poses[i].mask & 0x40)
                {
                    animations[a].framePoses[frame][i].rotation.w += framedata[dcounter]*poses[i].channelscale[6];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].scale.x = poses[i].channeloffset[7];

                if (poses[i].mask & 0x80)
                {
                    animations[a].framePoses[frame][i].scale.x += framedata[dcounter]*poses[i].channelscale[7];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].scale.y = poses[i].channeloffset[8];

                if (poses[i].mask & 0x100)
                {
                    animations[a].framePoses[frame][i].scale.y += framedata[dcounter]*poses[i].channelscale[8];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].scale.z = poses[i].channeloffset[9];

                if (poses[i].mask & 0x200)
                {
                    animations[a].framePoses[frame][i].scale.z += framedata[dcounter]*poses[i].channelscale[9];
                    dcounter++;
                }

                animations[a].framePoses[frame][i].rotation = QuaternionNormalize(animations[a].framePoses[frame][i].rotation);
            }
        }

        // Build frameposes
        for (unsigned int frame = 0; frame < anim[a].num_frames; frame++)
        {
            for (int i = 0; i < animations[a].boneCount; i++)
            {
                if (animations[a].bones[i].parent >= 0)
                {
                    animations[a].framePoses[frame][i].rotation = QuaternionMultiply(animations[a].framePoses[frame][animations[a].bones[i].parent].rotation, animations[a].framePoses[frame][i].rotation);
                    animations[a].framePoses[frame][i].translation = Vector3RotateByQuaternion(animations[a].framePoses[frame][i].translation, animations[a].framePoses[frame][animations[a].bones[i].parent].rotation);
                    animations[a].framePoses[frame][i].translation = Vector3Add(animations[a].framePoses[frame][i].translation, animations[a].framePoses[frame][animations[a].bones[i].parent].translation);
                    animations[a].framePoses[frame][i].scale = Vector3Multiply(animations[a].framePoses[frame][i].scale, animations[a].framePoses[frame][animations[a].bones[i].parent].scale);
                }
            }
        }
    }

    RL_FREE(fileData);

    RL_FREE(framedata);
    RL_FREE(poses);
    RL_FREE(anim);

    return animations;
}

#endif

#if defined(SUPPORT_FILEFORMAT_GLTF)
// Load image from different glTF provided methods (uri, path, buffer_view)
static Image LoadImageFromCgltfImage(cgltf_image *cgltfImage, const char *texPath)
{
    Image image = { 0 };

    if (cgltfImage->uri != NULL)     // Check if image data is provided as a uri (base64 or path)
    {
        if ((strlen(cgltfImage->uri) > 5) &&
            (cgltfImage->uri[0] == 'd') &&
            (cgltfImage->uri[1] == 'a') &&
            (cgltfImage->uri[2] == 't') &&
            (cgltfImage->uri[3] == 'a') &&
            (cgltfImage->uri[4] == ':'))     // Check if image is provided as base64 text data
        {
            // Data URI Format: data:<mediatype>;base64,<data>

            // Find the comma
            int i = 0;
            while ((cgltfImage->uri[i] != ',') && (cgltfImage->uri[i] != 0)) i++;

            if (cgltfImage->uri[i] == 0) TRACELOG(LOG_WARNING, "IMAGE: glTF data URI is not a valid image");
            else
            {
                int base64Size = (int)strlen(cgltfImage->uri + i + 1);
                int outSize = 3*(base64Size/4);         // TODO: Consider padding (-numberOfPaddingCharacters)
                void *data = NULL;

                cgltf_options options = { 0 };
                cgltf_result result = cgltf_load_buffer_base64(&options, outSize, cgltfImage->uri + i + 1, &data);

                if (result == cgltf_result_success)
                {
                    image = LoadImageFromMemory(".png", (unsigned char *)data, outSize);
                    cgltf_free((cgltf_data*)data);
                }
            }
        }
        else     // Check if image is provided as image path
        {
            image = LoadImage(TextFormat("%s/%s", texPath, cgltfImage->uri));
        }
    }
    else if (cgltfImage->buffer_view->buffer->data != NULL)    // Check if image is provided as data buffer
    {
        unsigned char *data = RL_MALLOC(cgltfImage->buffer_view->size);
        int offset = (int)cgltfImage->buffer_view->offset;
        int stride = (int)cgltfImage->buffer_view->stride? (int)cgltfImage->buffer_view->stride : 1;

        // Copy buffer data to memory for loading
        for (unsigned int i = 0; i < cgltfImage->buffer_view->size; i++)
        {
            data[i] = ((unsigned char *)cgltfImage->buffer_view->buffer->data)[offset];
            offset += stride;
        }

        // Check mime_type for image: (cgltfImage->mime_type == "image/png")
        // NOTE: Detected that some models define mime_type as "image\\/png"
        if ((strcmp(cgltfImage->mime_type, "image\\/png") == 0) ||
            (strcmp(cgltfImage->mime_type, "image/png") == 0)) image = LoadImageFromMemory(".png", data, (int)cgltfImage->buffer_view->size);
        else if ((strcmp(cgltfImage->mime_type, "image\\/jpeg") == 0) ||
                 (strcmp(cgltfImage->mime_type, "image/jpeg") == 0)) image = LoadImageFromMemory(".jpg", data, (int)cgltfImage->buffer_view->size);
        else TRACELOG(LOG_WARNING, "MODEL: glTF image data MIME type not recognized", TextFormat("%s/%s", texPath, cgltfImage->uri));

        RL_FREE(data);
    }

    return image;
}

// Load glTF file into model struct, .gltf and .glb supported
static Model LoadGLTF(const char *fileName)
{
    /*********************************************************************************************

        Function implemented by Wilhem Barbier(@wbrbr), with modifications by Tyler Bezera(@gamerfiend)
        Reviewed by Ramon Santamaria (@raysan5)

        FEATURES:
          - Supports .gltf and .glb files
          - Supports embedded (base64) or external textures
          - Supports PBR metallic/roughness flow, loads material textures, values and colors
                     PBR specular/glossiness flow and extended texture flows not supported
          - Supports multiple meshes per model (every primitives is loaded as a separate mesh)

        RESTRICTIONS:
          - Only triangle meshes supported
          - Vertex attibute types and formats supported:
              > Vertices (position): vec3: float
              > Normals: vec3: float
              > Texcoords: vec2: float
              > Colors: vec4: u8, u16, f32 (normalized)
              > Indices: u16, u32 (truncated to u16)
          - Node hierarchies or transforms not supported

    ***********************************************************************************************/

    // Macro to simplify attributes loading code
    #define LOAD_ATTRIBUTE(accesor, numComp, dataType, dstPtr) \
    { \
        int n = 0; \
        dataType *buffer = (dataType *)accesor->buffer_view->buffer->data + accesor->buffer_view->offset/sizeof(dataType) + accesor->offset/sizeof(dataType); \
        for (unsigned int k = 0; k < accesor->count; k++) \
        {\
            for (int l = 0; l < numComp; l++) \
            {\
                dstPtr[numComp*k + l] = buffer[n + l];\
            }\
            n += (int)(accesor->stride/sizeof(dataType));\
        }\
    }

    Model model = { 0 };

    // glTF file loading
    unsigned int dataSize = 0;
    unsigned char *fileData = LoadFileData(fileName, &dataSize);

    if (fileData == NULL) return model;

    // glTF data loading
    cgltf_options options = { 0 };
    cgltf_data *data = NULL;
    cgltf_result result = cgltf_parse(&options, fileData, dataSize, &data);

    if (result == cgltf_result_success)
    {
        if (data->file_type == cgltf_file_type_glb) TRACELOG(LOG_INFO, "MODEL: [%s] Model basic data (glb) loaded successfully", fileName);
        else if (data->file_type == cgltf_file_type_gltf) TRACELOG(LOG_INFO, "MODEL: [%s] Model basic data (glTF) loaded successfully", fileName);
        else TRACELOG(LOG_WARNING, "MODEL: [%s] Model format not recognized", fileName);

        TRACELOG(LOG_INFO, "    > Meshes count: %i", data->meshes_count);
        TRACELOG(LOG_INFO, "    > Materials count: %i (+1 default)", data->materials_count);
        TRACELOG(LOG_DEBUG, "    > Buffers count: %i", data->buffers_count);
        TRACELOG(LOG_DEBUG, "    > Images count: %i", data->images_count);
        TRACELOG(LOG_DEBUG, "    > Textures count: %i", data->textures_count);

        // Force reading data buffers (fills buffer_view->buffer->data)
        // NOTE: If an uri is defined to base64 data or external path, it's automatically loaded -> TODO: Verify this assumption
        result = cgltf_load_buffers(&options, data, fileName);
        if (result != cgltf_result_success) TRACELOG(LOG_INFO, "MODEL: [%s] Failed to load mesh/material buffers", fileName);

        int primitivesCount = 0;
        // NOTE: We will load every primitive in the glTF as a separate raylib mesh
        for (unsigned int i = 0; i < data->meshes_count; i++) primitivesCount += (int)data->meshes[i].primitives_count;

        // Load our model data: meshes and materials
        model.meshCount = primitivesCount;
        model.meshes = RL_CALLOC(model.meshCount, sizeof(Mesh));
        for (int i = 0; i < model.meshCount; i++) model.meshes[i].vboId = (unsigned int*)RL_CALLOC(MAX_MESH_VERTEX_BUFFERS, sizeof(unsigned int));

        // NOTE: We keep an extra slot for default material, in case some mesh requires it
        model.materialCount = (int)data->materials_count + 1;
        model.materials = RL_CALLOC(model.materialCount, sizeof(Material));
        model.materials[0] = LoadMaterialDefault();     // Load default material (index: 0)

        // Load mesh-material indices, by default all meshes are mapped to material index: 0
        model.meshMaterial = RL_CALLOC(model.meshCount, sizeof(int));

        // Load materials data
        //----------------------------------------------------------------------------------------------------
        for (unsigned int i = 0, j = 1; i < data->materials_count; i++, j++)
        {
            model.materials[j] = LoadMaterialDefault();
            const char *texPath = GetDirectoryPath(fileName);

            // Check glTF material flow: PBR metallic/roughness flow
            // NOTE: Alternatively, materials can follow PBR specular/glossiness flow
            if (data->materials[i].has_pbr_metallic_roughness)
            {
                // Load base color texture (albedo)
                if (data->materials[i].pbr_metallic_roughness.base_color_texture.texture)
                {
                    Image imAlbedo = LoadImageFromCgltfImage(data->materials[i].pbr_metallic_roughness.base_color_texture.texture->image, texPath);
                    if (imAlbedo.data != NULL)
                    {
                        model.materials[j].maps[MATERIAL_MAP_ALBEDO].texture = LoadTextureFromImage(imAlbedo);
                        UnloadImage(imAlbedo);
                    }
                }
                // Load base color factor (tint)
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.r = (unsigned char)(data->materials[i].pbr_metallic_roughness.base_color_factor[0]*255);
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.g = (unsigned char)(data->materials[i].pbr_metallic_roughness.base_color_factor[1]*255);
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.b = (unsigned char)(data->materials[i].pbr_metallic_roughness.base_color_factor[2]*255);
                model.materials[j].maps[MATERIAL_MAP_ALBEDO].color.a = (unsigned char)(data->materials[i].pbr_metallic_roughness.base_color_factor[3]*255);

                // Load metallic/roughness texture
                if (data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture)
                {
                    Image imMetallicRoughness = LoadImageFromCgltfImage(data->materials[i].pbr_metallic_roughness.metallic_roughness_texture.texture->image, texPath);
                    if (imMetallicRoughness.data != NULL)
                    {
                        model.materials[j].maps[MATERIAL_MAP_ROUGHNESS].texture = LoadTextureFromImage(imMetallicRoughness);
                        UnloadImage(imMetallicRoughness);
                    }

                    // Load metallic/roughness material properties
                    float roughness = data->materials[i].pbr_metallic_roughness.roughness_factor;
                    model.materials[j].maps[MATERIAL_MAP_ROUGHNESS].value = roughness;

                    float metallic = data->materials[i].pbr_metallic_roughness.metallic_factor;
                    model.materials[j].maps[MATERIAL_MAP_METALNESS].value = metallic;
                }

                // Load normal texture
                if (data->materials[i].normal_texture.texture)
                {
                    Image imNormal = LoadImageFromCgltfImage(data->materials[i].normal_texture.texture->image, texPath);
                    if (imNormal.data != NULL)
                    {
                        model.materials[j].maps[MATERIAL_MAP_NORMAL].texture = LoadTextureFromImage(imNormal);
                        UnloadImage(imNormal);
                    }
                }

                // Load ambient occlusion texture
                if (data->materials[i].occlusion_texture.texture)
                {
                    Image imOcclusion = LoadImageFromCgltfImage(data->materials[i].occlusion_texture.texture->image, texPath);
                    if (imOcclusion.data != NULL)
                    {
                        model.materials[j].maps[MATERIAL_MAP_OCCLUSION].texture = LoadTextureFromImage(imOcclusion);
                        UnloadImage(imOcclusion);
                    }
                }

                // Load emissive texture
                if (data->materials[i].emissive_texture.texture)
                {
                    Image imEmissive = LoadImageFromCgltfImage(data->materials[i].emissive_texture.texture->image, texPath);
                    if (imEmissive.data != NULL)
                    {
                        model.materials[j].maps[MATERIAL_MAP_EMISSION].texture = LoadTextureFromImage(imEmissive);
                        UnloadImage(imEmissive);
                    }

                    // Load emissive color factor
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.r = (unsigned char)(data->materials[i].emissive_factor[0]*255);
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.g = (unsigned char)(data->materials[i].emissive_factor[1]*255);
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.b = (unsigned char)(data->materials[i].emissive_factor[2]*255);
                    model.materials[j].maps[MATERIAL_MAP_EMISSION].color.a = 255;
                }
            }

            // Other possible materials not supported by raylib pipeline:
            // has_clearcoat, has_transmission, has_volume, has_ior, has specular, has_sheen
        }

        // Load meshes data
        //----------------------------------------------------------------------------------------------------
        for (unsigned int i = 0, meshIndex = 0; i < data->meshes_count; i++)
        {
            // NOTE: meshIndex accumulates primitives

            for (unsigned int p = 0; p < data->meshes[i].primitives_count; p++)
            {
                // NOTE: We only support primitives defined by triangles
                // Other alternatives: points, lines, line_strip, triangle_strip
                if (data->meshes[i].primitives[p].type != cgltf_primitive_type_triangles) continue;

                // NOTE: Attributes data could be provided in several data formats (8, 8u, 16u, 32...),
                // Only some formats for each attribute type are supported, read info at the top of this function!

                for (unsigned int j = 0; j < data->meshes[i].primitives[p].attributes_count; j++)
                {
                    // Check the different attributes for every pimitive
                    if (data->meshes[i].primitives[p].attributes[j].type == cgltf_attribute_type_position)      // POSITION
                    {
                        cgltf_accessor *attribute = data->meshes[i].primitives[p].attributes[j].data;

                        // WARNING: SPECS: POSITION accessor MUST have its min and max properties defined.

                        if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec3))
                        {
                            // Init raylib mesh vertices to copy glTF attribute data
                            model.meshes[meshIndex].vertexCount = (int)attribute->count;
                            model.meshes[meshIndex].vertices = RL_MALLOC(attribute->count*3*sizeof(float));

                            // Load 3 components of float data type into mesh.vertices
                            LOAD_ATTRIBUTE(attribute, 3, float, model.meshes[meshIndex].vertices)
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Vertices attribute data format not supported, use vec3 float", fileName);
                    }
                    else if (data->meshes[i].primitives[p].attributes[j].type == cgltf_attribute_type_normal)   // NORMAL
                    {
                        cgltf_accessor *attribute = data->meshes[i].primitives[p].attributes[j].data;

                        if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec3))
                        {
                            // Init raylib mesh normals to copy glTF attribute data
                            model.meshes[meshIndex].normals = RL_MALLOC(attribute->count*3*sizeof(float));

                            // Load 3 components of float data type into mesh.normals
                            LOAD_ATTRIBUTE(attribute, 3, float, model.meshes[meshIndex].normals)
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Normal attribute data format not supported, use vec3 float", fileName);
                    }
                    else if (data->meshes[i].primitives[p].attributes[j].type == cgltf_attribute_type_tangent)   // TANGENT
                    {
                        cgltf_accessor *attribute = data->meshes[i].primitives[p].attributes[j].data;

                        if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec4))
                        {
                            // Init raylib mesh tangent to copy glTF attribute data
                            model.meshes[meshIndex].tangents = RL_MALLOC(attribute->count*4*sizeof(float));

                            // Load 4 components of float data type into mesh.tangents
                            LOAD_ATTRIBUTE(attribute, 4, float, model.meshes[meshIndex].tangents)
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Tangent attribute data format not supported, use vec4 float", fileName);
                    }
                    else if (data->meshes[i].primitives[p].attributes[j].type == cgltf_attribute_type_texcoord) // TEXCOORD_0
                    {
                        // TODO: Support additional texture coordinates: TEXCOORD_1 -> mesh.texcoords2

                        cgltf_accessor *attribute = data->meshes[i].primitives[p].attributes[j].data;

                        if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec2))
                        {
                            // Init raylib mesh texcoords to copy glTF attribute data
                            model.meshes[meshIndex].texcoords = RL_MALLOC(attribute->count*2*sizeof(float));

                            // Load 3 components of float data type into mesh.texcoords
                            LOAD_ATTRIBUTE(attribute, 2, float, model.meshes[meshIndex].texcoords)
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Texcoords attribute data format not supported, use vec2 float", fileName);
                    }
                    else if (data->meshes[i].primitives[p].attributes[j].type == cgltf_attribute_type_color)    // COLOR_0
                    {
                        cgltf_accessor *attribute = data->meshes[i].primitives[p].attributes[j].data;

                        // WARNING: SPECS: All components of each COLOR_n accessor element MUST be clamped to [0.0, 1.0] range.

                        if ((attribute->component_type == cgltf_component_type_r_8u) && (attribute->type == cgltf_type_vec4))
                        {
                            // Init raylib mesh color to copy glTF attribute data
                            model.meshes[meshIndex].colors = RL_MALLOC(attribute->count*4*sizeof(unsigned char));

                            // Load 4 components of unsigned char data type into mesh.colors
                            LOAD_ATTRIBUTE(attribute, 4, unsigned char, model.meshes[meshIndex].colors)
                        }
                        else if ((attribute->component_type == cgltf_component_type_r_16u) && (attribute->type == cgltf_type_vec4))
                        {
                            // Init raylib mesh color to copy glTF attribute data
                            model.meshes[meshIndex].colors = RL_MALLOC(attribute->count*4*sizeof(unsigned char));

                            // Load data into a temp buffer to be converted to raylib data type
                            unsigned short *temp = RL_MALLOC(attribute->count*4*sizeof(unsigned short));
                            LOAD_ATTRIBUTE(attribute, 4, unsigned short, temp);

                            // Convert data to raylib color data type (4 bytes)
                            for (int c = 0; c < attribute->count*4; c++) model.meshes[meshIndex].colors[c] = (unsigned char)(((float)temp[c]/65535.0f)*255.0f);

                            RL_FREE(temp);
                        }
                        else if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec4))
                        {
                            // Init raylib mesh color to copy glTF attribute data
                            model.meshes[meshIndex].colors = RL_MALLOC(attribute->count*4*sizeof(unsigned char));

                            // Load data into a temp buffer to be converted to raylib data type
                            float *temp = RL_MALLOC(attribute->count*4*sizeof(float));
                            LOAD_ATTRIBUTE(attribute, 4, float, temp);

                            // Convert data to raylib color data type (4 bytes), we expect the color data normalized
                            for (int c = 0; c < attribute->count*4; c++) model.meshes[meshIndex].colors[c] = (unsigned char)(temp[c]*255.0f);

                            RL_FREE(temp);
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Color attribute data format not supported", fileName);
                    }

                    // NOTE: Attributes related to animations are processed separately
                }

                // Load primitive indices data (if provided)
                if (data->meshes[i].primitives[p].indices != NULL)
                {
                    cgltf_accessor *attribute = data->meshes[i].primitives[p].indices;

                    model.meshes[meshIndex].triangleCount = (int)attribute->count/3;

                    if (attribute->component_type == cgltf_component_type_r_16u)
                    {
                        // Init raylib mesh indices to copy glTF attribute data
                        model.meshes[meshIndex].indices = RL_MALLOC(attribute->count*sizeof(unsigned short));

                        // Load unsigned short data type into mesh.indices
                        LOAD_ATTRIBUTE(attribute, 1, unsigned short, model.meshes[meshIndex].indices)
                    }
                    else if (attribute->component_type == cgltf_component_type_r_32u)
                    {
                        // Init raylib mesh indices to copy glTF attribute data
                        model.meshes[meshIndex].indices = RL_MALLOC(attribute->count*sizeof(unsigned short));

                        // Load data into a temp buffer to be converted to raylib data type
                        unsigned int *temp = RL_MALLOC(attribute->count*sizeof(unsigned int));
                        LOAD_ATTRIBUTE(attribute, 1, unsigned int, temp);

                        // Convert data to raylib indices data type (unsigned short)
                        for (int d = 0; d < attribute->count; d++) model.meshes[meshIndex].indices[d] = (unsigned short)temp[d];

                        TRACELOG(LOG_WARNING, "MODEL: [%s] Indices data converted from u32 to u16, possible loss of data", fileName);

                        RL_FREE(temp);
                    }
                    else TRACELOG(LOG_WARNING, "MODEL: [%s] Indices data format not supported, use u16", fileName);
                }
                else model.meshes[meshIndex].triangleCount = model.meshes[meshIndex].vertexCount/3;    // Unindexed mesh

                // Assign to the primitive mesh the corresponding material index
                // NOTE: If no material defined, mesh uses the already assigned default material (index: 0)
                for (int m = 0; m < data->materials_count; m++)
                {
                    // The primitive actually keeps the pointer to the corresponding material,
                    // raylib instead assigns to the mesh the by its index, as loaded in model.materials array
                    // To get the index, we check if material pointers match and we assign the corresponding index,
                    // skipping index 0, the default material
                    if (&data->materials[m] == data->meshes[i].primitives[p].material)
                    {
                        model.meshMaterial[meshIndex] = m + 1;
                        break;
                    }
                }

                meshIndex++;       // Move to next mesh
            }
        }

/*
        // TODO: Load glTF meshes animation data
        // REF: https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#skins
        // REF: https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#skinned-mesh-attributes
        //----------------------------------------------------------------------------------------------------
        for (unsigned int i = 0, meshIndex = 0; i < data->meshes_count; i++)
        {
            for (unsigned int p = 0; p < data->meshes[i].primitives_count; p++)
            {
                // NOTE: We only support primitives defined by triangles
                if (data->meshes[i].primitives[p].type != cgltf_primitive_type_triangles) continue;

                for (unsigned int j = 0; j < data->meshes[i].primitives[p].attributes_count; j++)
                {
                    // NOTE: JOINTS_1 + WEIGHT_1 will be used for +4 joints influencing a vertex -> Not supported by raylib

                    if (data->meshes[i].primitives[p].attributes[j].type == cgltf_attribute_type_joints)        // JOINTS_n (vec4: 4 bones max per vertex / u8, u16)
                    {
                        cgltf_accessor *attribute = data->meshes[i].primitives[p].attributes[j].data;

                        if ((attribute->component_type == cgltf_component_type_r_8u) && (attribute->type == cgltf_type_vec4))
                        {
                            // Init raylib mesh bone ids to copy glTF attribute data
                            model.meshes[meshIndex].boneIds = RL_CALLOC(model.meshes[meshIndex].vertexCount*4, sizeof(unsigned char));

                            // Load 4 components of unsigned char data type into mesh.boneIds
                            LOAD_ATTRIBUTE(attribute, 4, unsigned char, model.meshes[meshIndex].boneIds)
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Joint attribute data format not supported, use vec4 u8", fileName);
                    }
                    else if (data->meshes[i].primitives[p].attributes[j].type == cgltf_attribute_type_weights)  // WEIGHTS_n (vec4 / u8, u16, f32)
                    {
                        cgltf_accessor *attribute = data->meshes[i].primitives[p].attributes[j].data;

                        if ((attribute->component_type == cgltf_component_type_r_32f) && (attribute->type == cgltf_type_vec4))
                        {
                            // Init raylib mesh bone weight to copy glTF attribute data
                            model.meshes[meshIndex].boneWeights = RL_CALLOC(model.meshes[meshIndex].vertexCount*4, sizeof(float));

                            // Load 4 components of float data type into mesh.boneWeights
                            LOAD_ATTRIBUTE(attribute, 4, float, model.meshes[meshIndex].boneWeights)
                        }
                        else TRACELOG(LOG_WARNING, "MODEL: [%s] Joint weight attribute data format not supported, use vec4 float", fileName);
                    }
                }

                meshIndex++;       // Move to next mesh
            }
        }
*/
        // Free all cgltf loaded data
        cgltf_free(data);
    }
    else TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load glTF data", fileName);

    // WARNING: cgltf requires the file pointer available while reading data
    UnloadFileData(fileData);

    return model;
}
#endif

#if defined(SUPPORT_FILEFORMAT_VOX)
// Load VOX (MagicaVoxel) mesh data
static Model LoadVOX(const char *fileName)
{
    Model model = { 0 };

    int nbvertices = 0;
    int meshescount = 0;
    unsigned int fileSize = 0;
    unsigned char *fileData = NULL;

    // Read vox file into buffer
    fileData = LoadFileData(fileName, &fileSize);
    if (fileData == 0)
    {
        TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load VOX file", fileName);
        return model;
    }

    // Read and build voxarray description
    VoxArray3D voxarray = { 0 };
    int ret = Vox_LoadFromMemory(fileData, fileSize, &voxarray);

    if (ret != VOX_SUCCESS)
    {
        // Error
        UnloadFileData(fileData);

        TRACELOG(LOG_WARNING, "MODEL: [%s] Failed to load VOX data", fileName);
        return model;
    }
    else
    {
        // Success: Compute meshes count
        nbvertices = voxarray.vertices.used;
        meshescount = 1 + (nbvertices/65536);

        TRACELOG(LOG_INFO, "MODEL: [%s] VOX data loaded successfully : %i vertices/%i meshes", fileName, nbvertices, meshescount);
    }

    // Build models from meshes
    model.transform = MatrixIdentity();

    model.meshCount = meshescount;
    model.meshes = (Mesh *)RL_CALLOC(model.meshCount, sizeof(Mesh));

    model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));

    model.materialCount = 1;
    model.materials = (Material *)RL_CALLOC(model.materialCount, sizeof(Material));
    model.materials[0] = LoadMaterialDefault();

    // Init model meshes
    int verticesRemain = voxarray.vertices.used;
    int verticesMax = 65532; // 5461 voxels x 12 vertices per voxel -> 65532 (must be inf 65536)

    // 6*4 = 12 vertices per voxel
    Vector3 *pvertices = (Vector3 *)voxarray.vertices.array;
    Color *pcolors = (Color *)voxarray.colors.array;

    unsigned short *pindices = voxarray.indices.array;    // 5461*6*6 = 196596 indices max per mesh

    int size = 0;

    for (int i = 0; i < meshescount; i++)
    {
        Mesh *pmesh = &model.meshes[i];
        memset(pmesh, 0, sizeof(Mesh));

        // Copy vertices
        pmesh->vertexCount = (int)fmin(verticesMax, verticesRemain);

        size = pmesh->vertexCount*sizeof(float)*3;
        pmesh->vertices = RL_MALLOC(size);
        memcpy(pmesh->vertices, pvertices, size);

        // Copy indices
        // TODO: Compute globals indices array
        size = voxarray.indices.used*sizeof(unsigned short);
        pmesh->indices = RL_MALLOC(size);
        memcpy(pmesh->indices, pindices, size);

        pmesh->triangleCount = (pmesh->vertexCount/4)*2;

        // Copy colors
        size = pmesh->vertexCount*sizeof(Color);
        pmesh->colors = RL_MALLOC(size);
        memcpy(pmesh->colors, pcolors, size);

        // First material index
        model.meshMaterial[i] = 0;

        verticesRemain -= verticesMax;
        pvertices += verticesMax;
        pcolors += verticesMax;
    }

    // Free buffers
    Vox_FreeArrays(&voxarray);
    UnloadFileData(fileData);

    return model;
}
#endif

