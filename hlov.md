╔══════════════════════════════════════════════════════════════════════════════╗
║                    XXXXRunner2 Architecture Overview                         ║
║                     (Lua/JS/BASIC - All use same pattern)                    ║
╚══════════════════════════════════════════════════════════════════════════════╝

┌──────────────────────────────────────────────────────────────────────────────┐
│                              MAIN COMPONENTS                                  │
└──────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────┐
    │                        BaseRunner                           │
    │                   (Framework/BaseRunner.mm)                 │
    ├─────────────────────────────────────────────────────────────┤
    │ Responsibilities:                                           │
    │  • Creates & manages NSApplication                          │
    │  • Sets up menu bar (App/File/Edit/Window)                  │
    │  • Initializes SuperTerminal Framework                      │
    │  • Manages 60 FPS render loop (CVDisplayLink)               │
    │  • Handles application lifecycle                            │
    │  • Manages background script thread                         │
    │  • Clean shutdown coordination                              │
    │                                                             │
    │ Provides to subclasses:                                     │
    │  • displayManager, textGrid, graphicsLayer                  │
    │  • renderer, fontAtlas, spriteManager                       │
    │  • inputManager, audioManager                               │
    │  • window, running flag, frameCount                         │
    └─────────────────────────────────────────────────────────────┘
                            ▲
                            │ inherits
                            │
    ┌───────────────────────┴─────────────────────────────────────┐
    │                                                              │
    │              Language-Specific Runner                        │
    │         (LuaRunner2/JSRunner2/BasicRunner2)                  │
    ├──────────────────────────────────────────────────────────────┤
    │ Implements 3 methods:                                        │
    │                                                              │
    │  1. initializeRuntime()                                      │
    │     - Create language runtime (Lua/JS/BASIC)                 │
    │     - Set up language-specific callbacks                     │
    │     - Register API bindings                                  │
    │                                                              │
    │  2. loadAndExecuteScript()                                   │
    │     - Load script from file                                  │
    │     - Execute on background thread                           │
    │     - Returns when script completes                          │
    │                                                              │
    │  3. cleanupRuntime()                                         │
    │     - Stop runtime execution                                 │
    │     - Wake up blocked threads                                │
    │     - Release language resources                             │
    │                                                              │
    │ Language-Specific State:                                     │
    │  • Lua: lua_State*                                           │
    │  • JS:  JSRuntime*                                           │
    │  • BASIC: BASICInterpreter*                                  │
    │                                                              │
    │ Frame Sync:                                                  │
    │  • Global mutex/condition_variable                           │
    │  • wait_frame() / Frame.wait() / WAIT n                      │
    │  • Overrides onFrameTick() to signal frames                  │
    └──────────────────────────────────────────────────────────────┘


┌──────────────────────────────────────────────────────────────────────────────┐
│                           THREAD ARCHITECTURE                                 │
└──────────────────────────────────────────────────────────────────────────────┘

    Main Thread                     Background Thread            Display Thread
    (NSApp)                         (Script Execution)          (CVDisplayLink)
    ───────────                     ──────────────────          ───────────────
        │                                   │                          │
        │ applicationDidFinishLaunching     │                          │
        ├──────────────────────────────────>│                          │
        │                                   │                          │
        │                              initializeRuntime()             │
        │                              loadAndExecuteScript()           │
        │                                   │                          │
        │ startRenderLoop()                 │                          │
        ├────────────────────────────────────────────────────────────>│
        │                                   │                          │
        │                                   │                    onFrameTick()
        │                                   │                          │
        │                                   │                    • updateTime()
        │                                   │                    • beginFrame()
        │                                   │                    • renderFrame()
        │                                   │                    • endFrame()
        │                                   │                    • signal ready
        │                                   │<─ frame ready ────────┤
        │                                   │                          │
        │                              wait_frame()                    │
        │                              (blocks)                        │
        │                                   │                          │
        │                                   │                    onFrameTick()
        │                                   │<─ frame ready ────────┤
        │                                   │                          │
        │                              (continues)                     │
        │                                   │                          │
        │  [User presses Cmd+Q]             │                          │
        │                                   │                          │
        │ applicationWillTerminate          │                          │
        │ • stop display link               │                          X
        │ • cleanupRuntime()                │                          
        │   - stop interpreter ─────────────>│                          
        │   - signal threads                │                          
        │ • join script thread              │                          
        │<──────────────────────────────────┤                          
        │                                                               
        │ (exits cleanly)                                               


