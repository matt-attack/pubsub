
uint16 num_points

# Point type, right now limited to a set list
# 0 = x,y,z
# 1 = x,y,z,intensity
uint8 point_type

# Pointcloud data itself in an interleaved array
uint8 data[]
