#pragma once

struct ps_event_set_t
{
#ifdef WIN32
    unsigned int num_handles;
    void** handles;
    int* sockets;
#else
    int fd;
    unsigned int num_events;
    int event_fd;
#endif
};

void ps_event_set_create(struct ps_event_set_t* set);

void ps_event_set_destroy(struct ps_event_set_t* set);

void ps_event_set_add_socket(struct ps_event_set_t* set, int socket);

void ps_event_set_remove_socket(struct ps_event_set_t* set, int socket);

void ps_event_set_trigger(struct ps_event_set_t* set);

unsigned int ps_event_set_count(const struct ps_event_set_t* set);

void ps_event_set_wait(struct ps_event_set_t* set, unsigned int timeout_ms);
