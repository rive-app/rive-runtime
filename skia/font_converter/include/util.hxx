#ifndef UTIL_HXX
#define UTIL_HXX

#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>
#include <memory>

template <typename... Args>
std::string string_format(const std::string& format, Args... args)
{
	int size = snprintf(nullptr, 0, format.c_str(), args...) +
	           1; // Extra space for '\0'
	if (size <= 0)
	{
		throw std::runtime_error("Error during formatting.");
	}
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args...);
	return std::string(buf.get(),
	                   buf.get() + size - 1); // We don't want the '\0' inside
}

// QUESTION: inline looks fun. so this copy pastes the code around i guess, but
// its considered faster than referencing code?
inline bool file_exists(const std::string& name)
{
	// https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exist-using-standard-c-c11-c
	struct stat buffer;
	return (stat(name.c_str(), &buffer) == 0);
}

inline int valueOrDefault(int value, int default_value)
{
	return value <= 0 ? default_value : value;
}

inline void scale(int* value, int targetValue, int* otherValue)
{
	if (*value != targetValue)
	{
		*otherValue = *otherValue * targetValue / *value;
		*value = targetValue;
	}
}

#endif