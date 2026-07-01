#include "Components/PMMO_CharacterMovementComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/PMMO_BaseCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogPMMOCharacterMovement, Log, All);

static TAutoConsoleVariable<int32> CVarPMMORootMotionContactLog(
	TEXT("p.PMMO.RootMotionContactLog"),
	0,
	TEXT("Enable verbose root motion contact stop logging."),
	ECVF_Default);

namespace
{
const TCHAR* PMMONetModeToString(const UWorld* World)
{
	if (!World)
	{
		return TEXT("NoWorld");
	}

	switch (World->GetNetMode())
	{
	case NM_Standalone: return TEXT("Standalone");
	case NM_DedicatedServer: return TEXT("DedicatedServer");
	case NM_ListenServer: return TEXT("ListenServer");
	case NM_Client: return TEXT("Client");
	default: return TEXT("Unknown");
	}
}

bool PMMOShouldLogRootMotionContact()
{
	return CVarPMMORootMotionContactLog.GetValueOnGameThread() != 0;
}
}

#define PMMO_ROOT_MOTION_CONTACT_LOG(Format, ...) \
	UE_CLOG(PMMOShouldLogRootMotionContact(), LogPMMOCharacterMovement, Warning, Format, ##__VA_ARGS__)

class FSavedMove_PMMOCharacter final : public FSavedMove_Character
{
public:
};

class FNetworkPredictionData_Client_PMMOCharacter final : public FNetworkPredictionData_Client_Character
{
public:
	explicit FNetworkPredictionData_Client_PMMOCharacter(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Client_Character(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_PMMOCharacter());
	}
};

void UPMMO_CharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	BindOwnerCapsuleHit();
}

void UPMMO_CharacterMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindOwnerCapsuleHit();
	ClearRootMotionContactBlock();

	Super::EndPlay(EndPlayReason);
}

void UPMMO_CharacterMovementComponent::ConfigureRootMotionContactStop(
	bool bEnabled,
	float StopAngleDegrees,
	float ReleasePercent,
	float CapsuleTolerance,
	UObject* SourceObject)
{
	const UObject* PreviousSource = RootMotionContactStopSource.Get();
	if (bEnabled && PreviousSource && SourceObject && PreviousSource != SourceObject && bRootMotionContactBlocked)
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact CONFIG_SOURCE_CHANGED_CLEAR Owner=%s PreviousSource=%s NewSource=%s"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(PreviousSource),
			*GetNameSafe(SourceObject));
		ClearRootMotionContactBlock();
	}

	bRootMotionContactStopEnabled = bEnabled;
	RootMotionContactStopAngleDegrees = FMath::Clamp(StopAngleDegrees, 0.0f, 180.0f);
	RootMotionContactReleasePercent = FMath::Clamp(ReleasePercent, 0.0f, 100.0f);
	RootMotionContactCapsuleTolerance = FMath::Max(0.0f, CapsuleTolerance);
	RootMotionContactStopSource = bEnabled ? SourceObject : nullptr;

	PMMO_ROOT_MOTION_CONTACT_LOG(
		TEXT("RootMotionContact CONFIG Owner=%s Source=%s Enabled=%d Angle=%.1f ReleasePercent=%.1f CapsuleTolerance=%.1f NetMode=%s Role=%d Local=%d Authority=%d"),
		*GetNameSafe(CharacterOwner),
		*GetNameSafe(SourceObject),
		bRootMotionContactStopEnabled ? 1 : 0,
		RootMotionContactStopAngleDegrees,
		RootMotionContactReleasePercent,
		RootMotionContactCapsuleTolerance,
		PMMONetModeToString(GetWorld()),
		CharacterOwner ? static_cast<int32>(CharacterOwner->GetLocalRole()) : -1,
		CharacterOwner ? CharacterOwner->IsLocallyControlled() : false,
		CharacterOwner ? CharacterOwner->HasAuthority() : false);

	if (!bRootMotionContactStopEnabled)
	{
		ClearRootMotionContactBlock();
	}
}

