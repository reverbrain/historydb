#ifndef APP_TEST2_H
#define APP_TEST2_H

#include <memory>

namespace history {
	class provider;
} /* namespace history */

void test2(std::shared_ptr<history::provider> provider);

#endif //APP_TEST2_H
