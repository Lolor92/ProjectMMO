#include "ProjectMMO/Public/Character/PMMO_BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "PlayerState/PMMO_PlayerState.h"

APMMO_BaseCharacter::APMMO_BaseCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	
	// Collision defaults.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetGenerateOverlapEvents(true);
	
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	// Base movement tuning.
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;
	GetCharacterMovement()->MaxCustomMovementSpeed = 400.0f;

	GetCharacterMovement()->MaxJumpApexAttemptsPerSimulation = 1;
	GetCharacterMovement()->JumpZVelocity = 850.f;
	GetCharacterMovement()->AirControl = 0.5f;
	GetCharacterMovement()->GravityScale = 2.5f;
}

UAbilitySystemComponent* APMMO_BaseCharacter::GetAbilitySystemComponent() const
{
	if (AbilitySystemComponent) return AbilitySystemComponent;

	// Fallback for actors that still own an ASC component directly.
	if (UAbilitySystemComponent* CharacterAbilitySystem = FindComponentByClass<UAbilitySystemComponent>())
	{
		return CharacterAbilitySystem;
	}

	APMMO_PlayerState* PMMO_PlayerState = GetPlayerState<APMMO_PlayerState>();
	return PMMO_PlayerState ? PMMO_PlayerState->GetAbilitySystemComponent() : nullptr;
}

UAttributeSet* APMMO_BaseCharacter::GetAttributeSet() const
{
	if (AttributeSet) return AttributeSet;

	const APMMO_PlayerState* PMMO_PlayerState = GetPlayerState<APMMO_PlayerState>();
	return PMMO_PlayerState ? PMMO_PlayerState->GetAttributeSet() : nullptr;
}
