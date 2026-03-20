# A message that tests a wide variety of things
int32 test_int

string test_string

struct Struct
string:10 test_array_string
uint8 i1
int64 i2
float f3
double d4
double a5[3]

TYPE_INT=0
TYPE_DOUBLE=1
enum uint8 test_enum
end_struct

Struct test_struct
Struct test_structs[2]
Struct test_struct_array[]

string:20 test_array_string2

TYPE2_FLAG1=1
TYPE2_FLAG2=2
bitmask uint8 test_bitmask[3]
