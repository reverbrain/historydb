#ifndef APP_TEST2_H
#define APP_TEST2_H

#include <memory>

namespace History
{
	class IProvider;
}

void Test2(std::shared_ptr<History::IProvider> provider);

#endif //APP_TEST2_H