Header header

struct Field
TYPE_FLOAT=0
enum uint8 type
string:7 name
end_struct

# description of the data in the message
Field fields[]

uint32 num_points

# Pointcloud data itself in an interleaved array
uint8 data[]