void UPMMO_CharacterMovementComponent::ClearRootMotionContactStop(UObject* SourceObject)
{
	UObject* CurrentSource = RootMotionContactStopSource.Get();
	if (CurrentSource && SourceObject && CurrentSource != SourceObject)
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact CONFIG_CLEAR_IGNORED Owner=%s RequestSource=%s CurrentSource=%s"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(SourceObject),
			*GetNameSafe(CurrentSource));
		return;
	}

	bRootMotionContactStopEnabled = false;
	RootMotionContactStopSource = nullptr;
	ClearRootMotionContactBlock();

	PMMO_ROOT_MOTION_CONTACT_LOG(
		TEXT("RootMotionContact CONFIG_CLEAR Owner=%s Source=%s"),
		*GetNameSafe(CharacterOwner),
		*GetNameSafe(SourceObject));
}

void UPMMO_CharacterMovementComponent::SetRootMotionContactBlock(
	bool bBlocked,
	const FVector& InBlockedDirection,
	float Duration)
{
	const bool bWasBlocked = bRootMotionContactBlocked;
	const FVector PreviousDirection = RootMotionContactBlockedDirection;
	const FVector NewDirection = bBlocked ? InBlockedDirection.GetSafeNormal2D() : FVector::ZeroVector;
	UAnimMontage* NewMontage = bBlocked ? GetRootMotionContactMontage() : nullptr;

	if (bBlocked && bRootMotionContactBlocked &&
		RootMotionContactBlockedDirection.Equals(NewDirection, UE_KINDA_SMALL_NUMBER) &&
		RootMotionContactMontage.Get() == NewMontage)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RootMotionContactBlockTimerHandle);
	}

	bRootMotionContactBlocked = bBlocked;
	RootMotionContactBlockedDirection = NewDirection;
	RootMotionContactMontage = NewMontage;

	if (bBlocked && Duration > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RootMotionContactBlockTimerHandle,
				this,
				&ThisClass::ClearRootMotionContactBlock,
				Duration,
				false);
		}
	}

	if (bBlocked || bWasBlocked || !PreviousDirection.Equals(RootMotionContactBlockedDirection))
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact BLOCK_SET Owner=%s Blocked=%d WasBlocked=%d Direction=%s Duration=%.3f Montage=%s NetMode=%s Role=%d Local=%d Authority=%d"),
			*GetNameSafe(CharacterOwner),
			bRootMotionContactBlocked ? 1 : 0,
			bWasBlocked ? 1 : 0,
			*RootMotionContactBlockedDirection.ToString(),
			Duration,
			*GetNameSafe(RootMotionContactMontage.Get()),
			PMMONetModeToString(GetWorld()),
			CharacterOwner ? static_cast<int32>(CharacterOwner->GetLocalRole()) : -1,
			CharacterOwner ? CharacterOwner->IsLocallyControlled() : false,
			CharacterOwner ? CharacterOwner->HasAuthority() : false);
	}
}

void UPMMO_CharacterMovementComponent::ClearRootMotionContactBlock()
{
	const bool bWasBlocked = bRootMotionContactBlocked;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RootMotionContactBlockTimerHandle);
	}

	bRootMotionContactBlocked = false;
	RootMotionContactBlockedDirection = FVector::ZeroVector;
	RootMotionContactMontage = nullptr;

	if (bWasBlocked)
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact BLOCK_CLEAR Owner=%s NetMode=%s Role=%d Local=%d Authority=%d"),
			*GetNameSafe(CharacterOwner),
			PMMONetModeToString(GetWorld()),
			CharacterOwner ? static_cast<int32>(CharacterOwner->GetLocalRole()) : -1,
			CharacterOwner ? CharacterOwner->IsLocallyControlled() : false,
			CharacterOwner ? CharacterOwner->HasAuthority() : false);
	}
}

void UPMMO_CharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
}

