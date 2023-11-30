#pragma once

#include "UObject/UObjectGlobals.h"

inline FString getEnumItemName(const TCHAR* name, int value)
{
	FString valueStr;

	auto MyEnum = FindObject<UEnum>(ANY_PACKAGE, name);
	if (MyEnum)
	{
		MyEnum->AddToRoot();

		valueStr = MyEnum->GetDisplayNameTextByValue(value).ToString();
	}
	else
	{
		valueStr = TEXT("(Unknown)");
	}

	return FString::Printf(TEXT("%s (%d)"), *valueStr, value);
}
