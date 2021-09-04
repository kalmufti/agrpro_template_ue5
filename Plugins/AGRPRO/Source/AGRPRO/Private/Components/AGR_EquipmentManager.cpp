// Copyright Adam Grodzki All Rights Reserved.

#include "Components/AGR_EquipmentManager.h"
#include "BlueprintGameplayTagLibrary.h"
#include "Components/AGR_ItemComponent.h"
#include "Data/AGRLibrary.h"
#include "Data/AGRTypes.h"
#include "Net/UnrealNetwork.h"

UAGR_EquipmentManager::UAGR_EquipmentManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
	SetAutoActivate(true);
}

void UAGR_EquipmentManager::BeginPlay()
{
	Super::BeginPlay();
}

void UAGR_EquipmentManager::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAGR_EquipmentManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, EquipmentList);
}

bool UAGR_EquipmentManager::GetAllItems(TArray<AActor*>& OutItems)
{
	OutItems.Reset(EquipmentList.Num());

	for(const FEquipment& EquipmentItem : EquipmentList)
	{
		if(IsValid(EquipmentItem.ItemActor))
		{
			OutItems.Add(EquipmentItem.ItemActor);
		}
	}

	return OutItems.Num() > 0;
}

bool UAGR_EquipmentManager::EquipItemInSlot(const FName Slot, AActor* ItemActor, AActor*& OutPreviousItem, AActor*& OutNewItem)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return false;
	}

	AActor* ItemInSlot;
	if(GetItemInSlot(Slot, ItemInSlot))
	{
		if(ItemInSlot == ItemActor)
		{
			// Same item is already equipped
			return false;
		}
	}
	
	for(int32 i = 0; i < EquipmentList.Num(); ++i)
	{
		const FEquipment& EquipmentElement = EquipmentList[i];

		if(Slot != EquipmentElement.Id)
		{
			continue;
		}

		if(!ItemActor->ActorHasTag(UAGR_ItemComponent::TAG_ITEM))
		{
			continue;
		}

		UAGR_ItemComponent* ItemComponent = UAGRLibrary::GetItemComponent(ItemActor);
		if(!IsValid(ItemComponent))
		{
			continue;
		}

		const bool bMatches = UBlueprintGameplayTagLibrary::MatchesAnyTags(
			ItemComponent->ItemTagSlotType,
			EquipmentElement.AcceptableSlots,
			false);
		if(!bMatches)
		{
			continue;
		}

		// All conditions are met -> Start to equip item

		// Skip unequip if item is NOT valid (empty slot!)
		if(IsValid(EquipmentElement.ItemActor))
		{
			OutPreviousItem = EquipmentElement.ItemActor;
			UAGR_ItemComponent* PreviousItemComponent = UAGRLibrary::GetItemComponent(OutPreviousItem);
			if(IsValid(PreviousItemComponent))
			{
				PreviousItemComponent->UnequipInternal();
			}
		}

		EquipmentList[i].ItemActor = ItemActor;
		OutNewItem = ItemActor;

		ItemComponent->EquipInternal();

		// Item equipped successfully
		return true;
	}

	// Failed to equip item
	return false;
}

void UAGR_EquipmentManager::SetupDefineSlots(const TArray<FEquipment> InEquipmentList)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return;
	}

	EquipmentList = InEquipmentList;
}

bool UAGR_EquipmentManager::UnequipItemFromSlot(const FName Slot, AActor*& OutItemUnequipped)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return false;
	}

	for(int32 i = 0; i < EquipmentList.Num(); ++i)
	{
		FEquipment& EquipmentElement = EquipmentList[i];

		if(Slot != EquipmentElement.Id)
		{
			continue;
		}

		if(!IsValid(EquipmentElement.ItemActor))
		{
			continue;
		}

		/* slot exists + item is an item + tag is acceptable in this slot */

		OutItemUnequipped = EquipmentElement.ItemActor;
		EquipmentElement.ItemActor = nullptr;

		UAGR_ItemComponent* UnequippedItemComponent = UAGRLibrary::GetItemComponent(OutItemUnequipped);
		if(IsValid(UnequippedItemComponent))
		{
			UnequippedItemComponent->UnequipInternal();
		}

		// Item unequipped successfully
		return true;
	}

	// Failed to unequip item
	return false;
}

bool UAGR_EquipmentManager::UnequipByReference(AActor* ItemActor, FText& OutNote)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if(!IsValid(ItemActor))
	{
		// TODO OutNote should be an enum!
		OutNote = FText::FromString("Not a valid actor");
		return false;
	}

	/* Remove from array */

	for(int32 i = 0; i < EquipmentList.Num(); ++i)
	{
		FEquipment& EquipmentElement = EquipmentList[i];

		if(EquipmentElement.ItemActor != ItemActor)
		{
			continue;
		}

		EquipmentElement.ItemActor = nullptr;

		UAGR_ItemComponent* UnequippedItemComponent = UAGRLibrary::GetItemComponent(ItemActor);
		if(IsValid(UnequippedItemComponent))
		{
			UnequippedItemComponent->UnequipInternal();
		}

		// Unequipped successfully
		OutNote = FText::FromString("Successfully unequiped");
		return true;
	}

	// Failed to unequip
	// TODO OutNote should be an enum!
	OutNote = FText::FromString("Not in equipment");
	return false;
}

bool UAGR_EquipmentManager::GetItemInSlot(const FName Slot, AActor*& OutItem)
{
	for(const FEquipment& EquimentElement : EquipmentList)
	{
		if(EquimentElement.Id != Slot)
		{
			continue;
		}

		// Found matching slot ID

		if(!IsValid(EquimentElement.ItemActor))
		{
			// Failed to find item that is also valid
			return false;
		}

		OutItem = EquimentElement.ItemActor;

		// Valid item found
		return true;
	}

	// Failed to find item
	return false;
}

void UAGR_EquipmentManager::SaveShortcutReference(const FName Key, AActor* Item)
{
	References.Add(Key, Item);
}

bool UAGR_EquipmentManager::GetShortcutReference(const FName Key, AActor*& OutActor)
{
	AActor** FoundActor = References.Find(Key);
	if(FoundActor == nullptr)
	{
		// Failed to get the reference: Key not found
		return false;
	}

	if(!IsValid(*FoundActor))
	{
		// Failed to get the reference: Key found but value invalid
		return false;
	}

	OutActor = *FoundActor;

	// Found actor reference
	return true;
}

