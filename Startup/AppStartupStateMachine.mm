//
// AppStartupStateMachine.cpp
// SuperTerminal v2
//
// Manages thread-safe application startup sequence
//

#include "AppStartupStateMachine.h"
#include <chrono>

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#define LOG_STATE(msg, ...) NSLog(@"[StartupState] " msg, ##__VA_ARGS__)
#else
#include <cstdio>
#define LOG_STATE(msg, ...) printf("[StartupState] " msg "\n", ##__VA_ARGS__)
#endif

namespace SuperTerminal {

AppStartupStateMachine::AppStartupStateMachine()
    : m_state(StartupState::UNINITIALIZED)
{
    LOG_STATE("State machine created");
}

bool AppStartupStateMachine::transitionTo(StartupState newState, const char* reason) {
    std::lock_guard<std::mutex> lock(m_mutex);

    StartupState currentState = m_state.load();

    // Validate transition
    if (!isValidTransition(currentState, newState)) {
        LOG_STATE("INVALID transition: %s -> %s",
                  getStateName(currentState), getStateName(newState));
        return false;
    }

    // Perform transition
    m_state.store(newState);

    if (reason) {
        LOG_STATE("%s -> %s: %s",
                  getStateName(currentState), getStateName(newState), reason);
    } else {
        LOG_STATE("%s -> %s",
                  getStateName(currentState), getStateName(newState));
    }

    // Notify waiters
    m_cv.notify_all();

    return true;
}

StartupState AppStartupStateMachine::getCurrentState() const {
    return m_state.load();
}

bool AppStartupStateMachine::isInState(StartupState state) const {
    return m_state.load() == state;
}

bool AppStartupStateMachine::isRunning() const {
    return m_state.load() == StartupState::RUNNING;
}

bool AppStartupStateMachine::hasError() const {
    return m_state.load() == StartupState::ERROR;
}

std::string AppStartupStateMachine::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_errorMessage;
}

bool AppStartupStateMachine::waitForState(StartupState state, int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);

    auto timeout = std::chrono::milliseconds(timeoutMs);

    return m_cv.wait_for(lock, timeout, [this, state]() {
        StartupState current = m_state.load();
        // Also return if we hit ERROR or RUNNING (terminal states)
        return current == state || current == StartupState::ERROR || current == StartupState::RUNNING;
    });
}

const char* AppStartupStateMachine::getStateName(StartupState state) {
    switch (state) {
        case StartupState::UNINITIALIZED:           return "UNINITIALIZED";
        case StartupState::CREATING_WINDOW:         return "CREATING_WINDOW";
        case StartupState::WINDOW_CREATED:          return "WINDOW_CREATED";
        case StartupState::INITIALIZING_METAL:      return "INITIALIZING_METAL";
        case StartupState::INITIALIZING_COMPONENTS: return "INITIALIZING_COMPONENTS";
        case StartupState::LOADING_CONTENT:         return "LOADING_CONTENT";
        case StartupState::RENDERING_INITIAL:       return "RENDERING_INITIAL";
        case StartupState::READY_TO_SHOW:           return "READY_TO_SHOW";
        case StartupState::SHOWING_WINDOW:          return "SHOWING_WINDOW";
        case StartupState::STARTING_RENDER_LOOP:    return "STARTING_RENDER_LOOP";
        case StartupState::RUNNING:                 return "RUNNING";
        case StartupState::ERROR:                   return "ERROR";
        default:                                    return "UNKNOWN";
    }
}

bool AppStartupStateMachine::canRenderThreadRun() const {
    StartupState state = m_state.load();
    // Render thread can only run after we've reached RUNNING state
    return state == StartupState::RUNNING;
}

bool AppStartupStateMachine::canProcessMouseEvents() const {
    StartupState state = m_state.load();
    // Mouse events can only be processed after window is shown and render loop started
    return state == StartupState::RUNNING;
}

bool AppStartupStateMachine::isValidTransition(StartupState from, StartupState to) const {
    // Can always transition to ERROR
    if (to == StartupState::ERROR) {
        return true;
    }

    // Define valid state transitions (strict ordering)
    switch (from) {
        case StartupState::UNINITIALIZED:
            return to == StartupState::CREATING_WINDOW;

        case StartupState::CREATING_WINDOW:
            return to == StartupState::WINDOW_CREATED;

        case StartupState::WINDOW_CREATED:
            return to == StartupState::INITIALIZING_METAL;

        case StartupState::INITIALIZING_METAL:
            return to == StartupState::INITIALIZING_COMPONENTS;

        case StartupState::INITIALIZING_COMPONENTS:
            return to == StartupState::LOADING_CONTENT;

        case StartupState::LOADING_CONTENT:
            return to == StartupState::RENDERING_INITIAL;

        case StartupState::RENDERING_INITIAL:
            return to == StartupState::READY_TO_SHOW;

        case StartupState::READY_TO_SHOW:
            return to == StartupState::SHOWING_WINDOW;

        case StartupState::SHOWING_WINDOW:
            return to == StartupState::STARTING_RENDER_LOOP;

        case StartupState::STARTING_RENDER_LOOP:
            return to == StartupState::RUNNING;

        case StartupState::RUNNING:
            // Once running, can only go to ERROR
            return false;

        case StartupState::ERROR:
            // ERROR is terminal
            return false;

        default:
            return false;
    }
}

} // namespace SuperTerminal
