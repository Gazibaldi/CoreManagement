#include "TransientActivityMonitor.h"

bool TransientActivityMonitor::ActivateMonitor() noexcept {
    m_IsActive = m_StateEvaluator();

	return m_IsActive;
}

// Core Tick Cycle Engine
void TransientActivityMonitor::MonitorCheck() noexcept {
    
	if (ActivateMonitor()) [[unlikely]]
		return;

    m_TickCounter++;

    if (m_TickCounter >= m_MaxTicksThreshold) {
        m_IsExpired = true;
        m_IsActive = false;
        
        return;
    }
    
    m_IsActive = true;
}

void TransientActivityMonitor::Reset() noexcept {
	m_IsActive = false;
	m_TickCounter = 0;
	m_IsExpired = false;
}