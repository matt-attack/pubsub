
#pragma once

#include <map>
#include <vector>
#include <string>

#include <stdexcept>

namespace pubsub
{

struct Argument
{
	std::string value;
	std::string default_value;
	std::string description;
	std::vector<std::string> names;
	bool present;
	bool flag;
};

class ArgParser
{
	std::vector<Argument*> arg_list_;
	std::map<std::string, Argument*> args_;
	std::vector<std::string> positional_;
	std::string usage_;

	// internal option add function
	void add(std::vector<std::string> names,
             const std::string& description,
             const std::string& default_value = "",
			 bool flag_ = false)
	{
		Argument* a = new Argument;
		a->default_value = default_value;
		a->description = description;
		a->names = names;
		a->present = false;
		a->flag = flag_;
		arg_list_.push_back(a);

		for (auto& name : names)
		{
			// todo check for duplicates
			if (args_.find(name) != args_.end())
			{
				throw std::runtime_error("Cannot have multiple arguments with same name '" + name + "'");
			}
			args_[name] = a;
		}
	}

public:

	ArgParser()
	{
		std::vector<std::string> names = { "h", "help" };
		AddFlag(names, "Display this message.");
	}

	~ArgParser()
	{
		for (auto& arg: arg_list_)
		{
			delete arg;
		}
	}

	void AddFlag(const char* name,
                 const std::string& description)
	{
		add({name}, description, "", true);
	}

	void AddFlag(std::vector<std::string> names,
                 const std::string& description)
	{
		add(names, description, "", true);
	}

	void AddOption(std::vector<std::string> names,
                  const std::string& description,
                  const std::string& default_value = "")
	{
		add(names, description, default_value);
	}

	void AddOption(const char* name,
             const std::string& description,
             const std::string& default_value = "")
	{
		add({name}, description, default_value);
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

                // -- is invalid with single character options
				if (s.length() == 1)
				{
					printf("invalid option '%s'\n", s.c_str());
					continue;
				}

				if (args_.find(s) == args_.end())
				{
					PrintInvalid(s);
					continue;
				}

				if (args_[s]->flag)
				{
					if (s == "h" || s == "help")
					{
						PrintHelp();
						exit(0);
						return;
					}
					args_[s]->value = "true";
					args_[s]->present = true;
					continue;
				}

				std::string value;
				if (i + 1 < argc && args[i + 1][0] != '-')
				{
					value = args[i + 1];
					i++;
				}

				args_[s]->value = value;
				args_[s]->present = true;
			}
			else if (s.length() > 1 && s[0] == '-')
			{
				s = s.substr(1);
				std::string option_name = s.substr(0, 1);

				if (args_.find(option_name) == args_.end())
				{
					PrintInvalid(option_name);
					continue;
				}

				if (args_[option_name]->flag)
				{
					if (s != option_name)
					{
						printf("Cannot provide arguments for flag option '%s'\n", option_name.c_str());
						exit(1);
					}

					if (option_name == "h" || option_name == "help")
					{
						PrintHelp();
						exit(0);
						return;
					}

					args_[option_name]->value = "true";
					args_[option_name]->present = true;
					continue;
				}

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
                    PrintInvalid(s);
					continue;
				}

				args_[s]->value = value;
				args_[s]->present = true;
			}
			else
			{
				positional_.push_back(s);
			}
		}

		return;
	}

    void SetUsage(const std::string& text)
    {
        usage_ = text;
    }

	void PrintHelp()
	{
        if (usage_.length())
        {
            printf("%s\n\n", usage_.c_str());
        }

		for (auto& ii : arg_list_)
		{
			std::string line = ii->names.front().length() > 1 ? " --" : " -";
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
					line += "   -" + ii->names[i] + "\n";
				else
					line += "  --" + ii->names[i] + "\n";
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

	const std::vector<std::string>& GetAllPositional()
	{
		return positional_;
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
	
	std::string GetString(const std::string& arg)
	{
		auto& ar = args_[arg];
		if (ar->present == false)
		{
			return ar->default_value;
		}
		return ar->value;
	}
private:

    void PrintInvalid(const std::string& option)
    {
       printf("unrecognized option '%s'\nTry --help for more information.\n", option.c_str());
       exit(-1);
    }
};

}
