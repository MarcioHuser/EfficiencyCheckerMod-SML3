#pragma once

#include <map>

#include "ComponentFilter.h"
#include "Resources/FGItemDescriptor.h"

class CollectSettings 
{
public:
	CollectSettings();
	CollectSettings(CollectSettings& baseCollectSettings);
	virtual ~CollectSettings() = default;

#define GET_SET_WRAPPER(Type, SetType, SetTypePtr, Name, name, defaultValue) \
	protected: \
		Type name = defaultValue; \
		Type* name##Ptr = &name; \
	public: \
		virtual Type& Get##Name() \
		{ \
			return *name##Ptr; \
		} \
		virtual void Set##Name(SetType in_##name) \
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

#define GET_SET_WRAPPER_SIMPLE(Type, Name, name, defaultValue) GET_SET_WRAPPER(Type, Type, Type*, Name, name, defaultValue)

#define MAP_ITEM_INT32 std::map<TSubclassOf<class UFGItemDescriptor>, int32>
#define MAP_ACTOR_ITEM std::map<AActor*, TSet<TSubclassOf<class UFGItemDescriptor>>>

	GET_SET_WRAPPER_SIMPLE(EResourceForm, ResourceForm, resourceForm, EResourceForm::RF_INVALID)
	GET_SET_WRAPPER(class UFGConnectionComponent*, class UFGConnectionComponent* const, class UFGConnectionComponent**, Connector, connector, nullptr)
	GET_SET_WRAPPER_SIMPLE(bool, CustomInjectedInput, customInjectedInput, false)

	virtual float
	GetInjectedInputTotal()
	{
		float total = 0;

		for (auto it : GetInjectedInput())
		{
			total += it.second;
		}

		return total;
	}

	GET_SET_WRAPPER(MAP_ITEM_INT32, const MAP_ITEM_INT32&, MAP_ITEM_INT32*, InjectedInput, injectedInput, MAP_ITEM_INT32())
	GET_SET_WRAPPER_SIMPLE(float, LimitedThroughput, limitedThroughput, 0)
	GET_SET_WRAPPER_SIMPLE(bool, CustomRequiredOutput, customRequiredOutput, false)

	virtual float
	GetRequiredOutputTotal()
	{
		float total = 0;

		for (auto it : GetRequiredOutput())
		{
			total += it.second;
		}

		return total;
	}

	GET_SET_WRAPPER(MAP_ITEM_INT32, const MAP_ITEM_INT32&, MAP_ITEM_INT32*, RequiredOutput, requiredOutput, MAP_ITEM_INT32())
	GET_SET_WRAPPER(MAP_ACTOR_ITEM, const MAP_ACTOR_ITEM&, MAP_ACTOR_ITEM*, SeenActors, seenActors, MAP_ACTOR_ITEM())
	GET_SET_WRAPPER(TSet<class AFGBuildable*>, const TSet<class AFGBuildable*>&, TSet<class AFGBuildable*>*, Connected, connected, TSet<class AFGBuildable*>())
	GET_SET_WRAPPER(
		TSet<TSubclassOf<class UFGItemDescriptor>>,
		const TSet<TSubclassOf<class UFGItemDescriptor>>&,
		TSet<TSubclassOf<class UFGItemDescriptor>>*,
		InjectedItems,
		injectedItems,
		TSet<TSubclassOf<class UFGItemDescriptor>>()
		)
	GET_SET_WRAPPER(FComponentFilter, const FComponentFilter&, FComponentFilter*, CurrentFilter, currentFilter, FComponentFilter())
	GET_SET_WRAPPER(class AFGBuildableSubsystem*, class AFGBuildableSubsystem* const, class AFGBuildableSubsystem**, BuildableSubsystem, buildableSubsystem, nullptr)
	GET_SET_WRAPPER_SIMPLE(int, Level, level, 0)
	GET_SET_WRAPPER_SIMPLE(bool, Overflow, overflow, false)
	GET_SET_WRAPPER(FString, const FString&, FString*, Indent, indent, TEXT(""))
	GET_SET_WRAPPER(int64, const int64&, int64*, Timeout, timeout, 0)
	GET_SET_WRAPPER_SIMPLE(int32, MachineStatusIncludeType, machineStatusIncludeType, 0)
};
