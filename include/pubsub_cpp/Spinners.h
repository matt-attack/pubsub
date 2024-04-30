#pragma once

#include <pubsub_cpp/Time.h>

#undef min
#undef max

namespace pubsub
{
class BlockingSpinner
{
	Node* node_;

	bool running_;

	std::thread thread_;

public:

	// todo make it work with more than one thread
	BlockingSpinner(int num_threads = 1) : running_(false), node_(0)
	{

	}

	void start()
	{
		running_ = true;
		thread_ = std::thread([this]()
		{
			while (running_ && ps_okay())
			{
				if (node_ == 0)
				{
					ps_sleep(10);
                    continue;
				}
				else
				{
                    ps_node_wait(node_->getNode(), 1000);
				}
				node_->lock_.lock();
				int res = 0;
				if (res = ps_node_spin(node_->getNode()))
				{
                	//printf("Received %i messages\n", res);
					// we got a message, now call a subscriber
					for (size_t i = 0; i < node_->subscribers_.size(); i++)
					{
						// this doesnt ensure switching subs, but works
						while (node_->subscribers_[i]->CallOne()) {};
					}
				}
				//printf("loop\n");

				// todo how to make this not scale with subscriber count...
				// though, it isnt very important here messages
				node_->lock_.unlock();
			}
		});
	}

	~BlockingSpinner()
	{
		if (running_)
		{
			stop();
		}
	}

	void setNode(Node& node)
	{
        node_ = &node;
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
	Node* node_;

	bool running_;

	std::thread thread_;
	std::mutex list_mutex_;

	struct Timer
	{
		Time next_trigger;
		Duration period;
		std::function<void()> fn;
	};
	std::vector<Timer> timers_;

	//ps_event_set_t events_;
public:

	// todo make it work with more than one thread
	BlockingSpinnerWithTimers(int num_threads = 1) : running_(false), node_(0)
	{
      //ps_event_set_create(&events_);
	}

	~BlockingSpinnerWithTimers()
	{
		if (running_)
		{
			stop();
		}

		// close out any events
		//ps_event_set_destroy(&events_);
	}

	void setNode(Node& node)
	{
        node_ = &node;
		//list_mutex_.lock();

		// build a wait list for all nodes
		//ps_node_create_events(node.getNode(), &events_);
		//nodes_.push_back(&node);
		//list_mutex_.unlock();
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
				if (node_ == 0)//ps_event_set_count(&events_) == 0)
				{
                    printf("Waiting for events\n");
					ps_sleep(10);
                    continue;
				}
				else
				{
					int timeout = 1000000;// in us
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
						timeout = std::max<int>(delay.usec, 100);// 10khz is max rate
						// todo there seems to be some kind of bug here that causes the timeout to be
						// occasionally exceptionally long

						// this line is necessary anyways, but happens to work around the above bug
						timeout = std::min<int>(timeout, 1000000);// make sure we dont block too long
                        //printf("setting timeout to %i\n", timeout);
					}
					list_mutex_.unlock();
                    //ps_node_wait(node_->getNode(), 0);
					// todo allow finer grained waits
					//if (timeout > 2)
					{
						// allows for fine grained waits
                        ps_event_set_set_timer(&node_->getNode()->events, timeout);// in us
						ps_node_wait(node_->getNode(), 1000);
						//ps_event_set_wait(&events_, timeout);
					}
					list_mutex_.lock();
				}

				// check all timers
				for (auto& timer : timers_)
				{
					Time now = Time::now();
					if (now >= timer.next_trigger)
					{
                        //printf("Calling timer\n");
						timer.next_trigger = timer.next_trigger + timer.period;
						timer.fn();
					}
				}

				// check all nodes
				//for (auto node : nodes_)
                if (node_)
				{
					node_->lock_.lock();
					ps_node_spin(node_->getNode());

					// todo how to make this not scale with subscriber count...
					// though, it isnt very important here
					// its probably worth more effort just make it not run any of this if we get no messages
					// we got a message, now call a subscriber
					for (size_t i = 0; i < node_->subscribers_.size(); i++)
					{
						// this doesnt ensure switching subs, but works
						while (node_->subscribers_[i]->CallOne()) {};
					}
					node_->lock_.unlock();
				}
				list_mutex_.unlock();
			}
		});
	}

	void addTimer(double period, std::function<void()> cb)
	{
		Timer t;
		t.period = Duration(period);
		t.next_trigger = Time::now();
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
					int res = 0;
					if (res = ps_node_spin(node->getNode()))
					{
                    	printf("Received %i messages\n", res);
						// we got a message, now call a subscriber
						// todo how to make this not scale with subscriber count...
						for (size_t i = 0; i < node->subscribers_.size(); i++)
						{
							// this doesnt ensure switching subs, but works
							while (node->subscribers_[i]->CallOne()) {};
						}
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
