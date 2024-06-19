#include <pubsub/Events.h>

#include <stdbool.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#else
#include <WinSock2.h>
#endif

void ps_event_set_create(struct ps_event_set_t* set)
{
#ifdef WIN32
  set->num_handles = 1;
  set->handles = (HANDLE*)malloc(sizeof(HANDLE));
  set->sockets = (int*)malloc(sizeof(int));

  set->handles[0] = WSACreateEvent();
  set->sockets[0] = -1;
#else
  set->fd = epoll_create(1);
  set->num_events = 0;

  // create the pipe for manual triggers
  set->event_fd = eventfd(0, 0);
  set->timer_fd = 0;

  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.ptr = 0;
  epoll_ctl(set->fd, EPOLL_CTL_ADD, set->event_fd, &event);
#endif
}

void ps_event_set_destroy(struct ps_event_set_t* set)
{
#ifdef WIN32
  for (unsigned int i = 0; i < set->num_handles; i++)
  {
    WSACloseEvent(set->handles[i]);
  }
  free(set->handles);
  free(set->sockets);
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
  HANDLE* new_handles = (HANDLE*)malloc(sizeof(HANDLE)*(cur_size + 1));
  int* new_sockets = (int*)malloc(sizeof(int)*(cur_size + 1));
  for (int i = 0; i < cur_size; i++)
  {
    new_handles[i] = set->handles[i];
    new_sockets[i] = set->sockets[i];
  }
  free(set->handles);
  free(set->sockets);
  set->handles = new_handles;
  set->sockets = new_sockets;
  set->num_handles++;

  set->handles[cur_size + 0] = WSACreateEvent();
  set->sockets[cur_size + 0] = socket;

  WSAEventSelect(socket, set->handles[cur_size + 0], FD_READ);
#else
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.ptr = 0;
  epoll_ctl(set->fd, EPOLL_CTL_ADD, socket, &event);
  set->num_events++;
#endif
}

void ps_event_set_add_socket_write(struct ps_event_set_t* set, int socket)
{
#ifdef WIN32
  // find the handle and change the select
  for (int i = 0; i < set->num_handles; i++)
  {
    if (set->sockets[i] == socket)
    {
      WSAEventSelect(socket, set->handles[i], FD_READ | FD_WRITE);
      break;
    }
  }
#else
  struct epoll_event event;
  event.events = EPOLLIN | EPOLLOUT;
  event.data.ptr = 0;
  epoll_ctl(set->fd, EPOLL_CTL_MOD, socket, &event);
#endif
}

void ps_event_set_add_socket_write_only(struct ps_event_set_t* set, int socket)
{
#ifdef WIN32
#else
  struct epoll_event event;
  event.events = EPOLLOUT;
  event.data.ptr = 0;
  epoll_ctl(set->fd, EPOLL_CTL_MOD, socket, &event);
#endif
}

void ps_event_set_remove_socket_write(struct ps_event_set_t* set, int socket)
{
#ifdef WIN32
  // find the handle and change the select
  for (int i = 0; i < set->num_handles; i++)
  {
    if (set->sockets[i] == socket)
    {
      WSAEventSelect(socket, set->handles[i], FD_READ);
      break;
    }
  }
#else
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.ptr = 0;
  epoll_ctl(set->fd, EPOLL_CTL_MOD, socket, &event);
#endif
}


void ps_event_set_remove_socket(struct ps_event_set_t* set, int socket)
{
#ifdef WIN32
  // find the socket to remove then remove it
  bool found = false;
  int index = 0;
  for (; index < set->num_handles; index++)
  {
    if (set->sockets[index] == socket)
    {
      found = true;
      break;
    }
  }

  if (!found)
  {
    printf("ERROR: Could not find socket to remove!");
    return;
  }

  // remove the one at that index
  WSACloseEvent(set->handles[index]);

  int cur_size = set->num_handles;
  HANDLE* new_handles = (HANDLE*)malloc(sizeof(HANDLE)*(cur_size - 1));
  int* new_sockets = (int*)malloc(sizeof(int)*(cur_size - 1));

  // copy those before the spot
  for (int i = 0; i < index; i++)
  {
    new_sockets[i] = set->sockets[i];
    new_handles[i] = set->handles[i];
  }

  // copy those after the spot
  for (int i = index + 1; i < cur_size - 1; i++)
  {
    new_sockets[i - 1] = set->sockets[i];
    new_handles[i - 1] = set->handles[i];
  }
  set->num_handles--;
  free(set->handles);
  free(set->sockets);
  set->handles = new_handles;
  set->sockets = new_sockets;

#else
  struct epoll_event event;
  event.events = EPOLLIN;
  epoll_ctl(set->fd, EPOLL_CTL_DEL, socket, &event);
  set->num_events--;
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
  {
    return;
  }

  // Reset the signaled event
  int id = event - WSA_WAIT_EVENT_0;
  bool bResult = WSAResetEvent(set->handles[id]);
  if (bResult == false) {
    printf("WSAResetEvent failed with error = %d\n", WSAGetLastError());
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

void ps_event_set_set_timer(struct ps_event_set_t* set, unsigned int timeout_us)
{
#ifdef WIN32
  // todo
#else
  if (set->timer_fd == 0)
  {
    set->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC|TFD_NONBLOCK);
    struct epoll_event event;
    event.data.fd = set->timer_fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(set->fd, EPOLL_CTL_ADD, set->timer_fd, &event);
  }

  // set it!
  struct itimerspec its;
  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = timeout_us*1000;
  its.it_interval.tv_nsec = 0;
  its.it_interval.tv_nsec = 0;
  timerfd_settime(set->timer_fd, 0, &its, NULL);
#endif
}
