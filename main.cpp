///////////////////////////////////////////////////////////////////////////////
///                         SIMPLE LOCK FREE STACK                          ///
///           Copyright (c) 2014 Michael Gazonda - http://mgaz.ca           ///
///                              CPOL Licensed                              ///
///       See CPOL.htm, or http://www.codeproject.com/info/cpol10.aspx      ///
///               http://www.codeproject.com/Articles/801537/               ///
///                 https://github.com/MGaz/lock_free_stack                 ///
///////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

// These are the testing parameters, you can change them
#define data_count 1
#define loop_count 100000000
#define thread_count 1

#if _WIN32
#	if _WIN64
#		define PROCESSOR_BITS		64
#	else
#		define PROCESSOR_BITS		32
#	endif
#	if _MSC_VER > 1800
#		define __noexcept			noexcept
#	else
#		define __noexcept
#	endif
#endif

#if __GNUC__
#	if __x86_64__ || __ppc64__
#		define PROCESSOR_BITS		64
#	else
#		define PROCESSOR_BITS		32
#	endif
#	define __noexcept				noexcept
#endif

#ifdef __APPLE__
#	import "TargetConditionals.h"
#	ifdef __MACH__
#		define PROCESSOR_BITS		64
#	endif
#	define __noexcept				noexcept
#endif

struct node
{
	node *n_;
#if PROCESSOR_BITS == 64
	using stack_id = uint16_t;
	
	inline node() __noexcept : n_{ nullptr }           { }
	inline node(node* n) __noexcept : n_{ n }          { }
	inline void create_id(const node& nid)             { ((stack_id*)this)[3] = ((const stack_id*)&nid)[3] + 1; }
	inline node* next_pointer()                        { return (node*)((uint64_t)n_ & 0x0000ffffffffffff); }
#elif PROCESSOR_BITS == 32
	using stack_id = uint32_t;
	stack_id t_;	
	inline node() __noexcept : n_{ nullptr }, t_{ 0 }  { }
	inline node(node* n) __noexcept : n_{ n }, t_{ 0 } { }
	inline void create_id(const node& nid)             { t_ = nid.t_ + 1; }
	inline node* next_pointer()                        { return n_; }
#endif
	inline void set(node* n, const node& nid)          { n_ = n; if (n_) create_id(nid); else create_id(*this); }
};

class stack
{
public:
    void push(node* n)
	{
		node old_head, new_head{ n };
		n->n_ = nullptr;
		
		if (head_.compare_exchange_weak(old_head, new_head))
			return;
		
		for (;;)
		{
			n->n_ = old_head.n_;
			new_head.create_id(old_head);
			if (head_.compare_exchange_weak(old_head, new_head))
				return;
			
			// testing conditions _never_ reach here, so why does this line make the program slower??
			std::this_thread::sleep_for(std::chrono::nanoseconds(5));
			
			// debug break is used to confirm execution never reaches here
			__debugbreak();
		}
	}
	bool pop(node*& n)
	{
		node old_head, new_head;
		n = nullptr;
		while (!head_.compare_exchange_weak(old_head, new_head))
		{
			n = old_head.next_pointer();
			if (!n)
				break;
			new_head.set(n->n_, old_head);
		}
		return n != nullptr;
	}
protected:
	std::atomic<node> head_;
};	

void thread_test(stack *s, std::atomic<uint64_t> *max_elapsed, std::atomic<size_t> *empty_count, size_t index)
{
	node* d[data_count];
	for (size_t i = 0; i < data_count; ++i)
		d[i] = new node;
	
	std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

	for (size_t loop = 0; loop < loop_count; ++loop)
	{
		for (size_t i = 0; i < data_count; ++i)
		{
			if (d[i])
				s->push(d[i]);
		}
		
		for (size_t i = 0; i < data_count; ++i)
			s->pop((node*&)d[i]);
		
	}
	
	std::chrono::high_resolution_clock::time_point finish = std::chrono::high_resolution_clock::now();
	std::chrono::milliseconds span = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
	
	*max_elapsed = span.count();
	
	for (size_t i = 0; i < data_count; ++i)
	{
		if (d[i])
			delete d[i];
		else
			(*empty_count)++;
	}
}

int main(int argc, const char * argv[])
{
	std::thread threads[thread_count];
	std::atomic<uint64_t> max_elapsed{ 0 };
	std::atomic<size_t> empty_count{ 0 };
	stack s;
		
	for (size_t i = 0; i < thread_count; ++i)
		threads[i] = std::thread(thread_test, &s, &max_elapsed, &empty_count, i);
	
	for (size_t i = 0; i < thread_count; ++i)
		threads[i].join();
	
	size_t operation_count = data_count * loop_count * thread_count * 2;
	std::cout << "operations per second: " << operation_count / (max_elapsed.load() > 0 ? max_elapsed.load() : 1) * 1000 << "\r\n";
	std::cout << "press any key to exit\r\n";
	std::getchar();
    return 0;
}
