#pragma once

#include <string>
#include <map>

// this parses a string and turns it into a map using yaml like parsing
enum ValueType
{
	None,
	String,
	Int,
	Bool,
	Float,
	Map
};

class Value
{
public:
	ValueType type;

	Value() : type(None) {}

	Value(const std::string& string) : type(String), str(string) { }

	Value(bool b) : type(Bool), boolean(b) { }


//private:
	double flt;//also holds ints
	std::string str;
	bool boolean;
	std::map<std::string, Value> map;
};

bool is_character(char c)
{
	return c != ' ' || c != '\t' || c != '\n';
}

// returns the number of characters grabbed
int parse(const std::string& input, Value& value)
{
	// strip leading whitespace
	int index = 0;
	for (index = 0; index < input.size(); index++)
	{
		if (is_character(input[index]))
		{
			break;
		}
	}
	std::string data = input.substr(index);
	if (data.length() == 0)
	{
		value = Value();
		return input.length();
	}

	int grabbed = input.length() - data.length();

	// okay, parse based on leading character
	if (data[0] == '{')
	{
		// its a struct
	}
	else if (data[0] >= '0' && data[0] <= '9' || data[0] == '-')
	{
		// its a number
		double num = atof(data.c_str());
		value = Value(num);
	}
	else if (data[0] == '"')
	{
		// its a string, grab characters until the end
		std::string str;
		bool found_close = false;
		for (int i = 1; i < data.length(); i++)
		{
			char c = data[i];
			if (c == '"')
			{
				found_close = true;
				break;
			}
			else if (c == '\\')
			{
				if (i - 1 < data.length())
				{
					str += data[i + 1];
				}
				else
				{
					//error
					throw std::string("String ended before finishing escape sequence.");
				}

				i += 2;

				continue;
			}
			str += data[i];
		}

		if (found_close == false)
		{
			// error!
			throw std::string("Missing closing \" on string.");
		}
		value = Value(str);
	}
	else if ((data[0] >= 'a' && data[0] <= 'z') || (data[0] >= 'A' && data[0] <= 'Z'))
	{
		// idk
		// todo parse until separator
		value = Value(data);
	}
}