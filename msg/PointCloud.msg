
uint32 num_points

# Point type, right now limited to a set list
POINT_XYZ=0   # x, y, z
POINT_XYZI=1  # x, y, z, intensity
POINT_XYZIO=2 # x, y, z, intensity, other
enum uint8 point_type

# Pointcloud data itself in an interleaved array
uint8 data[]
