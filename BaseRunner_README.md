# BaseRunner - Common Base Class for Runner Applications

## Overview

`BaseRunner` is an abstract base class that provides common functionality for all SuperTerminal runner applications (BasicRunner2, LuaRunner2, AppleScriptRunner2, JSRunner2, etc.). It eliminates code duplication and ensures consistent behavior across all runners.
Ideally yes, in reallity we needed to fork runner into lua runner.

## Features

### Common Functionality Provided

1. **Menu Bar System**
   - Application menu with About, Hide, Show All, and Quit
   - File menu with Close Window
   - Edit menu with Cut, Copy, Paste, Select All
   - Window menu with Minimize, Zoom, Bring All to Front
   - Standard keyboard shortcuts:
     - `Cmd+Q` - Quit application
     - `Cmd+W` - Close window
     - `Cmd+H` - Hide window
     - `Cmd+M` - Minimize window
     - etc.

2. **Framework Initialization**
   - DisplayManager setup
   - TextGrid, GraphicsLayer, SpriteManager creation
   - Metal renderer initialization
   - Font loading (Menlo → Monaco → VGA fallback)
   - AudioManager and InputManager setup
   - API context registration

3. **Render Loop Management**
   - CVDisplayLink for vsync
   - 60 FPS frame timing
   - Automatic frame counting
   - Time delta calculation
   - Input polling

4. **Thread Management**
   - Background script thread creation
   - Proper thread synchronization
   - Clean shutdown handling

5. **Window Management**
   - Window creation and configuration
   - Automatic centering
   - Close behavior

6. **Error Handling**
   - Alert dialogs for errors
   - Console logging
   - Graceful failure handling

## Architecture

### Class Hierarchy

```
NSObject
  └── BaseRunner (abstract)
        ├── BasicRunner2App
        ├── LuaRunner2App
        ├── AppleScriptRunner2App
        └── JSRunner2App
```

### Abstract Methods (Must Override)

Subclasses MUST implement these three methods:

#### `- (BOOL)initializeRuntime`
Initialize the language-specific runtime (Lua state, BASIC interpreter, etc.)

**Example (Lua):**
```objc
- (BOOL)initializeRuntime {
    _luaState = luaL_newstate();
    luaL_openlibs(_luaState);
    LuaBindings::registerBindings(_luaState);
    return YES;
}
```

#### `- (BOOL)loadAndExecuteScript`
Load and execute the script file. This runs on a background thread.

**Example (Lua):**
```objc
- (BOOL)loadAndExecuteScript {
    if (luaL_loadfile(_luaState, self.scriptPath.c_str()) != LUA_OK) {
        return NO;
    }
    return lua_pcall(_luaState, 0, 0, 0) == LUA_OK;
}
```

#### `- (void)cleanupRuntime`
Clean up runtime resources (close Lua state, free BASIC memory, etc.)

**Example (Lua):**
```objc
- (void)cleanupRuntime {
    if (_luaState) {
        lua_close(_luaState);
        _luaState = nullptr;
    }
}
```

### Protected Properties (Available to Subclasses)

```objc
@property (strong) NSWindow* window;
@property (assign) std::string scriptPath;
@property (assign) std::string runnerName;

// Framework components
@property (readonly) std::shared_ptr<DisplayManager> displayManager;
@property (readonly) std::shared_ptr<TextGrid> textGrid;
@property (readonly) std::shared_ptr<GraphicsLayer> graphicsLayer;
@property (readonly) std::shared_ptr<SpriteManager> spriteManager;
@property (readonly) std::shared_ptr<MetalRenderer> renderer;
@property (readonly) std::shared_ptr<FontAtlas> fontAtlas;
@property (readonly) std::shared_ptr<InputManager> inputManager;
@property (readonly) std::shared_ptr<AudioManager> audioManager;

// Runtime state
@property (readonly) std::atomic<bool>& running;
@property (readonly) uint64_t frameCount;
@property (readonly) double lastFrameTime;
```

### Optional Overrides

Subclasses CAN override these methods if needed:

#### `- (BOOL)initializeFramework`
Override to use default config (800x600, 80x25 text grid)

#### `- (BOOL)initializeFrameworkWithConfig:(DisplayConfig)config`
Override to use custom display configuration

#### `- (void)onFrameTick`
Override to add custom per-frame logic (but call `[super onFrameTick]`)

