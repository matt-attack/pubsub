//#include <cstdlib>
#include <stdio.h>

#include <pubsub/Bindings.h>
#include <stdint.h>

#include <Windows.h>

int main()
{
	int node = ps_create_node("/test", "", false);
	int pub = ps_create_publisher(node, "/data", "int32 t", true);
	int i = 0;
	while (ps_is_okay())
	{
		int32_t t = 4;
		ps_publish(pub, &t, 4);
		ps_spin_node(node);
		Sleep(100);
	}

	ps_destroy_node(node);

    return 0;
}

