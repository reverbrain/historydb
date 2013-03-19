#ifndef APP_TEST2_H
#define APP_TEST2_H

#include <memory>

namespace history {
	class iprovider;
} /* namespace history */

void test2(std::shared_ptr<history::iprovider> provider);

#endif //APP_TEST2_H