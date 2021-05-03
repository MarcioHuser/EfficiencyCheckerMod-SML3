#pragma once

#include "MachineStatusIncludeType.generated.h"

UENUM(Blueprintable, Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EMachineStatusIncludeType: uint8
{
	MSIT_None = 0 UMETA(DisplayName = "None"),
	MSIT_Unpowered = 1 << 0 UMETA(DisplayName = "Include Unpowered"),
	MSIT_Paused = 1 << 1 UMETA(DisplayName = "Include Paused"),
	//
	MSIT_All = MSIT_Unpowered | MSIT_Paused UMETA(DisplayName = "Include All"),
};

ENUM_CLASS_FLAGS(EMachineStatusIncludeType);

#define TO_EMachineStatusIncludeType(Enum) static_cast<uint32>(Enum);
#define Has_EMachineStatusIncludeType(Value, Enum) ((static_cast<uint32>(Value) & static_cast<uint32>(Enum)) == static_cast<uint32>(Enum))
#define Add_EMachineStatusIncludeType(Value, Enum) (static_cast<uint32>(Value) | static_cast<uint32>(Enum))
#define Remove_EMachineStatusIncludeType(Value, Enum) (static_cast<uint32>(Value) & ~static_cast<uint32>(Enum))
