#pragma once


#include "GameFramework/Actor.h"
#include "EfficiencyChecker_ConfigStruct.h"

#include "EfficiencyCheckerConfiguration.generated.h"

UCLASS()
class EFFICIENCYCHECKERMOD_API AEfficiencyCheckerConfiguration : public AActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="EfficiencyCheckerConfiguration")
	static void SetEfficiencyCheckerConfiguration
	(
		UPARAM(DisplayName = "Configuration") const struct FEfficiencyChecker_ConfigStruct& in_configuration
	);

	UFUNCTION(BlueprintCallable, Category="EfficiencyCheckerConfiguration")
	static void GetEfficiencyCheckerConfiguration
	(
		UPARAM(DisplayName = "Auto Update") bool& out_autoUpdate,
		UPARAM(DisplayName = "Log Level") int& out_logLevel,
		UPARAM(DisplayName = "Auto Update Timeout") float& out_autoUpdateTimeout,
		UPARAM(DisplayName = "Auto Updat Distance") float& out_autoUpdateDistance
	);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EfficiencyCheckerConfiguration")
	static bool IsAutoUpdateEnabled();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EfficiencyCheckerConfiguration")
	static int GetLogLevelECM();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EfficiencyCheckerConfiguration")
	static float GetAutoUpdateTimeout();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EfficiencyCheckerConfiguration")
	static float GetAutoUpdateDistance();

public:
	static FEfficiencyChecker_ConfigStruct configuration;
};
