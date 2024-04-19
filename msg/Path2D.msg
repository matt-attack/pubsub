
# Frame the path is in, either wgs84 or odometry
FRAME_WGS84=0
FRAME_ODOM=1
enum uint8 frame

struct Point
double x
double y
end_struct

# Points of the path
Point points[]
