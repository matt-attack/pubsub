
# Frame the path is in, either wgs84 or odometry
FRAME_WGS84=0
FRAME_ODOM=1
enum uint8 frame

# Type of path data
# Positions depend on the frame
#   Odom: Meters
#   WGS84: Degrees (X: Lon, Y: Lat)
# Angles are in radians
PATH_XY=0
PATH_XYZ=1
PATH_XY_Y=2
PATH_XYZ_Y=3
PATH_XYZ_YPR=4
enum uint8 path_type

# Points of the path
double points[]
