#include "PlayerState/PMMO_PlayerState.h"
#include "GAS/Attribute/PMMO_AttributeSet.h"
#include "GAS/Component/PMMO_AbilitySystemComponent.h"

APMMO_PlayerState::APMMO_PlayerState()
{
	SetNetUpdateFrequency(100.f);

	AbilitySystemComponent = CreateDefaultSubobject<UPMMO_AbilitySystemComponent>("PMMO_AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UPMMO_AttributeSet>("AttributeSet");
}