## Usage Example

### Creating a New Runner

```objc
// MyNewRunner.mm
#import "BaseRunner.h"
#include "MyLanguageRuntime.h"

@interface MyNewRunner : BaseRunner
@end

@implementation MyNewRunner {
    MyRuntimeState* _runtime;
}

- (BOOL)initializeRuntime {
    _runtime = myruntime_create();
    return _runtime != nullptr;
}

- (BOOL)loadAndExecuteScript {
    return myruntime_execute(_runtime, self.scriptPath.c_str());
}

- (void)cleanupRuntime {
    if (_runtime) {
        myruntime_destroy(_runtime);
        _runtime = nullptr;
    }
}

@end

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        if (argc < 2) {
            std::cerr << "Usage: MyNewRunner <script.ext>\n";
            return 1;
        }

        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        MyNewRunner* delegate = [[MyNewRunner alloc] 
            initWithScriptPath:std::string(argv[1])
                    runnerName:"MyNewRunner - SuperTerminal"];

        [app setDelegate:delegate];
        [app run];

        return 0;
    }
}
```

### Custom Display Configuration

```objc
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    // Setup menu bar first
    [self setupMenuBar];

    // Custom display config
    DisplayConfig config;
    config.windowWidth = 1024;
    config.windowHeight = 768;
    config.cellWidth = 8;
    config.cellHeight = 12;
    config.fullscreen = false;
    config.vsync = true;
    config.targetFPS = 60.0f;
    config.windowTitle = "My Custom Runner";

    if (![self initializeFrameworkWithConfig:config]) {
        [self showError:@"Failed to initialize framework"];
        [NSApp terminate:nil];
        return;
    }

    // Continue with runtime initialization...
    [super applicationDidFinishLaunching:notification];
}
```

## Benefits

### Before BaseRunner (Each runner had ~400-900 lines)
- Menu bar code duplicated across 4+ files
- Framework init code duplicated
- Render loop code duplicated
- Shutdown logic duplicated
- No consistent quit mechanism
- Hard to add features consistently

### After BaseRunner (Each runner has ~150-200 lines)
- ✅ Menu bar provided automatically
- ✅ Framework init handled by base class
- ✅ Render loop managed by base class
- ✅ Clean shutdown guaranteed
- ✅ Cmd+Q works everywhere
- ✅ New features added once, work everywhere
- ✅ Subclasses focus only on language-specific logic

## Code Reduction

| Runner | Before | After | Reduction |
|--------|--------|-------|-----------|
| LuaRunner2 | ~436 lines | ~187 lines | 57% |
| BasicRunner2 | ~955 lines | ~250 lines | 74% |
| AppleScriptRunner2 | ~585 lines | ~220 lines | 62% |
| JSRunner2 | ~450 lines | ~200 lines | 56% |

**Total: ~1000+ lines of duplicate code eliminated**

## Future Enhancements

Potential features to add to BaseRunner:

- [ ] Debug menu with frame rate display, logging controls
- [ ] View menu with fullscreen toggle, resolution switching
- [ ] Preferences window for common settings
- [ ] Script hot-reloading
- [ ] Screenshot/recording functionality
- [ ] Performance monitoring overlay
- [ ] Crash reporting
- [ ] Multiple window support
- [ ] Script debugging interface

## Testing

When modifying BaseRunner:

1. Test ALL runners to ensure they still work
2. Verify menu bar appears correctly
3. Test Cmd+Q quits properly
4. Test window close behavior
5. Test error dialogs appear
6. Verify frame timing is correct
7. Check for memory leaks on shutdown

## Migration Guide

To migrate an existing runner to use BaseRunner:

1. Include `BaseRunner.h` instead of individual framework headers
2. Change class declaration: `@interface YourRunner : BaseRunner`
3. Remove all framework initialization code
4. Remove menu bar setup code
5. Remove render loop code
6. Extract runtime-specific code into three methods:
   - `initializeRuntime`
   - `loadAndExecuteScript`
   - `cleanupRuntime`
7. Update `main()` to use `initWithScriptPath:runnerName:`
8. Remove instance variables that are now in BaseRunner
9. Access framework components via properties (e.g., `self.textGrid`)
10. Test thoroughly!

See `v2SuperTerminal/LuaRunner2/main_refactored.mm` for a complete example.

## License

Part of the SuperTerminal Framework.
