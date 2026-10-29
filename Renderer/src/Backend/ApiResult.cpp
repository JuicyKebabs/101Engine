#include "ApiResult.h"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace rendering::backend
{

Result MakeApiFailure(const char* operation, HRESULT hr)
{
	std::ostringstream message;

	message << operation
			<< " failed. HRESULT: 0x"
			<< std::uppercase
			<< std::hex
			<< std::setw(8)
			<< std::setfill('0')
			<< static_cast<std::uint32_t>(hr);

	return { false, message.str() };
}

}