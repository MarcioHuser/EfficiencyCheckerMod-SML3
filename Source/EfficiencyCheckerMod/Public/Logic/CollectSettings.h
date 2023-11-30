#pragma once

#include "ComponentFilter.h"
#include "Resources/FGItemDescriptor.h"

class ICollectSettings
{
public:
	virtual ~ICollectSettings() = default;
	virtual EResourceForm& GetResourceForm() =0;
	virtual void SetResourceForm(const EResourceForm ResourceForm) =0;

	virtual class UFGConnectionComponent*& GetConnector() =0;
	virtual void SetConnector(class UFGConnectionComponent* const Connector) =0;

	virtual bool& GetCustomInjectedInput() =0;
	virtual void SetCustomInjectedInput(const bool bCustomInjectedInput) =0;

	virtual TMap<TSubclassOf<UFGItemDescriptor>, int32>& GetInjectedInput() =0;
	virtual void SetInjectedInput(const TMap<TSubclassOf<UFGItemDescriptor>, int32>& InjectedInput) =0;

	virtual float
	GetInjectedInputTotal()
	{
		float total = 0;

		for (auto injectedInput : GetInjectedInput())
		{
			total += injectedInput.Value;
		}

		return total;
	}

	virtual float& GetLimitedThroughput() =0;
	virtual void SetLimitedThroughput(const float LimitedThroughput) =0;

	virtual bool& GetCustomRequiredOutput() =0;
	virtual void SetCustomRequiredOutput(const bool bCustomRequiredOutput) =0;

	virtual TMap<TSubclassOf<UFGItemDescriptor>, int32>& GetRequiredOutput() =0;
	virtual void SetRequiredOutput(const TMap<TSubclassOf<UFGItemDescriptor>, int32>& RequiredOutput) =0;

	virtual float
	GetRequiredOutputTotal()
	{
		float total = 0;

		for (auto injectedInput : GetRequiredOutput())
		{
			total += injectedInput.Value;
		}

		return total;
	}

	virtual TMap<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& GetSeenActors() =0;
	virtual void SetSeenActors(const TMap<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>& SeenActors) =0;

	virtual TSet<class AFGBuildable*>& GetConnected() =0;
	virtual void SetConnected(const TSet<class AFGBuildable*>& Connected) =0;

	virtual TSet<TSubclassOf<UFGItemDescriptor>>& GetInjectedItems() =0;
	virtual void SetInjectedItems(const TSet<TSubclassOf<UFGItemDescriptor>>& InjectedItems) =0;

	virtual FComponentFilter& GetCurrentFilter() =0;
	virtual void SetCurrentFilter(const FComponentFilter& CurrentFilter) =0;

	virtual class AFGBuildableSubsystem*& GetBuildableSubsystem() =0;
	virtual void SetBuildableSubsystem(class AFGBuildableSubsystem* const BuildableSubsystem) =0;

	virtual int& GetLevel() =0;
	virtual void SetLevel(const int Level) =0;

	virtual bool& GetOverflow() =0;
	virtual void SetOverflow(const bool bOverflow) =0;

	virtual FString& GetIndent() =0;
	virtual void SetIndent(const FString& Indent) =0;

	virtual int64& GetTimeout() =0;
	virtual void SetTimeout(const int64& Timeout) =0;

	virtual int32& GetMachineStatusIncludeType() =0;
	virtual void SetMachineStatusIncludeType(const int32 MachineStatusIncludeType) =0;
};

class CollectSettings : public ICollectSettings
{
protected:
	EResourceForm resourceForm = EResourceForm::RF_INVALID;

	class UFGConnectionComponent* connector = nullptr;

	bool customInjectedInput = false;
	TMap<TSubclassOf<class UFGItemDescriptor>, int32> injectedInput;
	float limitedThroughput = 0;
	bool customRequiredOutput = false;
	TMap<TSubclassOf<class UFGItemDescriptor>, int32> requiredOutput;

	TMap<AActor*, TSet<TSubclassOf<class UFGItemDescriptor>>> seenActors;

