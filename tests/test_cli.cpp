

#define ps_okay fake_ps_okay
#define main main_old
#include "../tools/pubsub.cpp"
#undef main
#undef ps_okay
#include "mini_mock.hpp"

static bool run = true;
int fake_ps_okay()
{
	return run;
}

extern "C"
{
int ps_okay();
}
#include <sstream>
#include <fstream>

#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Spinners.h>

#include <pubsub/String.msg.h>

using namespace std;

std::string slurp(std::ifstream& in) {
    std::ostringstream sstr;
    sstr << in.rdbuf();
    return sstr.str();
}

// Executes a CLI command and returns the output
std::string execute_command(const std::vector<std::string>& args)
{
	// capture std out
	const char* tmp_file = "tmp_output.out";
	int bak, nnew;
	fflush(stdout);
	bak = dup(1);
	nnew = open(tmp_file, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
	dup2(nnew, 1);
	close(nnew);

	std::vector<const char*> cargs;
	cargs.push_back("filename.o");
	for (const auto& a: args)
	{
		cargs.push_back(a.c_str());
	}
	main_old(cargs.size(), (char**)cargs.data());

	// Restore stdout
	fflush(stdout);
	dup2(bak, STDOUT_FILENO);
	close(nnew);

	// Read in captured std out output
	auto file = std::ifstream("tmp_output.out");
	std::string output = slurp(file);
	printf("%s\n", output.c_str());
	file.close();
	unlink(tmp_file);
	return output;
}

TEST(test_cli_help, []() {
	auto output = execute_command({"--help"});

	// Check result
	EXPECT(output.find("info (node name)") != std::string::npos);
});

TEST(test_cli_pub_latched, []() {
	run = true;
	std::thread thread([](){
		auto output = execute_command({"topic", "pub", "/data", "'hello'", "-l", "-r", "0"});
	});

	pubsub::Node node("simple_sub");
	
	pubsub::BlockingSpinnerWithTimers spinner;
	spinner.setNode(node);

	bool got_message = false;
	pubsub::Subscriber<pubsub::msg::String> subscriber(node, "/data", [&](const pubsub::msg::StringSharedPtr& msg) {
		printf("Got message %s in sub1\n", msg->value);
		EXPECT(strcmp("hello", msg->value) == 0);
		got_message = true;
		spinner.stop();
	}, 10);

	spinner.wait();
	EXPECT(got_message);
	run = false;

	thread.join();
});

TEST(test_cli_pub, []() {
	run = true;
	std::thread thread([](){
		auto output = execute_command({"topic", "pub", "/data", "'hello'", "-r", "20.0"});
	});

	pubsub::Node node("simple_sub");
	
	pubsub::BlockingSpinnerWithTimers spinner;
	spinner.setNode(node);

	bool got_message = false;
	pubsub::Subscriber<pubsub::msg::String> subscriber(node, "/data", [&](const pubsub::msg::StringSharedPtr& msg) {
		printf("Got message %s in sub1\n", msg->value);
		EXPECT(strcmp("hello", msg->value) == 0);
		got_message = true;
		spinner.stop();
	}, 10);

	spinner.wait();
	EXPECT(got_message);
	run = false;

	thread.join();
});

CREATE_MAIN_ENTRY_POINT();