┌──────────────────────────────────────────────────────────────────────────────┐
│                        FRAMEWORK COMPONENTS USED                              │
└──────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────┐
    │ DisplayManager  │──────> Window + Metal view
    │                 │──────> Coordinates rendering
    └────────┬────────┘
             │ uses
             ├──────────────────────────────────┐
             │                                   │
    ┌────────▼────────┐              ┌──────────▼──────────┐
    │   TextGrid      │              │  MetalRenderer      │
    │   (80x25)       │              │  (Metal shaders)    │
    └─────────────────┘              └──────────┬──────────┘
                                                │ uses
                                     ┌──────────▼──────────┐
                                     │    FontAtlas        │
                                     │  (glyph texture)    │
                                     └─────────────────────┘

    ┌─────────────────┐              ┌─────────────────────┐
    │ GraphicsLayer   │              │  SpriteManager      │
    │ (primitives)    │              │  (sprite quads)     │
    └─────────────────┘              └─────────────────────┘

    ┌─────────────────┐              ┌─────────────────────┐
    │ InputManager    │              │  AudioManager       │
    │ (keyboard/mouse)│              │  (synth/MIDI)       │
    └─────────────────┘              └─────────────────────┘

                       All registered in
                    ┌──────────────────────┐
                    │  STApi::Context      │
                    │  (global singleton)  │
                    └──────────────────────┘


┌──────────────────────────────────────────────────────────────────────────────┐
│                             API FLOW                                          │
└──────────────────────────────────────────────────────────────────────────────┘

    User Script                  Language Binding           C API           Component
    ───────────                  ────────────────          ──────          ─────────
    
    Lua:
      text_put(x, y, "Hi")  -->  lua_st_text_put()  -->  st_text_put()  -->  TextGrid
      wait_frame()          -->  runner_wait_frame() -->  (sync wait)
    
    JavaScript:
      Text.put(x, y, "Hi")  -->  JSRuntime wrapper  -->  st_text_put()  -->  TextGrid
      Frame.wait()          -->  runtime callback   -->  (sync wait)
    
    BASIC:
      PRINT "Hi"            -->  interpreter        -->  st_text_put()  -->  TextGrid
      WAIT 60               -->  basicWait()        -->  (sync wait)


┌──────────────────────────────────────────────────────────────────────────────┐
│                         FRAME SYNCHRONIZATION                                 │
└──────────────────────────────────────────────────────────────────────────────┘

    Script calls wait_frame():
    
    1. Set g_frameReady = false
    2. Lock g_frameMutex
    3. g_frameCV.wait_for(lock, 100ms, [](){ return g_frameReady; })
       ├─ If timeout: return anyway (prevents deadlock)
       └─ If signaled: continue
    4. Unlock mutex
    5. Return to script
    
    
    Render thread signals frame:
    
    1. onFrameTick() called at 60 FPS
    2. Update timing, poll input
    3. Render current state
    4. Lock g_frameMutex
    5. Set g_frameReady = true
    6. g_frameCV.notify_one()
    7. Unlock mutex
    
    
    Result: Script runs at 60 FPS, synchronized with render loop