	TSet<class AFGBuildable*> connected;

	TSet<TSubclassOf<UFGItemDescriptor>> injectedItems;

	FComponentFilter currentFilter;

	class AFGBuildableSubsystem* buildableSubsystem = nullptr;

	int level = 0;

	bool overflow = false;

	FString indent = TEXT("");

	int64 timeout = 0;

	int32 machineStatusIncludeType = 0;

public:
	virtual ~CollectSettings() override = default;

#define GET_SET(GetType, SetType, SetTypePtr, Name, name) \
	virtual GetType& Get##Name() override \
	{ \
		return name; \
	} \
	virtual void Set##Name(SetType in_##name) override \
	{ \
		name = in_##name; \
	}

#define GET_SET_SIMPLE(Type, Name, name) GET_SET(Type, Type, Type*, Name, name)
	
	GET_SET_SIMPLE(EResourceForm, ResourceForm, resourceForm)
	GET_SET(class UFGConnectionComponent*, class UFGConnectionComponent* const, class UFGConnectionComponent**, Connector, connector)
	GET_SET_SIMPLE(bool, CustomInjectedInput, customInjectedInput)
	#define TMAP_ITEM_INT32 TMap<TSubclassOf<UFGItemDescriptor>, int32>
	GET_SET(TMAP_ITEM_INT32, const TMAP_ITEM_INT32&, TMAP_ITEM_INT32*, InjectedInput, injectedInput)
	GET_SET_SIMPLE(float, LimitedThroughput, limitedThroughput)
	GET_SET_SIMPLE(bool, CustomRequiredOutput, customRequiredOutput)
	GET_SET(TMAP_ITEM_INT32, const TMAP_ITEM_INT32&, TMAP_ITEM_INT32*, RequiredOutput, requiredOutput)
	#define TMAP_ACTOR_ITEM TMap<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>
	GET_SET(TMAP_ACTOR_ITEM, const TMAP_ACTOR_ITEM&, TMAP_ACTOR_ITEM*, SeenActors, seenActors)
	GET_SET(TSet<AFGBuildable*>, const TSet<AFGBuildable*>&, TSet<AFGBuildable*>*, Connected, connected)
	GET_SET(TSet<TSubclassOf<UFGItemDescriptor>>, const TSet<TSubclassOf<UFGItemDescriptor>>&, TSet<TSubclassOf<UFGItemDescriptor>>*, InjectedItems, injectedItems)
	GET_SET(FComponentFilter, const FComponentFilter&, FComponentFilter*, CurrentFilter, currentFilter)
	GET_SET(class AFGBuildableSubsystem*, class AFGBuildableSubsystem* const, class AFGBuildableSubsystem**, BuildableSubsystem, buildableSubsystem)
	GET_SET_SIMPLE(int, Level, level)
	GET_SET_SIMPLE(bool, Overflow, overflow)
	GET_SET(FString, const FString&, FString*, Indent, indent)
	GET_SET(int64, const int64&, int64*, Timeout, timeout)
	GET_SET_SIMPLE(int32, MachineStatusIncludeType, machineStatusIncludeType)
};

class CollectSettingsWrapper : public CollectSettings
{
	EResourceForm* resourceFormPtr = &resourceForm;

	class UFGConnectionComponent** connectorPtr = &connector;

	bool* customInjectedInputPtr = &customInjectedInput;
	TMap<TSubclassOf<class UFGItemDescriptor>, int32>* injectedInputPtr = &injectedInput;
	float* limitedThroughputPtr = &limitedThroughput;
	bool* customRequiredOutputPtr = &customRequiredOutput;
	TMap<TSubclassOf<class UFGItemDescriptor>, int32>* requiredOutputPtr = &requiredOutput;

	TMap<AActor*, TSet<TSubclassOf<class UFGItemDescriptor>>>* seenActorsPtr = &seenActors;

	TSet<class AFGBuildable*>* connectedPtr = &connected;

	TSet<TSubclassOf<UFGItemDescriptor>>* injectedItemsPtr = &injectedItems;

