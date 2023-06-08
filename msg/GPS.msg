# Represents a GPS measurement with a optional velocity track
# Dont publish if measurement is invalid/no fix present 

# Lat and Lon are invalid if both zero
double latitude
double longitude
double altitude

FIX_2D=0
FIX_3D=1
FIX_RTK_FLOAT=2
FIX_RTK_FIXED=3
enum uint8 fix_type

# Estimated accuracy of the location in meters, negative is invalid
float horizontal_accuracy
float vertical_accuracy

# Note this heading is meant for dopplar based heading, not dual antenna
# Heading of motion track in degrees (0-360), negative invalid
float heading
# Speed in m/s of motion track (unsigned), negative invalid
float speed
