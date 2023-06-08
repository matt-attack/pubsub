# Data from a joystick

# Bitmask of up to 32 buttons
# 1 = pressed
# 0 = released
uint32 buttons

# Joystick axes from -1 to 1
# Enough for 4 sticks, or 3 sticks and two triggers
float axes[8]
