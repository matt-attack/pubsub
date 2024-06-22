# A log message. Useful for viewing logs remotely.

# Log level
DEBUG=0
INFO=1
WARN=2
ERROR=3
enum uint8 level

# The message you want to log
string text

# The filename of the source file this log occurred in
string file
# The linenumber of the source file this log occurred in
uint32 line
