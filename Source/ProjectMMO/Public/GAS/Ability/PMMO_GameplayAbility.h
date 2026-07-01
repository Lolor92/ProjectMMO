#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "PMMO_GameplayAbility.generated.h"


UCLASS()
class PROJECTMMO_API UPMMO_GameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UPMMO_GameplayAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category="Ability|Root Motion")
	bool bStopRootMotionOnCapsuleContact = true;

	UPROPERTY(EditDefaultsOnly, Category="Ability|Root Motion",
		meta=(EditCondition="bStopRootMotionOnCapsuleContact", ClampMin="0.0", ClampMax="180.0", UIMin="0.0", UIMax="180.0", Units="Degrees"))
	float RootMotionContactStopAngleDegrees = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category="Ability|Root Motion",
		meta=(EditCondition="bStopRootMotionOnCapsuleContact", ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0", Units="Percent"))
	float RootMotionContactReleasePercent = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category="Ability|Root Motion",
		meta=(EditCondition="bStopRootMotionOnCapsuleContact", ClampMin="0.0", UIMin="0.0", Units="Centimeters"))
	float RootMotionContactCapsuleTolerance = 5.0f;
};
