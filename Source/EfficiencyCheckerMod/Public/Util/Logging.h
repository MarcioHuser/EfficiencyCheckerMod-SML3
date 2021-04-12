#pragma once

#include "Logging/LogMacros.h"

#include <sstream>

class CommaLog
{
public:
	template <typename T>
	inline CommaLog&
	operator,(const T& value)
	{
		wos << value;

		return *this;
	}

	inline CommaLog&
	operator,(const FString& value)
	{
		wos << *value;

		return *this;
	}

	inline CommaLog&
	operator,(const FText& value)
	{
		wos << *value.ToString();

		return *this;
	}

	std::wostringstream wos;
};

DECLARE_LOG_CATEGORY_EXTERN(LogEfficiencyChecker, Log, All)

#define EC_LOG_Verbosity(verbosity, first, ...) \
	{ \
		CommaLog l; \
		l, first, ##__VA_ARGS__; \
		UE_LOG(LogEfficiencyChecker, verbosity, TEXT("%s"), l.wos.str().c_str()) \
	}

#define EC_LOG_Display(first, ...) EC_LOG_Verbosity(Display, first, ##__VA_ARGS__)
#define EC_LOG_Warning(first, ...) EC_LOG_Verbosity(Warning, first, ##__VA_ARGS__)
#define EC_LOG_Error(first, ...) EC_LOG_Verbosity(Error, first, ##__VA_ARGS__)
