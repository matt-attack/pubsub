#pragma once

#include <string>
#include <map>
#include <vector>

// this parses a string and turns it into a map using yaml like parsing
enum ValueType
{
	None,
	String,
	Number,
	Bool,
	Map,
	Array
};

class Value
{
public:
	ValueType type;

	Value() : type(None) {}

	Value(const std::string& string) : type(String), str(string) { }

	Value(bool b) : type(Bool), boolean(b) { }

	Value(double d) : type(Number), flt(d) { }


//private:
	double flt;//also holds ints
	std::string str;
	bool boolean;
	std::map<std::string, Value> map;
	std::vector<Value> arr;
};

bool is_character(char c)
{
	return c != ' ' && c != '\t' && c != '\n';
}


//okay, lets do this in two passes
// first tokenize, then parse recursively
enum class TokenTypes
{
	None,
	OpenBracket,
	CloseBracket,
	OpenBox,
	CloseBox,
	String,
	Number,
	Colon,
	Comma
};

class Token
{
public:
	TokenTypes type;
	std::string text;

	Token() : type(TokenTypes::None) {}

	Token(TokenTypes t) : type(t) {}

	Token(const std::string& str) : type(TokenTypes::String), text(str) {}

	Token(TokenTypes t, const std::string& str) : type(t), text(str) {}
};

void tokenize(const std::string& input, std::vector<Token>& tokens)
{
	unsigned int index = 0;
	while (index < input.length())
	{
		// strip leading whitespace
		if (!is_character(input[index]))
		{
			index++;
			continue;
		}

		// okay we got a character
		char c = input[index];
		if (c == '{')
		{
			tokens.push_back(Token(TokenTypes::OpenBracket));
			index++;
		}
		else if (c == '}')
		{
			tokens.push_back(Token(TokenTypes::CloseBracket));
			index++;
		}
		else if (c == '[')
		{
			tokens.push_back(Token(TokenTypes::OpenBox));
			index++;
		}
		else if (c == ']')
		{
			tokens.push_back(Token(TokenTypes::CloseBox));
			index++;
		}
		else if (c == ':')
		{
			tokens.push_back(Token(TokenTypes::Colon));
			index++;
		}
		else if (c == ',')
		{
			tokens.push_back(Token(TokenTypes::Comma));
			index++;
		}
		// number
		else if (c >= '0' && c <= '9' || c == '-')
		{
			std::string txt;
			txt += c;
			index++;
			for (; index < input.length(); index++)
			{
				char c2 = input[index];
				if ((c2 > '9' || c2 < '0') && c2 != '.')
				{
					break;
				}
				txt += input[index];
			}
			tokens.push_back(Token(TokenTypes::Number, txt));
		}
		else if (c == '"')
		{
			// quoted string
			std::string str;
			index++;
			// start grabbing until close quote
			for (; index < input.length(); index++)
			{
				if (input[index] == '"')
				{
					index++;
					break;
				}
				str += input[index];
			}
			tokens.push_back(Token(str));
		}
		else // grab one word
		{
			std::string str;
			// start grabbing until close quote
			for (; index < input.length(); index++)
			{
				if (input[index] == ':' || input[index] == '{' || input[index] == ',' || input[index] == '}')
				{
					break;
				}
				else if (input[index] == ' ' || input[index] == '\n' || input[index] == '\t')
				{
					index++;
					break;
				}
				str += input[index];
			}
			tokens.push_back(Token(str));
		}
	}
}

TokenTypes peek(const std::vector<Token>& tokens, unsigned int index)
{
	if (index >= tokens.size())
	{
		return TokenTypes::None;
	}
	return tokens[index].type;
}

void grab(const std::vector<Token>& tokens, unsigned int index, TokenTypes type)
{
	if (index >= tokens.size())
	{
		throw std::string("Out of tokens");
	}

	if (tokens[index].type != type)
	{
		throw std::string("Unexpected token");
	}
}

// now lets parse using these tokens
int parse(const std::vector<Token>& tokens, int start, Value& value)
{
	const Token& t = tokens[start];
	if (t.type == TokenTypes::OpenBracket)
	{
		//its an array/map parse it
		value.type = Map;

		if (peek(tokens, start + 1) == TokenTypes::CloseBracket)
		{
			return 2;
		}

		// start building it
		int count = 1;
		while (true)
		{
			if (peek(tokens, start + count + 1) != TokenTypes::Colon)
			{
				//grab it as a value at this index
				Value v;
				count += parse(tokens, start + count, v);
				std::string key = std::to_string(value.map.size());
				value.map[key] = v;
			}
			// it has a name
			else
			{
				Token name = tokens[start + count];
				count++;

				//grab next one, asserting its a colon
				grab(tokens, start + count, TokenTypes::Colon); count++;

				// then next comes the value
				Value v;
				count += parse(tokens, start + count, v);
				value.map[name.text] = v;
			}

			// if next is a commma
			if (peek(tokens, start + count) == TokenTypes::Comma)
			{
				// grab it and continue
				count++;
				continue;
			}
			else
			{
				break;// we are done
			}
		}

		// grab the close bracket
		grab(tokens, start + count, TokenTypes::CloseBracket); count++;
		return count;
	}
	else if (t.type == TokenTypes::OpenBox)
	{
		//its an array/map parse it
		value.type = Array;

		if (peek(tokens, start + 1) == TokenTypes::CloseBox)
		{
			return 2;
		}

		// start building it
		int count = 1;
		while (true)
		{
			//grab it as a value at this index
			Value v;
			count += parse(tokens, start + count, v);
			//std::string key = std::to_string(value.map.size());
			value.arr.push_back(v);

			// if next is a commma
			if (peek(tokens, start + count) == TokenTypes::Comma)
			{
				// grab it and continue
				count++;
				continue;
			}
			else
			{
				break;// we are done
			}
		}

		// grab the close bracket
		grab(tokens, start + count, TokenTypes::CloseBox); count++;
		return count;
	}
	else if (t.type == TokenTypes::String)
	{
		value = Value(t.text);
		return 1;
	}
	else if (t.type == TokenTypes::Number)
	{
		value = Value(std::atof(t.text.c_str()));
		return 1;
	}
	else
	{
		throw std::string("Unhandled token type");
	}
}

// returns the number of characters grabbed
int parse(const std::string& input, Value& value)
{
	std::vector<Token> tokens;
	tokenize(input, tokens);

	return parse(tokens, 0, value);
}