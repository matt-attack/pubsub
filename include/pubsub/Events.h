#pragma once

//#include <Windows.h>
//#include <WinSock2.h>
//#include <WS2tcpip.h>

struct ps_event_t
{
#ifdef WIN32
	HANDLE handle;
#endif
};

struct ps_event_t ps_event_create()
{
	struct ps_event_t ev;
	ev.handle = WSACreateEvent();
	return ev;
}

void ps_event_trigger(struct ps_event_t* ev)
{
	WSASetEvent(ev->handle);
}

void ps_event_reset(struct ps_event_t* ev)
{
	WSAResetEvent(ev->handle);
}

void ps_event_destroy(struct ps_event_t* ev)
{
	WSACloseEvent(ev->handle);
	ev->handle = 0;
}

void ps_event_wait_multiple(struct ps_event_t* events, unsigned int num_events, unsigned int timeout_ms)
{
	DWORD event = WSAWaitForMultipleEvents(num_events, (HANDLE*)events, false, timeout_ms, false);
	if (event == WSA_WAIT_TIMEOUT)
		return;

	// Reset the signaled event
	int id = event - WSA_WAIT_EVENT_0;
	bool bResult = WSAResetEvent(events[id].handle);
	if (bResult == false) {
		wprintf(L"WSAResetEvent failed with error = %d\n", WSAGetLastError());
	}
}