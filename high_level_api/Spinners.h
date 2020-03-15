#pragma once

#include <pubsub_cpp/Time.h>

#undef min
#undef max

namespace pubsub
{
class BlockingSpinner
{
	std::vector<Node*> nodes_;

	bool running_;

	std::thread thread_;
	std::mutex list_mutex_;

	ps_event_set_t events_;
public:

	// todo make it work with more than one thread
	BlockingSpinner(int num_threads = 1) : running_(false)
	{
      ps_event_set_create(&events_);
	}

	void start()
	{
		running_ = true;
		thread_ = std::thread([this]()
		{
			while (running_ && ps_okay())
			{
				list_mutex_.lock();
				if (ps_event_set_count(&events_) == 0)
				{
					ps_sleep(10);
				}
				else
				{
					ps_event_set_wait(&events_, 1000);
				}
				for (auto node : nodes_)
				{
					node->lock_.lock();
					ps_node_spin(node->getNode());

					// todo how to make this not scale with subscriber count...
					// though, it isnt very important here
					// its probably worth more effort just make it not run any of this if we get no messages
					// we got a message, now call a subscriber
					for (size_t i = 0; i < node->subscribers_.size(); i++)
					{
						// this doesnt ensure switching subs, but works
						while (node->subscribers_[i]->CallOne()) {};
					}
					node->lock_.unlock();
				}
				list_mutex_.unlock();
			}
		});
	}

	~BlockingSpinner()
	{
		if (running_)
		{
			stop();
		}

		ps_event_set_destroy(&events_);
	}

	void addNode(Node& node)
	{
		list_mutex_.lock();

		// build a wait list for all nodes
		ps_node_create_events(node.getNode(), &events_);
		nodes_.push_back(&node);
		list_mutex_.unlock();
	}

	void wait()
	{
		if (!running_)
		{
			start();
		}
		thread_.join();
	}

	void stop()
	{
		// kill everything
		if (!running_)
		{
			return;
		}

		running_ = false;
        if (thread_.joinable())
		    thread_.join();// wait for it to stop
	}
};

// Note this has a max rate of ~500 Hz per timer
// On Windows its harder to do better than this without blocking all the time
// So do your own loop if you need better
class BlockingSpinnerWithTimers
{
	std::vector<Node*> nodes_;

	bool running_;

	std::thread thread_;
	std::mutex list_mutex_;

	ps_event_set_t events_;
public:

	// todo make it work with more than one thread
	BlockingSpinnerWithTimers(int num_threads = 1) : running_(false)
	{
      ps_event_set_create(&events_);
	}

	~BlockingSpinnerWithTimers()
	{
		if (running_)
		{
			stop();
		}

		// close out any events
		ps_event_set_destroy(&events_);
	}

	void addNode(Node& node)
	{
		list_mutex_.lock();

		// build a wait list for all nodes
		ps_node_create_events(node.getNode(), &events_);
		nodes_.push_back(&node);
		list_mutex_.unlock();
	}

	void start()
	{
		running_ = true;
		thread_ = std::thread([this]()
		{
			while (running_ && ps_okay())
			{
                //printf("Entering thread\n");
				list_mutex_.lock();
				if (ps_event_set_count(&events_) == 0)
				{
                    printf("Waiting for events\n");
					ps_sleep(10);
				}
				else
				{
					int timeout = 1000;
					if (timers_.size())
					{
						// get time to neareast timer
						Time now = Time::now();
						Time next = timers_[0].next_trigger;
						for (size_t i = 1; i < timers_.size(); i++)
						{
							if (timers_[i].next_trigger < next)
							{
								next = timers_[i].next_trigger;
							}
						}
						Duration delay = next - now;
						timeout = std::max<int>(delay.usec / 1000, 1);
						// todo there seems to be some kind of bug here that causes the timeout to be
						// occasionally exceptionally long

						// this line is necessary anyways, but happens to work around the above bug
						timeout = std::min<int>(timeout, 1000);// make sure we dont block too long
                        //printf("setting timeout to %i\n", timeout);
					}
					list_mutex_.unlock();
					ps_event_set_wait(&events_, timeout);
					list_mutex_.lock();
				}

				// check all timers
				for (auto& timer : timers_)
				{
					Time now = Time::now();
					if (now > timer.next_trigger)
					{
						// so either trigger at least 2 ms from now or at the next desired time
						// this makes sure we dont develop a backlog
                        //printf("Calling timer\n");
						timer.next_trigger = std::max(now + Duration(0, 2000), timer.next_trigger + timer.period);
						timer.fn();
					}
				}

				// check all nodes
				for (auto node : nodes_)
				{
					node->lock_.lock();
					ps_node_spin(node->getNode());

					// todo how to make this not scale with subscriber count...
					// though, it isnt very important here
					// its probably worth more effort just make it not run any of this if we get no messages
					// we got a message, now call a subscriber
					for (size_t i = 0; i < node->subscribers_.size(); i++)
					{
						// this doesnt ensure switching subs, but works
						while (node->subscribers_[i]->CallOne()) {};
					}
					node->lock_.unlock();
				}
				list_mutex_.unlock();
			}
		});
	}

	struct Timer
	{
		Time next_trigger;
		Duration period;
		std::function<void()> fn;
	};
	std::vector<Timer> timers_;
	void addTimer(double period, std::function<void()> cb)
	{
		Timer t;
		t.period = Duration(period);
		t.next_trigger = Time::now() + t.period;
		t.fn = cb;
		list_mutex_.lock();
		timers_.push_back(t);
		list_mutex_.unlock();
	}

	void wait()
	{
		if (!running_)
		{
			start();
		}
		thread_.join();
	}

	void stop()
	{
		// kill everything
		if (!running_)
		{
			return;
		}

		running_ = false;
        if (thread_.joinable())
		    thread_.join();// wait for it to stop
	}
};


class Spinner
{
	std::vector<Node*> nodes_;

	bool running_;

	std::thread thread_;
	std::mutex list_mutex_;
public:

	// todo make it work with more than one thread
	Spinner(int num_threads = 1) : running_(false)
	{

	}

	void start()
	{
		running_ = true;
		thread_ = std::thread([this]()
		{
			while (running_ && ps_okay())
			{
				list_mutex_.lock();
				for (auto node : nodes_)
				{
					node->lock_.lock();
					if (ps_node_spin(node->getNode()))
					{

					}
					// todo how to make this not scale with subscriber count...
					// though, it isnt very important here
					// its probably worth more effort just make it not run any of this if we get no messages
					// we got a message, now call a subscriber
					for (size_t i = 0; i < node->subscribers_.size(); i++)
					{
						// this doesnt ensure switching subs, but works
						while (node->subscribers_[i]->CallOne()) {};
					}
					node->lock_.unlock();
				}
				list_mutex_.unlock();

				ps_sleep(10);// make this configurable?
			}
		});
	}

	~Spinner()
	{
		if (running_)
		{
			stop();
		}
	}

	void addNode(Node& node)
	{
		list_mutex_.lock();
		nodes_.push_back(&node);
		list_mutex_.unlock();
	}

	void wait()
	{
		if (!running_)
		{
			start();
		}
		thread_.join();
	}

	void stop()
	{
		// kill everything
		if (!running_)
		{
			return;
		}

		running_ = false;
		thread_.join();// wait for it to stop
	}
};

}
