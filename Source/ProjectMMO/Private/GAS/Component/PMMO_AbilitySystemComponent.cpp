#include "GAS/Component/PMMO_AbilitySystemComponent.h"


UPMMO_AbilitySystemComponent::UPMMO_AbilitySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPMMO_AbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
	GrantAbilitySets();
}

void UPMMO_AbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveGrantedAbilitySets();
	Super::EndPlay(EndPlayReason);
}

void UPMMO_AbilitySystemComponent::GrantAbilitySets()
{
	if (bAbilitySetsGranted) return;
	if (!IsOwnerActorAuthoritative()) return;
	
	for (const UPMMO_AbilitySet* AbilitySet : AbilitySetsToGrant)
	{
		if (!AbilitySet) continue;
		
		AbilitySet->GiveToAbilitySystem(this, &GrantedHandles, GetOwner());
	}
	
	bAbilitySetsGranted = true;
}

void UPMMO_AbilitySystemComponent::RemoveGrantedAbilitySets()
{
	if (!bAbilitySetsGranted) return;
	if (!IsOwnerActorAuthoritative()) return;
	
	GrantedHandles.ClearAbilities(this);
}
