﻿# Frame this costmap is in
# 0 = odom (x, y)
# 1 = wgs84 (lon, lat)
FRAME_ODOM=0
FRAME_WGS84=1
enum uint8 frame

# Dimensions in pixels
uint32 width
uint32 height

# Side dimension of each pixel in world units
float resolution

# x or lon
double left
# y or lat
double bottom

# Underlying costmap data. Values are application specific.
uint8 data[]
