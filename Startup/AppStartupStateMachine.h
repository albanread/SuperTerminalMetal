//
// AppStartupStateMachine.h
// SuperTerminal v2
//
// Manages thread-safe application startup sequence
//

#ifndef SUPERTERMINAL_APP_STARTUP_STATE_MACHINE_H
#define SUPERTERMINAL_APP_STARTUP_STATE_MACHINE_H

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>

namespace SuperTerminal {

/// Application startup states
enum class StartupState {
    UNINITIALIZED,
    CREATING_WINDOW,
    WINDOW_CREATED,
    INITIALIZING_METAL,
    INITIALIZING_COMPONENTS,
    LOADING_CONTENT,
    RENDERING_INITIAL,
    READY_TO_SHOW,
    SHOWING_WINDOW,
    STARTING_RENDER_LOOP,
    RUNNING,
    ERROR
};

/// Thread-safe startup state machine
class AppStartupStateMachine {
public:
    AppStartupStateMachine();
    ~AppStartupStateMachine() = default;

    bool transitionTo(StartupState newState, const char* reason = nullptr);
    StartupState getCurrentState() const;
    bool isInState(StartupState state) const;
    bool isRunning() const;
    bool hasError() const;
    std::string getErrorMessage() const;
    bool waitForState(StartupState state, int timeoutMs = 5000);
    static const char* getStateName(StartupState state);
    bool canRenderThreadRun() const;
    bool canProcessMouseEvents() const;

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<StartupState> m_state;
    std::string m_errorMessage;
    bool isValidTransition(StartupState from, StartupState to) const;
};

} // namespace SuperTerminal

#endif
