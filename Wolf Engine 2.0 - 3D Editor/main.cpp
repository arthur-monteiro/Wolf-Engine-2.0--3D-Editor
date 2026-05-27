#include "SystemManager.h"

int main()
{
#ifdef __linux__
	setenv("JSC_SIGNAL_FOR_GC", "34", 1);
#endif

	const std::unique_ptr<SystemManager> s(new SystemManager);
	s->run();

	return EXIT_SUCCESS;
}