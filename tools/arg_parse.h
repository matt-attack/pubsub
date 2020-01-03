
#pragma once

#include <map>
#include <vector>
#include <string>

namespace pubsub
{

struct Argument
{
	std::string value;
	std::string default_value;
	std::string description;
	std::vector<std::string> names;
	bool present;
};

class ArgParser
{
	std::vector<Argument*> arg_list_;
	std::map<std::string, Argument*> args_;
	std::vector<std::string> positional_;
public:

	ArgParser()
	{
		std::vector<std::string> names = { "h", "help" };
		AddMulti(names, "Display this message.");
	}

	void AddMulti(std::vector<std::string> names, const std::string& description, const std::string& default_value = "")
	{
		Argument* a = new Argument;
		a->default_value = default_value;
		a->description = description;
		a->names = names;
		a->present = false;
		arg_list_.push_back(a);

		for (auto& name : names)
		{
			// todo check for duplicates
			args_[name] = a;
		}
	}

	void Add(const std::string& name, const std::string& description, const std::string& default_value = "")
	{
		Argument* a = new Argument;
		a->description = description;
		a->default_value = default_value;
		a->present = false;
		a->names.push_back(name);
		args_[name] = a;
		arg_list_.push_back(a);
	}

	void Parse(char** args, int argc, int skip = 0)
	{
		for (int i = 1 + skip; i < argc; i++)
		{
			// parse it yo!
			std::string s = args[i];
			if (s.length() > 2 && s[0] == '-' && s[1] == '-')
			{
				s = s.substr(2);

				if (s.length() == 1)
				{
					printf("Invalid argument %s", s.c_str());
					continue;
				}

				std::string value;
				if (i + 1 < argc && args[i + 1][0] != '-')
				{
					value = args[i + 1];
					i++;
				}

				if (args_.find(s) == args_.end())
				{
					printf("Warning: Unused argument %s", s.c_str());
					continue;
				}

				args_[s]->value = value;
				args_[s]->present = true;
			}
			else if (s.length() > 1 && s[0] == '-')
			{
				s = s.substr(1);

				std::string value;
				if (s.length() > 1)
				{
					// grab the rest of the argument as the value
					value = s.substr(1);
					s = s.substr(0, 1);
				}
				else if (i + 1 < argc && args[i + 1][0] != '-')
				{
					value = args[i + 1];
					i++;
				}

				if (args_.find(s) == args_.end())
				{
					printf("Warning: Unused argument %s", s.c_str());
					continue;
				}

				args_[s]->value = value;
				args_[s]->present = true;
			}
			else
			{
				positional_.push_back(s);
			}

			if (s == "h" || s == "help")
			{
				PrintHelp();
				exit(0);
				return;
			}
		}

		return;
	}

	void PrintHelp()
	{
		for (auto& ii : arg_list_)
		{
			std::string line = ii->names.front().length() > 1 ? "--" : "-";
			line += ii->names.front();
			int spaces = 13 - line.length();
			for (int i = 0; i < spaces; i++)
			{
				line += ' ';
			}
			line += ii->description;
			if (ii->default_value.length())
				line += "  [" + ii->default_value + "]";
			line += "\n";
			for (size_t i = 1; i < ii->names.size(); i++)
			{
				if (ii->names[i].length() == 1)
					line += "  -" + ii->names[i] + "\n";
				else
					line += " --" + ii->names[i] + "\n";
			}
			printf("%s", line.c_str());
		}
	}

	std::string GetPositional(unsigned int i)
	{
		if (i >= positional_.size())
		{
			return "";
		}
		return positional_[i];
	}

	bool GetBool(const std::string& arg)
	{
		auto& ar = args_[arg];
		return (ar->present && ar->value.length() == 0) 
			|| (ar->present && (ar->value == "true" || ar->value == "1")) 
			|| (ar->present == false && ar->default_value == "true");
	}

	double GetDouble(const std::string& arg)
	{
		auto& ar = args_[arg];
		if (ar->present == false)
		{
			return std::atof(ar->default_value.c_str());
		}
		return std::atof(ar->value.c_str());
	}
};

}