	FComponentFilter* currentFilterPtr = &currentFilter;

	class AFGBuildableSubsystem** buildableSubsystemPtr = &buildableSubsystem;

	int* levelPtr = &level;

	bool* overflowPtr = &overflow;

	FString* indentPtr = &indent;

	int64* timeoutPtr = &timeout;

	int32* machineStatusIncludeTypePtr = &machineStatusIncludeType;

public:
	CollectSettingsWrapper();
	CollectSettingsWrapper(ICollectSettings& baseCollectSettings);
	virtual ~CollectSettingsWrapper() override = default;

#define GET_SET_WRAPPER(GetType, SetType, SetTypePtr, Name, name) \
	virtual GetType& Get##Name() override \
	{ \
		return *name##Ptr; \
	} \
	virtual void Set##Name(SetType in_##name) override \
	{ \
		*name##Ptr = in_##name; \
	} \
	virtual void Set##Name(SetType in_##name, bool reset) \
	{ \
		if (reset) \
		{ \
			Set##Name##Ptr(nullptr); \
		} \
		Set##Name(in_##name); \
	} \
	virtual void Set##Name##Ptr(SetTypePtr in_##name##Ptr) \
	{ \
		name##Ptr = in_##name##Ptr ? in_##name##Ptr : &name; \
	}

#define GET_SET_WRAPPER_SIMPLE(Type, Name, name) GET_SET_WRAPPER(Type, Type, Type*, Name, name)

	GET_SET_WRAPPER_SIMPLE(EResourceForm, ResourceForm, resourceForm)
	GET_SET_WRAPPER(class UFGConnectionComponent*, class UFGConnectionComponent* const, class UFGConnectionComponent**, Connector, connector)
	GET_SET_WRAPPER_SIMPLE(bool, CustomInjectedInput, customInjectedInput)
#define TMAP_ITEM_INT32 TMap<TSubclassOf<UFGItemDescriptor>, int32>
	GET_SET_WRAPPER(TMAP_ITEM_INT32, const TMAP_ITEM_INT32&, TMAP_ITEM_INT32*, InjectedInput, injectedInput)
	GET_SET_WRAPPER_SIMPLE(float, LimitedThroughput, limitedThroughput)
	GET_SET_WRAPPER_SIMPLE(bool, CustomRequiredOutput, customRequiredOutput)
	GET_SET_WRAPPER(TMAP_ITEM_INT32, const TMAP_ITEM_INT32&, TMAP_ITEM_INT32*, RequiredOutput, requiredOutput)
#define TMAP_ACTOR_ITEM TMap<AActor*, TSet<TSubclassOf<UFGItemDescriptor>>>
	GET_SET_WRAPPER(TMAP_ACTOR_ITEM, const TMAP_ACTOR_ITEM&, TMAP_ACTOR_ITEM*, SeenActors, seenActors)
	GET_SET_WRAPPER(TSet<AFGBuildable*>, const TSet<AFGBuildable*>&, TSet<AFGBuildable*>*, Connected, connected)
	GET_SET_WRAPPER(TSet<TSubclassOf<UFGItemDescriptor>>, const TSet<TSubclassOf<UFGItemDescriptor>>&, TSet<TSubclassOf<UFGItemDescriptor>>*, InjectedItems, injectedItems)
	GET_SET_WRAPPER(FComponentFilter, const FComponentFilter&, FComponentFilter*, CurrentFilter, currentFilter)
	GET_SET_WRAPPER(class AFGBuildableSubsystem*, class AFGBuildableSubsystem* const, class AFGBuildableSubsystem**, BuildableSubsystem, buildableSubsystem)
	GET_SET_WRAPPER_SIMPLE(int, Level, level)
	GET_SET_WRAPPER_SIMPLE(bool, Overflow, overflow)
	GET_SET_WRAPPER(FString, const FString&, FString*, Indent, indent)
	GET_SET_WRAPPER(int64, const int64&, int64*, Timeout, timeout)
	GET_SET_WRAPPER_SIMPLE(int32, MachineStatusIncludeType, machineStatusIncludeType)
};
