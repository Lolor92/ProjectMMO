#include "ProjectMMO/Public/Character/PMMO_BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "Components/PMMO_CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"
#include "PlayerState/PMMO_PlayerState.h"

APMMO_BaseCharacter::APMMO_BaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPMMO_CharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
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

void APMMO_BaseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && GetCapsuleComponent())
	{
		Debug_ServerCapsuleLocation = GetCapsuleComponent()->GetComponentLocation();
	}

	DrawNetworkCapsuleDebug();
}

void APMMO_BaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APMMO_BaseCharacter, Debug_ServerCapsuleLocation);
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

void APMMO_BaseCharacter::DrawNetworkCapsuleDebug() const
{
	if (!GetCapsuleComponent())
	{
		return;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector LocalCapsuleLocation = Capsule->GetComponentLocation();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();

	const bool bAuthority = HasAuthority();
	const bool bLocalPawn = IsLocallyControlled();

	const FColor LocalColor =
		bAuthority ? FColor::Red :
		bLocalPawn ? FColor::Green :
		FColor::Cyan;

	DrawDebugCapsule(
		World,
		LocalCapsuleLocation,
		HalfHeight,
		Radius,
		Capsule->GetComponentQuat(),
		LocalColor,
		false,
		0.0f,
		0,
		2.0f);

	if (!bAuthority)
	{
		DrawDebugCapsule(
			World,
			Debug_ServerCapsuleLocation,
			HalfHeight,
			Radius,
			FQuat::Identity,
			FColor::Red,
			false,
			0.0f,
			0,
			4.0f);

		DrawDebugLine(
			World,
			LocalCapsuleLocation,
			Debug_ServerCapsuleLocation,
			FColor::Yellow,
			false,
			0.0f,
			0,
			2.0f);

		DrawDebugString(
			World,
			LocalCapsuleLocation + FVector(0.0f, 0.0f, HalfHeight + 30.0f),
			FString::Printf(
				TEXT("Local copy: %s\nServer copy: %s\nDelta: %.1f"),
				*LocalCapsuleLocation.ToCompactString(),
				*Debug_ServerCapsuleLocation.ToCompactString(),
				FVector::Dist(LocalCapsuleLocation, Debug_ServerCapsuleLocation)),
			nullptr,
			LocalColor,
			0.0f,
			true);
	}
}
