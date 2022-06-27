# Frame
# 0 = wgs84 (lon, lat, alt)
# 1 = odom (x,y,z?)
uint8 frame

# Marker ID
uint16 id

# Type of marker
# 0 = line list 2d (2 doubles per point, 2 points per line)
# 1 = line segments (1 double indicating number of points, then 2 doubles per point making up rest of segment)
# 2 = polygon 2d (1 double indicating number of points, then 2x that many positions making up rest)
uint8 marker_type

# Data making up the marker
double data[]

# Color data per marker
uint32 colors[]
