#include "boost\atomic.hpp"

int x() {
	boost::atomic<int> xyz;
	boost::atomics::detail::lockpool::scoped_lock lp(0);
	return 0;
}