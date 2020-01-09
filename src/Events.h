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

struct ps_event_t ps_create_event()
{
	struct ps_event_t ev;
	ev.handle = WSACreateEvent();
	return ev;
}

void ps_trigger_event(struct ps_event_t* ev)
{
	WSASetEvent(ev->handle);
}

void ps_reset_event(struct ps_event_t* ev)
{
	WSAResetEvent(ev->handle);
}

void ps_destroy_event(struct ps_event_t* ev)
{
	WSACloseEvent(ev->handle);
	ev->handle = 0;
}

void ps_event_wait_multiple(struct ps_event_t* events, unsigned int num_events, unsigned int timeout_ms)
{
	int event = WSAWaitForMultipleEvents(num_events, &events, false, timeout_ms, false);

	// Reset the signaled event
	bool bResult = WSAResetEvent(events[event - WSA_WAIT_EVENT_0].handle);
	if (bResult == false) {
		wprintf(L"WSAResetEvent failed with error = %d\n", WSAGetLastError());
	}
}