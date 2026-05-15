#pragma once

/// Get the value type from a pointer to data member
template<typename T>
struct member_pointer_value;
template<typename Class, typename Value>
struct member_pointer_value<Value Class::*>
{
    typedef Value type;
};

/// Get the class type from a pointer to data member
template<typename T>
struct member_pointer_class;
template<typename Class, typename Value>
struct member_pointer_class<Value Class::*>
{
    typedef Class type;
};
