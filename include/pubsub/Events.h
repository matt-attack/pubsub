#pragma once

//#include <Windows.h>
//#include <WinSock2.h>
//#include <WS2tcpip.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#endif

struct ps_event_set_t
{
#ifdef WIN32
    unsigned int num_handles;
    HANDLE* handles;
#else
    int fd;
    unsigned int num_events;
    int event_fd;
#endif
};

void ps_event_set_create(struct ps_event_set_t* set)
{
#ifdef WIN32
  set->num_handles = 1;
  set->handles = (HANDLE*)malloc(sizeof(HANDLE));

  set->handles[0] = WSACreateEvent();
#else
  set->fd = epoll_create(1);
  set->num_events = 0;

  // create the pipe for manual triggers
  set->event_fd = eventfd(0, 0);

  struct epoll_event event;
  event.events = EPOLLIN;
  epoll_ctl(set->fd, EPOLL_CTL_ADD, set->event_fd, &event);
#endif
}

void ps_event_set_destroy(struct ps_event_set_t* set)
{
#ifdef WIN32
  for (int i = 0; i < set->num_handles; i++)
  {
    WSACloseEvent(set->handles[i]);
  }
  free(set->handles);
#else
  set->num_events = 0;
  close(set->event_fd);
  close(set->fd);
#endif
}

void ps_event_set_add_socket(struct ps_event_set_t* set, int socket)
{
#ifdef WIN32
  // allocate a new spot
  int cur_size = set->num_handles;
  HANDLE* new_handles = (HANDLE*)malloc(sizeof(HANDLE)*(cur_size+1));
  for (int i = 0; i < cur_size; i++)
  {
    new_handles[i] = set->handles[i];
  }
  free(set->handles);
  set->handles = new_handles;
  set->num_handles++;
  
  set->handles[cur_size+0] = WSACreateEvent();

  WSAEventSelect(socket, set->handles[cur_size+0], FD_READ);
#else
  struct epoll_event event;
  event.events = EPOLLIN;
  epoll_ctl(set->fd, EPOLL_CTL_ADD, socket, &event);
  set->num_events++;
#endif
}

void ps_event_set_trigger(struct ps_event_set_t* set)
{
  if (!set)
  {
    return;
  }
#ifdef WIN32
  WSASetEvent(set->handles[0]);
#else
  uint64_t val = 1;
  write(set->event_fd, &val, 8);
#endif
}

unsigned int ps_event_set_count(const struct ps_event_set_t* set)
{
#ifdef WIN32
  return set->num_handles-1;
#else
  return set->num_events;
#endif
}

void ps_event_set_wait(struct ps_event_set_t* set, unsigned int timeout_ms)
{
#ifdef WIN32
  DWORD event = WSAWaitForMultipleEvents(set->num_handles, set->handles, false, timeout_ms, false);
  if (event == WSA_WAIT_TIMEOUT)
    return;

  // Reset the signaled event
  int id = event - WSA_WAIT_EVENT_0;
  bool bResult = WSAResetEvent(set->handles[id]);
  if (bResult == false) {
    wprintf(L"WSAResetEvent failed with error = %d\n", WSAGetLastError());
  }
#else
  //printf("waiting for %i events\n", set->num_events);
  struct epoll_event events[10];
  int num_ready = epoll_wait(set->fd, events, 10, timeout_ms);
  //printf("%i ready to read\n", num_ready);
  if (num_ready < 0)
  {
    perror("poll error");
  }
#endif
}

/*struct ps_event_t
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
}*/
