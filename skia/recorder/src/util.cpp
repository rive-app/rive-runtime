#include <cmath>
#include <util.hpp>

// template <typename... Args>
// std::string string_format(const std::string& format, Args... args)
// {
// 	fprintf(stderr, "waht");
// 	int size = snprintf(nullptr, 0, format.c_str(), args...) +
// 	           1; // Extra space for '\0'
// 	if (size <= 0)
// 	{
// 		throw std::runtime_error("Error during formatting.");
// 	}
// 	std::unique_ptr<char[]> buf(new char[size]);
// 	snprintf(buf.get(), size, format.c_str(), args...);
// 	return std::string(buf.get(),
// 	                   buf.get() + size - 1); // We don't want the '\0' inside
// }

// // QUESTION: inline looks fun. so this copy pastes the code around i guess,
// but
// // its considered faster than referencing code?
// inline bool file_exists(const std::string& name)
// {
// 	//
// https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exist-using-standard-c-c11-c
// 	struct stat buffer;
// 	return (stat(name.c_str(), &buffer) == 0);
// }

int nextMultipleOf(float number, int multiple_of)
{
	return std::ceil(number / multiple_of) * multiple_of;
}
