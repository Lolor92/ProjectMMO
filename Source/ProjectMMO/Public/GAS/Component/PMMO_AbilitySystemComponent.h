#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GAS/Data/PMMO_AbilitySet.h"
#include "PMMO_AbilitySystemComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTMMO_API UPMMO_AbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UPMMO_AbilitySystemComponent();
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(BlueprintCallable)
	void GrantAbilitySets();

	UFUNCTION(BlueprintCallable)
	void RemoveGrantedAbilitySets();
	
protected:
	UPROPERTY(EditDefaultsOnly, Category="Abilities")
	TArray<TObjectPtr<UPMMO_AbilitySet>> AbilitySetsToGrant;
	
private:
	UPROPERTY()
	FPMMO_AbilitySet_GrantedHandles GrantedHandles;

	bool bAbilitySetsGranted = false;
};