FNetworkPredictionData_Client* UPMMO_CharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);

	if (ClientPredictionData == nullptr)
	{
		UPMMO_CharacterMovementComponent* MutableThis = const_cast<UPMMO_CharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_PMMOCharacter(*this);
	}

	return ClientPredictionData;
}

void UPMMO_CharacterMovementComponent::ApplyRootMotionToVelocity(float DeltaTime)
{
	const bool bHadRootMotionVelocity =
		HasAnimRootMotion() ||
		CurrentRootMotion.HasOverrideVelocity() ||
		CurrentRootMotion.HasAdditiveVelocity();

	Super::ApplyRootMotionToVelocity(DeltaTime);

	if (bRootMotionContactBlocked && HasReachedRootMotionContactReleasePercent())
	{
		ClearRootMotionContactBlock();
	}

	if (bHadRootMotionVelocity && !bRootMotionContactBlocked)
	{
		TryStartRootMotionContactBlockFromCapsuleProbe(DeltaTime, Velocity);
	}

	if (bHadRootMotionVelocity)
	{
		Velocity = RemoveVelocityIntoRootMotionContactBlock(Velocity);
	}
}

FVector UPMMO_CharacterMovementComponent::CalcAnimRootMotionVelocity(
	const FVector& RootMotionDeltaMove,
	float DeltaSeconds,
	const FVector& CurrentVelocity) const
{
	FVector RootMotionVelocity = Super::CalcAnimRootMotionVelocity(
		RootMotionDeltaMove,
		DeltaSeconds,
		CurrentVelocity);

	return RemoveVelocityIntoRootMotionContactBlock(RootMotionVelocity);
}

void UPMMO_CharacterMovementComponent::HandleOwnerCapsuleHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	PMMO_ROOT_MOTION_CONTACT_LOG(
		TEXT("RootMotionContact HIT Owner=%s Other=%s OtherComp=%s Enabled=%d Blocked=%d HitLoc=%s Impact=%s StartPen=%d Blocking=%d NetMode=%s Role=%d Local=%d Authority=%d"),
		*GetNameSafe(CharacterOwner),
		*GetNameSafe(OtherActor),
		*GetNameSafe(OtherComp),
		bRootMotionContactStopEnabled ? 1 : 0,
		bRootMotionContactBlocked ? 1 : 0,
		*Hit.Location.ToCompactString(),
		*Hit.ImpactPoint.ToCompactString(),
		Hit.bStartPenetrating ? 1 : 0,
		Hit.bBlockingHit ? 1 : 0,
		PMMONetModeToString(GetWorld()),
		CharacterOwner ? static_cast<int32>(CharacterOwner->GetLocalRole()) : -1,
		CharacterOwner ? CharacterOwner->IsLocallyControlled() : false,
		CharacterOwner ? CharacterOwner->HasAuthority() : false);

	if (!CharacterOwner || !ShouldBlockRootMotionForCapsuleHit(OtherActor, OtherComp, Hit))
	{
		return;
	}

	FVector OtherCapsuleLocation = FVector::ZeroVector;
	if (!GetRootMotionContactOtherCapsuleLocation(OtherActor, OtherCapsuleLocation))
	{
		return;
	}

	FVector OwnerCapsuleLocation = FVector::ZeroVector;
	if (!GetRootMotionContactOwnerCapsuleLocation(OwnerCapsuleLocation))
	{
		return;
	}

	FVector BlockedDirection = OtherCapsuleLocation - OwnerCapsuleLocation;

	BlockedDirection.Z = 0.0f;
	if (BlockedDirection.IsNearlyZero())
	{
		BlockedDirection = -Hit.ImpactNormal;
		BlockedDirection.Z = 0.0f;
	}

	const float BlockDuration = RootMotionContactReleasePercent < 100.0f
		? 0.0f
		: RootMotionContactBlockDuration;

	PMMO_ROOT_MOTION_CONTACT_LOG(
		TEXT("RootMotionContact HIT_ACCEPT Owner=%s Other=%s ServerOtherCapsule=%s BlockDirection=%s Duration=%.3f"),
		*GetNameSafe(CharacterOwner),
		*GetNameSafe(OtherActor),
		*OtherCapsuleLocation.ToCompactString(),
		*BlockedDirection.ToCompactString(),
		BlockDuration);

	SetRootMotionContactBlock(true, BlockedDirection, BlockDuration);
}