┌──────────────────────────────────────────────────────────────────────────────┐
│                            SHUTDOWN SEQUENCE                                  │
└──────────────────────────────────────────────────────────────────────────────┘

    User presses Cmd+Q:
    
    1. NSApp receives quit command
       └─> applicationWillTerminate: called
    
    2. BaseRunner::applicationWillTerminate:
       ├─> Set _running = false
       ├─> Stop CVDisplayLink (render thread)
       ├─> Call cleanupRuntime() (subclass)
       │   ├─> Lua:   lua_close()
       │   ├─> JS:    jsRuntime->stop()
       │   └─> BASIC: interpreter->stop(), wake threads
       ├─> Join script thread (wait for it to finish)
       └─> NSApp terminates
    
    Script thread:
    
    1. Currently in wait_frame() / WAIT
       └─> 100ms timeout expires OR woken by cleanup
    
    2. Returns from wait
       ├─> Lua:   checks condition, exits loop naturally
       ├─> JS:    checks condition, exits loop naturally  
       └─> BASIC: interpreter checks m_running, exits loop
    
    3. Script execution completes
    4. Thread exits
    
    Result: Clean shutdown, no deadlocks, no force quit needed!


┌──────────────────────────────────────────────────────────────────────────────┐
│                          CODE SIZE COMPARISON                                 │
└──────────────────────────────────────────────────────────────────────────────┘

    Before BaseRunner:
    ──────────────────
    Each runner: 400-900 lines
    - Menu bar code: ❌ None
    - Framework init: ~150 lines (duplicated)
    - Render loop: ~100 lines (duplicated)
    - Shutdown: ~50 lines (duplicated)
    - Language logic: ~200-600 lines
    
    After BaseRunner:
    ─────────────────
    BaseRunner: 437 lines (shared)
    Each runner: 200-400 lines
    - Menu bar code: ✅ Inherited
    - Framework init: ✅ Inherited
    - Render loop: ✅ Inherited
    - Shutdown: ✅ Inherited
    - Language logic: ~200-400 lines (only what's unique)
    
    Savings: ~1000 lines + consistent behavior + menu bar!


┌──────────────────────────────────────────────────────────────────────────────┐
│                           KEY DESIGN DECISIONS                                │
└──────────────────────────────────────────────────────────────────────────────┘

    ✅ Template Method Pattern
       - BaseRunner provides algorithm skeleton
       - Subclasses fill in language-specific steps
    
    ✅ Composition over Complex Inheritance
       - BaseRunner exposes framework components as properties
       - Subclasses use them, don't inherit complexity
    
    ✅ Global Frame Sync (per runner)
       - Simple static variables for mutex/cv
       - No complex callback registration needed
       - Easy to understand and debug
    
    ✅ Timeout-Based Waiting
       - wait_for() with 100ms timeout prevents deadlocks
       - Allows responsive shutdown
       - No need for complex interrupt mechanisms
    
    ✅ Minimal Interface
       - Only 3 methods to implement
       - Clear responsibilities
       - Hard to get wrong


┌──────────────────────────────────────────────────────────────────────────────┐
│                            ADDING NEW RUNNERS                                 │
└──────────────────────────────────────────────────────────────────────────────┘

    To add a new language (e.g., PythonRunner2):
    
    1. Create PythonRunner2/main.mm:
    
       @interface PythonRunner2App : BaseRunner
       @end
       
       @implementation PythonRunner2App {
           PyObject* _pythonState;
       }
       
       - (BOOL)initializeRuntime {
           // Initialize Python interpreter
           // Register Python bindings
       }
       
       - (BOOL)loadAndExecuteScript {
           // Load and run Python script
       }
       
       - (void)cleanupRuntime {
           // Clean up Python
       }
       @end
    
    2. Set up frame sync (copy pattern from Lua/JS)
    
    3. Done! You get menu bar, framework, render loop for free!
    
    Typical implementation: ~200-300 lines


╔══════════════════════════════════════════════════════════════════════════════╗
║                              END OF OVERVIEW                                  ║
╚══════════════════════════════════════════════════════════════════════════════╝
