#include "core/StateMachine.h"
#include "core/EventBus.h"

namespace Recorder::Core {

    StateMachine::StateMachine() : m_currentState(EngineState::Idle) {}

    EngineState StateMachine::GetState() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentState;
    }

    bool StateMachine::CanTransitionTo(EngineState targetState) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentState == targetState) return false;

        switch (m_currentState) {
            case EngineState::Idle:
                return (targetState == EngineState::Starting || targetState == EngineState::Faulted);

            case EngineState::Starting:
                return (targetState == EngineState::Recording || targetState == EngineState::Faulted || targetState == EngineState::Idle);

            case EngineState::Recording:
                return (targetState == EngineState::Paused || targetState == EngineState::Stopping || targetState == EngineState::Faulted);

            case EngineState::Paused:
                return (targetState == EngineState::Recording || targetState == EngineState::Stopping || targetState == EngineState::Faulted);

            case EngineState::Stopping:
                return (targetState == EngineState::Remuxing || targetState == EngineState::Idle || targetState == EngineState::Faulted);

            case EngineState::Remuxing:
                return (targetState == EngineState::Idle || targetState == EngineState::Faulted);

            case EngineState::Faulted:
                return (targetState == EngineState::Idle);

            default:
                return false;
        }
    }

    bool StateMachine::TransitionTo(EngineState targetState) {
        EngineState previousState;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!CanTransitionTo(targetState)) {
                return false;
            }
            previousState = m_currentState;
            m_currentState = targetState;
        }

        // Notify subscribers via EventBus
        EventBus::Instance().Publish(StateChangedEvent{ previousState, targetState });
        return true;
    }

    void StateMachine::ResetToIdle() {
        EngineState previousState;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            previousState = m_currentState;
            m_currentState = EngineState::Idle;
        }
        if (previousState != EngineState::Idle) {
            EventBus::Instance().Publish(StateChangedEvent{ previousState, EngineState::Idle });
        }
    }

} // namespace Recorder::Core