void UPMMO_CharacterMovementComponent::BindOwnerCapsuleHit()
{
	if (!CharacterOwner)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent())
	{
		Capsule->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleOwnerCapsuleHit);
	}
}

void UPMMO_CharacterMovementComponent::UnbindOwnerCapsuleHit()
{
	if (!CharacterOwner)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent())
	{
		Capsule->OnComponentHit.RemoveDynamic(this, &ThisClass::HandleOwnerCapsuleHit);
	}
}

bool UPMMO_CharacterMovementComponent::ShouldBlockRootMotionForCapsuleHit(
	const AActor* OtherActor,
	const UPrimitiveComponent* OtherComp,
	const FHitResult& Hit) const
{
	if (!bRootMotionContactStopEnabled || !CharacterOwner || !OtherActor || OtherActor == CharacterOwner || !OtherComp)
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact REJECT basic Enabled=%d Owner=%s Other=%s OtherComp=%s SameActor=%d"),
			bRootMotionContactStopEnabled ? 1 : 0,
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(OtherActor),
			*GetNameSafe(OtherComp),
			OtherActor && OtherActor == CharacterOwner ? 1 : 0);
		return false;
	}

	if (!Cast<ACharacter>(OtherActor))
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact REJECT other-not-character Owner=%s Other=%s Class=%s"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(OtherActor),
			*GetNameSafe(OtherActor->GetClass()));
		return false;
	}

	if (!OtherComp->IsA<UCapsuleComponent>())
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact REJECT other-comp-not-capsule Owner=%s Other=%s OtherComp=%s Class=%s"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(OtherActor),
			*GetNameSafe(OtherComp),
			*GetNameSafe(OtherComp->GetClass()));
		return false;
	}

	FVector OwnerCapsuleLocation = FVector::ZeroVector;
	if (!GetRootMotionContactOwnerCapsuleLocation(OwnerCapsuleLocation))
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact REJECT no-owner-server-capsule Owner=%s"),
			*GetNameSafe(CharacterOwner));
		return false;
	}

	FVector OtherCapsuleLocation = FVector::ZeroVector;
	if (!GetRootMotionContactOtherCapsuleLocation(OtherActor, OtherCapsuleLocation))
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact REJECT no-other-server-capsule Owner=%s Other=%s DebugLocUnavailable"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(OtherActor));
		return false;
	}

	const UCapsuleComponent* OwnerCapsule = CharacterOwner->GetCapsuleComponent();
	const UCapsuleComponent* OtherCapsule = Cast<UCapsuleComponent>(OtherComp);
	if (!OwnerCapsule || !OtherCapsule)
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact REJECT missing-capsule Owner=%s OwnerCapsule=%s Other=%s OtherCapsule=%s"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(OwnerCapsule),
			*GetNameSafe(OtherActor),
			*GetNameSafe(OtherCapsule));
		return false;
	}

	float HorizontalDistance = 0.0f;
	float MaxHorizontalDistance = 0.0f;
	float VerticalDistance = 0.0f;
	float MaxVerticalDistance = 0.0f;
	if (!AreRootMotionContactCapsulesTouching(OwnerCapsule, OtherCapsule, OwnerCapsuleLocation, OtherCapsuleLocation,
		HorizontalDistance, MaxHorizontalDistance, VerticalDistance, MaxVerticalDistance))
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact REJECT server-capsules-not-touching-horizontal Owner=%s Other=%s OwnerCheckCapsule=%s OtherServerCapsule=%s DistXY=%.2f StrictMaxXY=%.2f ToleratedMaxXY=%.2f Tolerance=%.2f"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(OtherActor),
			*OwnerCapsuleLocation.ToCompactString(),
			*OtherCapsuleLocation.ToCompactString(),
			HorizontalDistance,
			MaxHorizontalDistance - RootMotionContactCapsuleTolerance,
			MaxHorizontalDistance,
			RootMotionContactCapsuleTolerance);
		return false;
	}

	float ForwardDot = 0.0f;
	float MinForwardDot = 0.0f;
	const bool bPassAngle = IsRootMotionContactWithinAngle(OwnerCapsuleLocation, OtherCapsuleLocation,
		ForwardDot, MinForwardDot);
	PMMO_ROOT_MOTION_CONTACT_LOG(
		TEXT("RootMotionContact VALIDATE Owner=%s Other=%s Result=%s OwnerCheckCapsule=%s OtherServerCapsule=%s DistXY=%.2f StrictMaxXY=%.2f ToleratedMaxXY=%.2f Tolerance=%.2f DistZ=%.2f MaxZ=%.2f ForwardDot=%.3f MinDot=%.3f AngleSetting=%.1f"),
		*GetNameSafe(CharacterOwner),
		*GetNameSafe(OtherActor),
		bPassAngle ? TEXT("PASS") : TEXT("REJECT_ANGLE"),
		*OwnerCapsuleLocation.ToCompactString(),
		*OtherCapsuleLocation.ToCompactString(),
		HorizontalDistance,
		MaxHorizontalDistance - RootMotionContactCapsuleTolerance,
		MaxHorizontalDistance,
		RootMotionContactCapsuleTolerance,
		VerticalDistance,
		MaxVerticalDistance,
		ForwardDot,
		MinForwardDot,
		RootMotionContactStopAngleDegrees);

	return bPassAngle;
}

