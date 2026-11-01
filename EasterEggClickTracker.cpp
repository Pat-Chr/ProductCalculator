#include "framework.h"
#include "EasterEggClickTracker.h"

bool EasterEggClickTracker::RegisterClick(ULONGLONG clickTick)
{
	if (m_clickCount <= 0)
	{
		m_clickCount = 1;
		m_lastClickTick = clickTick;
		return false;
	}

	if (clickTick - m_lastClickTick <= kMaxIntervalMs)
	{
		++m_clickCount;
	}
	else
	{
		m_clickCount = 1;
	}

	m_lastClickTick = clickTick;

	if (m_clickCount >= kRequiredClicks)
	{
		m_clickCount = 0;
		m_lastClickTick = 0;
		return true;
	}

	return false;
}
