# Lists dynamic parameters, their values and min and max acceptable parameters
string name[]

string description[]

# The type of each parameter
DOUBLE=0
INT=1
enum uint8 type[]

# The current value for the parameter
string value[]

# Inclusive maximum valid value for the parameter
double min[]

# Inclusive minimum valid value for the parameter
# Not valid for max
double max[]