bool UPMMO_CharacterMovementComponent::TryStartRootMotionContactBlockFromCapsuleProbe(
	float DeltaTime,
	const FVector& InVelocity)
{
	if (!bRootMotionContactStopEnabled || !CharacterOwner || !GetWorld())
	{
		return false;
	}

	const UCapsuleComponent* OwnerCapsule = CharacterOwner->GetCapsuleComponent();
	if (!OwnerCapsule)
	{
		return false;
	}

	FVector OwnerCapsuleLocation = FVector::ZeroVector;
	if (!GetRootMotionContactOwnerCapsuleLocation(OwnerCapsuleLocation))
	{
		return false;
	}

	const FVector PredictedOwnerLocation =
		OwnerCapsuleLocation + FVector(InVelocity.X, InVelocity.Y, 0.0f) * FMath::Max(DeltaTime, 0.0f);

	for (TActorIterator<APMMO_BaseCharacter> It(GetWorld()); It; ++It)
	{
		APMMO_BaseCharacter* OtherCharacter = *It;
		if (!OtherCharacter || OtherCharacter == CharacterOwner)
		{
			continue;
		}

		const UCapsuleComponent* OtherCapsule = OtherCharacter->GetCapsuleComponent();
		if (!OtherCapsule)
		{
			continue;
		}

		FVector OtherCapsuleLocation = FVector::ZeroVector;
		if (!GetRootMotionContactOtherCapsuleLocation(OtherCharacter, OtherCapsuleLocation))
		{
			continue;
		}

		float HorizontalDistance = 0.0f;
		float MaxHorizontalDistance = 0.0f;
		float VerticalDistance = 0.0f;
		float MaxVerticalDistance = 0.0f;
		if (!AreRootMotionContactCapsulesTouching(OwnerCapsule, OtherCapsule, PredictedOwnerLocation,
			OtherCapsuleLocation, HorizontalDistance, MaxHorizontalDistance, VerticalDistance, MaxVerticalDistance))
		{
			continue;
		}

		float ForwardDot = 0.0f;
		float MinForwardDot = 0.0f;
		if (!IsRootMotionContactWithinAngle(PredictedOwnerLocation, OtherCapsuleLocation,
			ForwardDot, MinForwardDot))
		{
			continue;
		}

		FVector BlockedDirection = OtherCapsuleLocation - PredictedOwnerLocation;
		BlockedDirection.Z = 0.0f;
		if (BlockedDirection.IsNearlyZero())
		{
			continue;
		}

		const float BlockDuration = RootMotionContactReleasePercent < 100.0f
			? 0.0f
			: RootMotionContactBlockDuration;

		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact PROBE_ACCEPT Owner=%s Other=%s OwnerPredictedCapsule=%s OtherServerCapsule=%s InVelocity=%s DeltaTime=%.4f DistXY=%.2f MaxXY=%.2f ForwardDot=%.3f MinDot=%.3f"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(OtherCharacter),
			*PredictedOwnerLocation.ToCompactString(),
			*OtherCapsuleLocation.ToCompactString(),
			*InVelocity.ToCompactString(),
			DeltaTime,
			HorizontalDistance,
			MaxHorizontalDistance,
			ForwardDot,
			MinForwardDot);

		SetRootMotionContactBlock(true, BlockedDirection, BlockDuration);
		return true;
	}

	return false;
}

