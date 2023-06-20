# Represents a 2D image

# Image format
R8G8B8=0
R8=1
R16=2
R32=3
R8G8B8A8=4
enum uint8 type

# Image width in pixels
uint32 width
# Image height in pixels
uint32 height

# Image data, stored row by row
uint8 data[]