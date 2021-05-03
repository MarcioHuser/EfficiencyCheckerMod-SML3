#pragma once

#include "PlacementType.generated.h"

UENUM(BlueprintType)
enum class EPlacementType: uint8
{
	PT_INVALID UMETA(DisplayName = "Invalid"),
	PT_GROUND UMETA(DisplayName = "Ground"),
	PT_WALL UMETA(DisplayName = "Wall"),
};