bool UPMMO_CharacterMovementComponent::AreRootMotionContactCapsulesTouching(
	const UCapsuleComponent* OwnerCapsule,
	const UCapsuleComponent* OtherCapsule,
	const FVector& OwnerLocation,
	const FVector& OtherLocation,
	float& OutHorizontalDistance,
	float& OutMaxHorizontalDistance,
	float& OutVerticalDistance,
	float& OutMaxVerticalDistance) const
{
	if (!OwnerCapsule || !OtherCapsule)
	{
		return false;
	}

	OutMaxHorizontalDistance =
		OwnerCapsule->GetScaledCapsuleRadius() +
		OtherCapsule->GetScaledCapsuleRadius() +
		RootMotionContactCapsuleTolerance;
	OutHorizontalDistance = FVector::Dist2D(OwnerLocation, OtherLocation);

	OutMaxVerticalDistance =
		OwnerCapsule->GetScaledCapsuleHalfHeight() +
		OtherCapsule->GetScaledCapsuleHalfHeight();
	OutVerticalDistance = FMath::Abs(OwnerLocation.Z - OtherLocation.Z);

	return OutHorizontalDistance <= OutMaxHorizontalDistance &&
		OutVerticalDistance <= OutMaxVerticalDistance;
}

bool UPMMO_CharacterMovementComponent::IsRootMotionContactWithinAngle(
	const FVector& OwnerLocation,
	const FVector& OtherLocation,
	float& OutForwardDot,
	float& OutMinForwardDot) const
{
	FVector Forward = CharacterOwner ? CharacterOwner->GetActorForwardVector() : FVector::ZeroVector;
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		return false;
	}

	FVector ToHit = OtherLocation - OwnerLocation;
	ToHit.Z = 0.0f;
	if (!ToHit.Normalize())
	{
		return false;
	}

	OutMinForwardDot = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(RootMotionContactStopAngleDegrees, 0.0f, 180.0f)));
	OutForwardDot = FVector::DotProduct(Forward, ToHit);

	return OutForwardDot >= OutMinForwardDot;
}

bool UPMMO_CharacterMovementComponent::GetRootMotionContactOwnerCapsuleLocation(FVector& OutLocation) const
{
	if (!CharacterOwner)
	{
		return false;
	}

	const UCapsuleComponent* OwnerCapsule = CharacterOwner->GetCapsuleComponent();

	if (CharacterOwner->HasAuthority() || CharacterOwner->IsLocallyControlled())
	{
		OutLocation = OwnerCapsule ? OwnerCapsule->GetComponentLocation() : CharacterOwner->GetActorLocation();
		return true;
	}

	if (const APMMO_BaseCharacter* OwnerPMMOCharacter = Cast<APMMO_BaseCharacter>(CharacterOwner))
	{
		OutLocation = OwnerPMMOCharacter->GetDebugServerCapsuleLocation();
		return !OutLocation.IsNearlyZero();
	}

	return false;
}

