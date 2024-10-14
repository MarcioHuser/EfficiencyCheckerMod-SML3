#include "Util/EfficiencyCheckerConfiguration.h"
#include "Util/ECMOptimize.h"
#include "EfficiencyCheckerBuilding.h"
#include "Buildables/FGBuildableFrackingActivator.h"
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Patching/NativeHookManager.h"
#include "Util/ECMLogging.h"

FEfficiencyChecker_ConfigStruct AEfficiencyCheckerConfiguration::configuration;

void AEfficiencyCheckerConfiguration::SetEfficiencyCheckerConfiguration(const FEfficiencyChecker_ConfigStruct& in_configuration)
{
	configuration = in_configuration;

	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	EC_LOG_Display(TEXT("setConfiguration"));

	if (configuration.updateTimeout <= 0)
	{
		configuration.updateTimeout = 15;
	}

	EC_LOG_Display(TEXT("autoUpdate = "), configuration.autoUpdate ? TEXT("true") : TEXT("false"));
	EC_LOG_Display(TEXT("autoUpdateTimeout = "), configuration.autoUpdateTimeout);
	EC_LOG_Display(TEXT("autoUpdateDistance = "), configuration.autoUpdateDistance);
	EC_LOG_Display(TEXT("logLevel = "), configuration.logLevel);
	EC_LOG_Display(TEXT("ignoreStorageTeleporter = "), configuration.ignoreStorageTeleporter ? TEXT("true") : TEXT("false"));
	EC_LOG_Display(TEXT("updateTimeout = "), configuration.updateTimeout);
	EC_LOG_Display(TEXT("logicVersion = "), configuration.logicVersion);

	static auto hooked = false;
	static FDelegateHandle AFGBuildableFactory_SetPendingPotential;
	static FDelegateHandle AFGBuildableFactory_SetPendingProductionBoost;
	static FDelegateHandle AFGBuildableFrackingActivator_SetPendingPotential;
	static FDelegateHandle AFGBuildableGeneratorFuel_SetPendingPotential;

#if UE_BUILD_SHIPPING
	if (configuration.autoUpdate)
	{
		if (!hooked)
		{
			hooked = true;

			{
				void* ObjectInstance = GetMutableDefault<AFGBuildableFactory>();

				AFGBuildableFactory_SetPendingPotential = SUBSCRIBE_METHOD_VIRTUAL_AFTER(
					AFGBuildableFactory::SetPendingPotential,
					ObjectInstance,
					[](AFGBuildableFactory * factory, float potential) {
					AEfficiencyCheckerBuilding::setPendingPotentialCallback(factory, potential);
					}
					);

				AFGBuildableFactory_SetPendingProductionBoost = SUBSCRIBE_METHOD_VIRTUAL_AFTER(
					AFGBuildableFactory::SetPendingProductionBoost,
					ObjectInstance,
					[](AFGBuildableFactory * factory, float productionBoost) {
					AEfficiencyCheckerBuilding::setPendingProductionBoostCallback(factory, productionBoost);
					}
					);
			}

			{
				void* ObjectInstance = GetMutableDefault<AFGBuildableFrackingActivator>();

				AFGBuildableFrackingActivator_SetPendingPotential = SUBSCRIBE_METHOD_VIRTUAL_AFTER(
					AFGBuildableFrackingActivator::SetPendingPotential,
					ObjectInstance,
					[](AFGBuildableFrackingActivator * factory, float potential) {
					AEfficiencyCheckerBuilding::setPendingPotentialCallback(factory, potential);
					}
					);
			}

			{
				void* ObjectInstance = GetMutableDefault<AFGBuildableGeneratorFuel>();

				AFGBuildableGeneratorFuel_SetPendingPotential = SUBSCRIBE_METHOD_VIRTUAL_AFTER(
					AFGBuildableGeneratorFuel::SetPendingPotential,
					ObjectInstance,
					[](AFGBuildableGeneratorFuel * factory, float potential) {
					AEfficiencyCheckerBuilding::setPendingPotentialCallback(factory, potential);
					}
					);
			}
		}
	}
	else
	{
		if (hooked)
		{
			hooked = false;

			if (AFGBuildableFactory_SetPendingPotential.IsValid())
			{
				UNSUBSCRIBE_METHOD(
					AFGBuildableFactory::SetPendingPotential,
					AFGBuildableFactory_SetPendingPotential
					);
			}

			if (AFGBuildableFactory_SetPendingProductionBoost.IsValid())
			{
				UNSUBSCRIBE_METHOD(
					AFGBuildableFactory::SetPendingProductionBoost,
					AFGBuildableFactory_SetPendingProductionBoost
					);
			}

			if (AFGBuildableFrackingActivator_SetPendingPotential.IsValid())
			{
				UNSUBSCRIBE_METHOD(
					AFGBuildableFrackingActivator::SetPendingPotential,
					AFGBuildableFrackingActivator_SetPendingPotential
					);
			}

			if (AFGBuildableGeneratorFuel_SetPendingPotential.IsValid())
			{
				UNSUBSCRIBE_METHOD(
					AFGBuildableGeneratorFuel::SetPendingPotential,
					AFGBuildableGeneratorFuel_SetPendingPotential
					);
			}
		}
	}
#endif

	EC_LOG_Display(TEXT("==="));
}

void AEfficiencyCheckerConfiguration::GetEfficiencyCheckerConfiguration(bool& out_autoUpdate, int& out_logLevel, float& out_autoUpdateTimeout, float& out_autoUpdateDistance)
{
	EC_LOG_Display_Condition(TEXT("AEfficiencyCheckerConfiguration: GetEfficiencyCheckerSettings"));

	out_autoUpdate = AEfficiencyCheckerConfiguration::configuration.autoUpdate;
	out_logLevel = AEfficiencyCheckerConfiguration::configuration.logLevel;
	out_autoUpdateTimeout = AEfficiencyCheckerConfiguration::configuration.autoUpdateTimeout;
	out_autoUpdateDistance = AEfficiencyCheckerConfiguration::configuration.autoUpdateDistance;
}

bool AEfficiencyCheckerConfiguration::IsAutoUpdateEnabled()
{
	return configuration.autoUpdate;
}

int AEfficiencyCheckerConfiguration::GetLogLevelECM()
{
	return configuration.logLevel;
}

float AEfficiencyCheckerConfiguration::GetAutoUpdateTimeout()
{
	return configuration.autoUpdateTimeout;
}

float AEfficiencyCheckerConfiguration::GetAutoUpdateDistance()
{
	return configuration.autoUpdateDistance;
}
