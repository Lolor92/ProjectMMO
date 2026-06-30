#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "PMMO_PlayerState.generated.h"

class UAttributeSet;
class UAbilitySystemComponent;

UCLASS()
class PROJECTMMO_API APMMO_PlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	APMMO_PlayerState();
			
	// Player-owned GAS state.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Ability")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadWrite, Category="Ability")
	TObjectPtr<UAttributeSet> AttributeSet;
};
