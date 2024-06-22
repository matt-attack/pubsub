# Represents the pose of a robot in both world and local frames
# All fields must be valid besides lat, lon and altitude. Mark invalid with NaN

# Global pose
double latitude
double longitude
double altitude
float yaw
float pitch
float roll

# Odometry pose
double x
double y
double z
float odom_yaw
float odom_pitch
float odom_roll
