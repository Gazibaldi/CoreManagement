/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <functional>
#include <concepts>
#include <utility>

enum class TransientActivityType {
	Unknown,
    Bathing,
    Jail,
    Barber,
    Fishing
};

class TransientActivityMonitor {
    TransientActivityType m_Activity = TransientActivityType::Unknown;
    bool m_IsActive = false;
    bool m_IsExpired = false;

    int m_TickCounter = 0;
    int m_MaxTicksThreshold = 0;
	int m_ThresholdSeconds = 0;

    std::function<bool()> m_StateEvaluator;

public:
    TransientActivityMonitor() = default;

    template<typename F>
        requires std::invocable<F>&& std::convertible_to<std::invoke_result_t<F>, bool>
    TransientActivityMonitor(TransientActivityType activity, int thresholdSeconds, F&& evaluator)
        : m_Activity(activity)
        , m_StateEvaluator(std::forward<F>(evaluator))
    {
        // Internal tick math based on your global interval configuration
		m_ThresholdSeconds = thresholdSeconds;
		RefreshThreshold(false); // default to unrestricted threshold until the first tick cycle
    }

    bool ActivateMonitor() noexcept;
    void MonitorCheck() noexcept;
    void Reset() noexcept;
    void RefreshThreshold(bool isRestricted) noexcept;

    // Read-only Getters (Fully encapsulated)
    [[nodiscard]] constexpr bool IsActive() const noexcept { return m_IsActive; }
    [[nodiscard]] constexpr bool IsExpired() const noexcept { return m_IsExpired; }
    [[nodiscard]] constexpr TransientActivityType GetActivityType() const noexcept { return m_Activity; }
};