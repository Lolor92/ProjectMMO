#include "GAS/Ability/PMMO_GameplayAbility.h"

#include "Components/PMMO_CharacterMovementComponent.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogPMMOGameplayAbility, Log, All);

UPMMO_GameplayAbility::UPMMO_GameplayAbility()
{
	// Default setup for locally predicted, per-character ability instances.
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy  = EGameplayAbilityNetSecurityPolicy::ClientOrServer;

	bServerRespectsRemoteAbilityCancellation = false;
	bRetriggerInstancedAbility = true;
}

bool UPMMO_GameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UPMMO_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (Character)
	{
		if (UPMMO_CharacterMovementComponent* MoveComp =
			Cast<UPMMO_CharacterMovementComponent>(Character->GetCharacterMovement()))
		{
			MoveComp->ConfigureRootMotionContactStop(
				bStopRootMotionOnCapsuleContact,
				RootMotionContactStopAngleDegrees,
				RootMotionContactReleasePercent,
				RootMotionContactCapsuleTolerance,
				this);

			UE_LOG(LogPMMOGameplayAbility, Log,
				TEXT("RootMotionContact ability configure Ability=%s Avatar=%s Enabled=%d Angle=%.1f ReleasePercent=%.1f CapsuleTolerance=%.1f Authority=%d Local=%d Role=%d"),
				*GetNameSafe(GetClass()),
				*GetNameSafe(Character),
				bStopRootMotionOnCapsuleContact ? 1 : 0,
				RootMotionContactStopAngleDegrees,
				RootMotionContactReleasePercent,
				RootMotionContactCapsuleTolerance,
				ActorInfo && ActorInfo->IsNetAuthority(),
				ActorInfo && ActorInfo->IsLocallyControlled(),
				static_cast<int32>(Character->GetLocalRole()));
		}
		else
		{
			UE_LOG(LogPMMOGameplayAbility, Warning,
				TEXT("RootMotionContact ability configure failed: Avatar=%s has movement component %s, not PMMO CMC"),
				*GetNameSafe(Character),
				*GetNameSafe(Character->GetCharacterMovement()));
		}
	}
	else
	{
		UE_LOG(LogPMMOGameplayAbility, Warning,
			TEXT("RootMotionContact ability configure failed: no character avatar Ability=%s"),
			*GetNameSafe(GetClass()));
	}
}

void UPMMO_GameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		if (UPMMO_CharacterMovementComponent* MoveComp =
			Cast<UPMMO_CharacterMovementComponent>(Character->GetCharacterMovement()))
		{
			MoveComp->ClearRootMotionContactStop(this);

			UE_LOG(LogPMMOGameplayAbility, Log,
				TEXT("RootMotionContact ability end Ability=%s Avatar=%s Authority=%d Local=%d Role=%d"),
				*GetNameSafe(GetClass()),
				*GetNameSafe(Character),
				ActorInfo && ActorInfo->IsNetAuthority(),
				ActorInfo && ActorInfo->IsLocallyControlled(),
				static_cast<int32>(Character->GetLocalRole()));
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
