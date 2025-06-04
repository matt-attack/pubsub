#include <cstdlib>
#include <stdio.h>
#include <cmath>

#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Spinners.h>
#include <pubsub/System.h>

#include <pubsub/String.msg.h>
#include <pubsub/Image.msg.h>
#include <pubsub/Costmap.msg.h>
#include <pubsub/Marker.msg.h>
#include <pubsub/PointCloud.msg.h>
#include <pubsub/Pose.msg.h>
#include <pubsub/Parameters.msg.h>

#include <pubsub/TCPTransport.h>

int main()
{
	pubsub::Node node("simple_publisher");
	
	struct ps_transport_t tcp_transport;
    ps_tcp_transport_init(&tcp_transport, node.getNode());
    ps_node_add_transport(node.getNode(), &tcp_transport);

	pubsub::Publisher<pubsub::msg::String> string_pub(node, "/data");

    //string_pub.addCustomEndpoint(0x7FFFFFFF, 11312, 0);

	pubsub::Publisher<pubsub::msg::Image> image_pub(node, "/image");
	
	
	pubsub::Publisher<pubsub::msg::Image> image_pub2(node, "/image2");
	
	pubsub::Publisher<pubsub::msg::Costmap> costmap_pub(node, "/costmap");
	
	pubsub::Publisher<pubsub::msg::Marker> marker_pub(node, "/marker");
	
	pubsub::Publisher<pubsub::msg::PointCloud> pointcloud_pub(node, "/pointcloud");

    pubsub::Publisher<pubsub::msg::Pose> pose_pub(node, "/pose");

    pubsub::Publisher<pubsub::msg::Parameters> param_pub(node, "/parameters", true);

	pubsub::BlockingSpinnerWithTimers spinner;
	spinner.setNode(node);

		// make a parameters message
		pubsub::msg::Parameters p;
		p.name_length = 3;
		p.name = new char*[3];
		p.name[0] = "/test";
		p.name[1] = "/test1";
		p.name[2] = "/test3";
		p.type_length = 3;
		p.type = new uint8_t[3];
		p.type[0] = 0;
		p.type[1] = 1;
		p.type[2] = 2;
		p.value_length = 0;
		p.min_length = 0;
		p.max_length = 0;
		param_pub.publish(p);
		delete[] p.type;
		delete[] p.name;
		p.type_length = p.name_length = 0;
		p.name = 0;
		p.type = 0;

    int iteration = 0;
	int i = 0;
	spinner.addTimer(0.1, [&]()
	{
		pubsub::msg::String msg;
		char value[20];
		sprintf(value, "Hello %i", i++);
		msg.value = value;
		string_pub.publish(msg);

        ps_sleep(rand()%8);

        //printf("%i clients\n", string_pub.getNumSubscribers());

		// okay, since we are publishing with shared pointer we actually need to allocate the string properly
		/*auto shared = pubsub::msg::StringSharedPtr(new pubsub::msg::String);
		shared->value = new char[strlen(msg.value) + 1];
		strcpy(shared->value, msg.value);
		string_pub.publish(shared);*/

		msg.value = 0;// so it doesnt get freed by the destructor since we allocated it ourself

		// generate an image for testing
		auto img = pubsub::msg::ImageSharedPtr(new pubsub::msg::Image);
		img->width = 100;
		img->type = pubsub::msg::Image::R8G8B8;
		img->height = 100;
		img->data_length = 100*100*3;
		img->data = (uint8_t*)malloc(img->data_length);
		for (int y = 0; y < 100; y++)
		{
			for (int x = 0; x < 100; x++)
			{
				img->data[y*100*3 + x*3] = x*2;// r
				img->data[y*100*3 + x*3 + 1] = y*2;// g
				img->data[y*100*3 + x*3 + 2] = 0;// b	
			}
		}
		image_pub.publish(img);
		image_pub2.publish(img);
		
		auto map = pubsub::msg::CostmapSharedPtr(new pubsub::msg::Costmap);
		map->frame = 0;
		map->resolution = 0.5;
		map->left = 0;
		map->bottom = 0;
		map->width = 100;
		map->height = 100;
		map->data_length = map->width*map->height;
		map->data = (uint8_t*)malloc(map->data_length);
		for (int i = 0; i < map->data_length; i++)
		{
			map->data[i] = rand()%255;
		}
		costmap_pub.publish(map);
		
		auto marker = pubsub::msg::MarkerSharedPtr(new pubsub::msg::Marker);
		marker->frame = 1;
		marker->id = 0;
		marker->marker_type = 0;
		marker->data_length = 5*4;
		marker->data = (double*)malloc(sizeof(double)*marker->data_length);
		for (int i = 0; i < marker->data_length; i += 4)
		{
			marker->data[i] = i*3;
			marker->data[i+1] = i*3;
			marker->data[i+2] = (i+4)*3;
			marker->data[i+3] = (i+4)*3;
		}
		marker_pub.publish(marker);
		
		auto pcld = pubsub::msg::PointCloudSharedPtr(new pubsub::msg::PointCloud);
		pcld->point_type = pubsub::msg::PointCloud::POINT_XYZI;
		pcld->num_points = 10000;
		pcld->data_length = pcld->num_points*4*4;
		pcld->data = (uint8_t*)malloc(pcld->data_length);
		for (int i = 0; i < pcld->num_points; i++)
		{
			float* pt = (float*)&pcld->data[i*4*4];
			pt[0] = rand()%100 - 50;//x
			pt[1] = rand()%100 - 50;//y
			pt[2] = rand()%100 - 50;//z
			pt[3] = (rand()%30)/30.0;//i
		}
		pointcloud_pub.publish(pcld);
		pointcloud_pub.publish(pcld);
		pointcloud_pub.publish(pcld);
		
		marker = pubsub::msg::MarkerSharedPtr(new pubsub::msg::Marker);
		marker->frame = 1;
		marker->id = 1;
		marker->marker_type = 1;
		marker->data_length = 5*2 + 1;
		marker->data = (double*)malloc(sizeof(double)*marker->data_length);
		marker->data[0] = 5;
		for (int i = 1; i < marker->data_length; i += 2)
		{
			marker->data[i] = -i*3;
			marker->data[i+1] = -i*3;
		}
		marker_pub.publish(marker);

        auto pose = pubsub::msg::PoseSharedPtr(new pubsub::msg::Pose);
        pose->latitude = 0.0;
        pose->longitude = 0.0;
        pose->z = 10.0;
        pose->y = 100.0*sin(iteration*0.01);
        pose->x = 100.0*cos(iteration*0.01);
        pose->odom_yaw = iteration*0.01;
        pose->odom_pitch = 0;
        pose->odom_roll = 0;
        pose_pub.publish(pose);

        iteration++;
	});

	spinner.wait();

    return 0;
}

