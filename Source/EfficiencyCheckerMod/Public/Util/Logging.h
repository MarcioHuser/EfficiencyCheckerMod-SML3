#pragma once

#include "Logging/LogMacros.h"

#include <sstream>

#define FUNCTIONSTR2(x) #x
#define FUNCTIONSTR TEXT(FUNCTIONSTR2(__FUNCTION__))

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

#define EC_LOG_Log(first, ...) EC_LOG_Verbosity(Log, first, ##__VA_ARGS__)
#define EC_LOG_Display(first, ...) EC_LOG_Verbosity(Display, first, ##__VA_ARGS__)
#define EC_LOG_Warning(first, ...) EC_LOG_Verbosity(Warning, first, ##__VA_ARGS__)
#define EC_LOG_Error(first, ...) EC_LOG_Verbosity(Error, first, ##__VA_ARGS__)

#define IS_EC_LOG_LEVEL(level) (AEfficiencyCheckerLogic::configuration.logLevel > 0 && AEfficiencyCheckerLogic::configuration.logLevel >= static_cast<uint8>(level))

#define EC_LOG_Log_Condition(first, ...) if(IS_EC_LOG_LEVEL(ELogVerbosity::Log)) EC_LOG_Log(first, ##__VA_ARGS__)
#define EC_LOG_Display_Condition(first, ...) if(IS_EC_LOG_LEVEL(ELogVerbosity::Display)) EC_LOG_Display(first, ##__VA_ARGS__)
#define EC_LOG_Warning_Condition(first, ...) if(IS_EC_LOG_LEVEL(ELogVerbosity::Warning)) EC_LOG_Warning(first, ##__VA_ARGS__)
#define EC_LOG_Error_Condition(first, ...) if(IS_EC_LOG_LEVEL(ELogVerbosity::Error)) EC_LOG_Error(first, ##__VA_ARGS__)
