#pragma once

#include "AutoUpdateType.generated.h"

UENUM(BlueprintType)
enum class EAutoUpdateType: uint8
{
	AUT_INVALID UMETA(DisplayName = "Invalid"),
	AUT_USE_DEFAULT UMETA(DisplayName = "Use Default"),
	AUT_ENABLED UMETA(DisplayName = "Enabled"),
	AUT_DISABLED UMETA(DisplayName = "Disabled"),
};
