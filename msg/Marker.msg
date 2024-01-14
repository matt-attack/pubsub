# Frame
# 0 = wgs84 (lon, lat, alt)
# 1 = odom (x,y,z)
FRAME_WGS84=0
FRAME_ODOM=1
enum uint8 frame

# Marker ID
uint16 id

# Type of marker
# line list 2d: 2 doubles per point x,y, and 2 points per line
# line segments: 1 double indicating number of points in the segment, then 2 doubles per point x,y making up rest of segment
# polygon 2d: 1 double indicating number of points, then 2 doubles per point x and y
# point list 3d: 4 doubles per point x,y,z and size in pixels
LINE_LIST_2D=0
LINE_SEGMENTS_2D=1
POLYGON_2D=2
POINT_LIST_3D=3
enum uint8 marker_type

# Data making up the marker
double data[]

# Color data per marker
uint32 colors[]
