
uint32 num_points

# Point type, right now limited to a set list
# 0 = x,y,z
# 1 = x,y,z,intensity
POINT_XYZ=0
POINT_XYZI=1
enum uint8 point_type

# Pointcloud data itself in an interleaved array
uint8 data[]
