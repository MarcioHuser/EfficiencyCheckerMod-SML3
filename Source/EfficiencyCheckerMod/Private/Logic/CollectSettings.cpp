#include "Logic/CollectSettings.h"
#include "Util/EfficiencyCheckerOptimize.h"

#ifndef OPTIMIZE
#pragma optimize( "", off )
#endif

CollectSettings::CollectSettings()
{
}

CollectSettings::CollectSettings(CollectSettings& baseCollectSettings)
{
	resourceFormPtr = &baseCollectSettings.GetResourceForm();

	connectorPtr = &baseCollectSettings.GetConnector();

	customInjectedInputPtr = &baseCollectSettings.GetCustomInjectedInput();
	injectedInputPtr = &baseCollectSettings.GetInjectedInput();
	limitedThroughputPtr = &baseCollectSettings.GetLimitedThroughput();
	customRequiredOutputPtr = &baseCollectSettings.GetCustomRequiredOutput();
	requiredOutputPtr = &baseCollectSettings.GetRequiredOutput();

	seenActorsPtr = &baseCollectSettings.GetSeenActors();

	connectedPtr = &baseCollectSettings.GetConnected();

	injectedItemsPtr = &baseCollectSettings.GetInjectedItems();

	currentFilterPtr = &baseCollectSettings.GetCurrentFilter();

	buildableSubsystemPtr = &baseCollectSettings.GetBuildableSubsystem();

	levelPtr = &baseCollectSettings.GetLevel();

	overflowPtr = &baseCollectSettings.GetOverflow();

	indentPtr = &baseCollectSettings.GetIndent();

	timeoutPtr = &baseCollectSettings.GetTimeout();

	machineStatusIncludeTypePtr = &baseCollectSettings.GetMachineStatusIncludeType();
}

#ifndef OPTIMIZE
#pragma optimize( "", on )
#endif