bool UPMMO_CharacterMovementComponent::GetRootMotionContactOtherCapsuleLocation(
	const AActor* OtherActor,
	FVector& OutLocation) const
{
	if (!OtherActor)
	{
		return false;
	}

	if (CharacterOwner && CharacterOwner->HasAuthority())
	{
		const ACharacter* OtherCharacter = Cast<ACharacter>(OtherActor);
		const UCapsuleComponent* OtherCapsule = OtherCharacter ? OtherCharacter->GetCapsuleComponent() : nullptr;
		OutLocation = OtherCapsule ? OtherCapsule->GetComponentLocation() : OtherActor->GetActorLocation();
		return true;
	}

	if (const APMMO_BaseCharacter* OtherPMMOCharacter = Cast<APMMO_BaseCharacter>(OtherActor))
	{
		OutLocation = OtherPMMOCharacter->GetDebugServerCapsuleLocation();
		return !OutLocation.IsNearlyZero();
	}

	return false;
}

bool UPMMO_CharacterMovementComponent::HasReachedRootMotionContactReleasePercent() const
{
	if (RootMotionContactReleasePercent >= 100.0f)
	{
		return false;
	}

	const UAnimMontage* Montage = RootMotionContactMontage.Get();
	if (!Montage)
	{
		Montage = GetRootMotionContactMontage();
	}

	const USkeletalMeshComponent* MeshComp = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	const float MontageLength = Montage ? Montage->GetPlayLength() : 0.0f;
	if (!AnimInstance || !Montage || MontageLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float MontagePercent = (AnimInstance->Montage_GetPosition(Montage) / MontageLength) * 100.0f;
	const bool bReachedRelease = MontagePercent >= RootMotionContactReleasePercent;
	if (bReachedRelease)
	{
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact RELEASE_PERCENT Owner=%s Montage=%s Percent=%.2f ReleasePercent=%.2f"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(Montage),
			MontagePercent,
			RootMotionContactReleasePercent);
	}

	return bReachedRelease;
}

UAnimMontage* UPMMO_CharacterMovementComponent::GetRootMotionContactMontage() const
{
	const USkeletalMeshComponent* MeshComp = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	return AnimInstance ? AnimInstance->GetCurrentActiveMontage() : nullptr;
}

FVector UPMMO_CharacterMovementComponent::RemoveVelocityIntoRootMotionContactBlock(const FVector& InVelocity) const
{
	if (!bRootMotionContactBlocked)
	{
		return InVelocity;
	}

	if (HasReachedRootMotionContactReleasePercent())
	{
		return InVelocity;
	}

	FVector OutVelocity = InVelocity;
	OutVelocity.X = 0.0f;
	OutVelocity.Y = 0.0f;

	const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	if (CurrentTime < 0.0 || CurrentTime - LastRootMotionContactClampLogTime >= 0.1)
	{
		LastRootMotionContactClampLogTime = CurrentTime;
		PMMO_ROOT_MOTION_CONTACT_LOG(
			TEXT("RootMotionContact CLAMP Owner=%s InVelocity=%s OutVelocity=%s BlockDirection=%s NetMode=%s Role=%d Local=%d Authority=%d"),
			*GetNameSafe(CharacterOwner),
			*InVelocity.ToCompactString(),
			*OutVelocity.ToCompactString(),
			*RootMotionContactBlockedDirection.ToCompactString(),
			PMMONetModeToString(GetWorld()),
			CharacterOwner ? static_cast<int32>(CharacterOwner->GetLocalRole()) : -1,
			CharacterOwner ? CharacterOwner->IsLocallyControlled() : false,
			CharacterOwner ? CharacterOwner->HasAuthority() : false);
	}

	return OutVelocity;
}
