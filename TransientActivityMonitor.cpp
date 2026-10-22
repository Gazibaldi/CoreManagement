#include "TransientActivityMonitor.h"

bool TransientActivityMonitor::ActivateMonitor() noexcept {
    m_IsActive = m_StateEvaluator();

	return m_IsActive;
}

// Core Tick Cycle Engine
void TransientActivityMonitor::MonitorCheck() noexcept {
    
	// If the state evaluator returns false and the monitor is active, increment the tick counter
	// If the tick counter exceeds the threshold, mark the monitor as expired and inactive
	// this is basically a fuzzy debounce mechanism to ensure that transient states are not immediately considered expired
    if (!m_StateEvaluator() && m_IsActive) {
        m_TickCounter++;

        if (m_TickCounter >= m_MaxTicksThreshold) {
            m_IsExpired = true;
            m_IsActive = false;

            return;
        }

        m_IsActive = true;
    }
}

void TransientActivityMonitor::Reset() noexcept {
	m_IsActive = false;
	m_TickCounter = 0;
	m_IsExpired = false;
}

void TransientActivityMonitor::RefreshThreshold() noexcept {
    // Recalculate the threshold based on the current global configuration
    m_MaxTicksThreshold = (1000 / g_StateChangeTickIntervalMs.load(std::memory_order_acquire)) * m_ThresholdSeconds;
}