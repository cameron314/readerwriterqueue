// ©2013 Cameron Desrochers.
// Provides an extremely basic unit testing framework.

#include <cstdio>
#include <string>
#include <map>
#include <vector>
#include <type_traits>
#include <typeinfo>

#ifdef __GNUG__
#include <cxxabi.h>
#include <cstdlib>
#endif



#define REGISTER_TEST(testName) registerTest(#testName, &subclass_t::testName)

#define ASSERT_OR_FAIL(expr) { if (!(expr)) { notifyTestFailed(__LINE__, #expr); return false; } }
#define SUCCEED() { return true; }



// Uses CRTP
template<typename TSubclass>
class TestClass
{
public:
	static void notifyTestFailed(int line, const char* expr)
	{
		std::printf("    FAILED!\n    ******* Assertion failed (line %d): %s\n\n", line, expr);
	}
	
	bool run()
	{
		bool success = true;
		for (auto it = testVec.cbegin(); it != testVec.cend(); ++it) {
			if (!execTest(*it)) {
				success = false;
			}
		}
		return success;
	}
	
	bool run(std::vector<std::string> const& which)
	{
		bool success = true;
		for (auto it = which.begin(); it != which.end(); ++it) {
			if (!execTest(*testMap.find(*it))) {
				success = false;
			}
		}
		return success;
	}
	
protected:
	typedef TSubclass subclass_t;

	void registerTest(const char* name, bool (subclass_t::* method)())
	{
		testVec.push_back(std::make_pair(std::string(name), method));
		testMap[std::string(name)] = method;
	}
	
	bool execTest(std::pair<std::string, bool (subclass_t::*)()> const& testRef)
	{
		std::printf("%s::%s... \n", demangle_type_name(typeid(subclass_t).name()).c_str(), testRef.first.c_str());
		
		bool result = (static_cast<subclass_t*>(this)->*testRef.second)();
		
		if (result) {
			std::printf("    passed\n\n");
		}
		return result;
	}
	
private:
	static std::string demangle_type_name(const char* name)
	{
#ifdef __GNUG__
		// Adapted from http://stackoverflow.com/a/4541470/21475
		int status = -4;
		char* res = abi::__cxa_demangle(name, nullptr, nullptr, &status);

		const char* const demangled_name = (status == 0) ? res : name;
		std::string ret(demangled_name);

		std::free(res);
		return ret;
#else
		return name;
#endif
	}
	
protected:
	std::vector<std::pair<std::string, bool (TSubclass::*)()> > testVec;
	std::map<std::string, bool (TSubclass::*)()> testMap;
};
