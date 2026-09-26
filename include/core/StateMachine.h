#pragma once

#include "Types.h"
#include <mutex>
#include <atomic>

namespace Recorder::Core {

    class StateMachine {
    public:
        StateMachine();
        ~StateMachine() = default;

        EngineState GetState() const;
        bool CanTransitionTo(EngineState targetState) const;
        bool TransitionTo(EngineState targetState);
        void ResetToIdle();

    private:
        mutable std::mutex m_mutex;
        EngineState m_currentState;
    };

} // namespace Recorder::Core
