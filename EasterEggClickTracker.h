#pragma once

#include <windows.h>

class EasterEggClickTracker
{
public:
	bool RegisterClick(ULONGLONG clickTick);

private:
	static constexpr int kRequiredClicks = 10;
	static constexpr ULONGLONG kMaxIntervalMs = 350;

	int m_clickCount = 0;
	ULONGLONG m_lastClickTick = 0;
};
