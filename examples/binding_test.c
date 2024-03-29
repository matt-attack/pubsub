//#include <cstdlib>
#include <stdio.h>

#include <pubsub/Bindings.h>
#include <pubsub/System.h>
#include <stdint.h>

#ifdef _WIN32
#include <Windows.h>
#endif

struct message
{
    int64_t t;
    double h;
};

int main()
{
	int node = ps_create_node("/test", "", false);
	int pub = ps_create_publisher(node, "/data", "test_msg;int64 t;float64 hi", true);
	int i = 0;
	while (ps_is_okay())
	{
		struct message msg;
		msg.t = 4;
		msg.h = 20;
		ps_publish(pub, &msg, 16);
		ps_spin_node(node);
#ifdef _WIN32
		Sleep(100);
#else
		ps_sleep(100);
#endif
	}

	ps_destroy_node(node);

    return 0;
}

