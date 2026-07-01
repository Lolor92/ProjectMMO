#include "Components/PMMO_PredictionComponent.h"

#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"


UPMMO_PredictionComponent::UPMMO_PredictionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UPMMO_PredictionComponent::PlayPredictedReactionOnTargetProxy(AActor* TargetActor, FGameplayTag ReactionTag)
{
	if (!ReactionData || !ReactionTag.IsValid()) return false;
	
	FPMMO_ReactionDataEntry Reaction;
	if (!ReactionData->FindReaction(ReactionTag, Reaction)) return false;
	
	if (!CanPlayPredictedReactionOnTargetProxy(TargetActor, Reaction)) return false;
	
	const FPMMO_ReactionPredictionContext Context = MakeReactionPredictionContext();
	
	AddPendingPredictedReaction(Context, TargetActor, ReactionTag);
	
	const float StartPosition = GetReactionStartPosition(Reaction);
	
	const bool bPlayed = PlayReactionMontageOnActor(TargetActor, Reaction, StartPosition, true);
	
	if (!bPlayed)
	{
		ConsumePendingPredictedReaction(Context, TargetActor, ReactionTag);
		return false;
	}

	const float MontageLength = Reaction.Montage ? Reaction.Montage->GetPlayLength() : 0.f;
	const float PlayRate = FMath::Max(FMath::Abs(Reaction.PlayRate), KINDA_SMALL_NUMBER);
	const float IgnoreDuration = FMath::Max(0.05f, (MontageLength - StartPosition) / PlayRate);
	AddTemporaryPredictedTargetMovementIgnore(TargetActor, IgnoreDuration);

	ServerConfirmPredictedReaction(Context, TargetActor, ReactionTag);

	return true;
}

void UPMMO_PredictionComponent::ServerConfirmPredictedReaction_Implementation(FPMMO_ReactionPredictionContext Context,
	AActor* TargetActor, FGameplayTag ReactionTag)
{
	AActor* OwnerActor = GetOwner();

	if (!OwnerActor || !OwnerActor->HasAuthority()) return;

	if (!Context.IsValid() || !TargetActor || !ReactionData || !ReactionTag.IsValid()) return;

	FPMMO_ReactionDataEntry Reaction;
	if (!ReactionData->FindReaction(ReactionTag, Reaction)) return;

	const float StartPosition = GetReactionStartPosition(Reaction);

	PlayReactionMontageOnActor(TargetActor, Reaction, StartPosition, true);

	if (UWorld* World = GetWorld())
	{
		const float MontageLength = Reaction.Montage ? Reaction.Montage->GetPlayLength() : 0.f;
		const float PlayRate = FMath::Max(FMath::Abs(Reaction.PlayRate), KINDA_SMALL_NUMBER);
		const float RemainingDuration = FMath::Max(0.05f, (MontageLength - StartPosition) / PlayRate);
		TWeakObjectPtr<UPMMO_PredictionComponent> WeakThis(this);
		TWeakObjectPtr<AActor> WeakTarget(TargetActor);
		const FPMMO_ReactionPredictionContext CapturedContext = Context;
		const FGameplayTag CapturedReactionTag = ReactionTag;

		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(
			TimerHandle,
			[WeakThis, WeakTarget, CapturedContext, CapturedReactionTag]()
			{
				UPMMO_PredictionComponent* StrongThis = WeakThis.Get();
				AActor* StrongTarget = WeakTarget.Get();
				if (!StrongThis || !StrongTarget)
				{
					return;
				}

				const FVector ServerFinalLocation = StrongTarget->GetActorLocation();
				StrongThis->MulticastFinishConfirmedReaction(
					CapturedContext,
					StrongTarget,
					CapturedReactionTag,
					ServerFinalLocation);
			},
			RemainingDuration,
			false);
	}
	
	if (UPMMO_PredictionComponent* TargetPredictionComponent =
	TargetActor->FindComponentByClass<UPMMO_PredictionComponent>())
	{
		TargetPredictionComponent->ClientPlayOwnerConfirmedReaction(Context, TargetActor, OwnerActor, ReactionTag);
	}
	
	MulticastPlayConfirmedReaction(Context, TargetActor, ReactionTag);
}

void UPMMO_PredictionComponent::MulticastPlayConfirmedReaction_Implementation(FPMMO_ReactionPredictionContext Context,
	AActor* TargetActor, FGameplayTag ReactionTag)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || OwnerActor->HasAuthority()) return;

	if (!TargetActor || !ReactionData || !ReactionTag.IsValid()) return;

	if (ConsumePendingPredictedReaction(Context, TargetActor, ReactionTag))
	{
		AddDeferredPredictedReactionCorrection(Context, TargetActor, ReactionTag);

		UE_LOG(LogTemp, Warning,
			TEXT("SP Multicast skip predicted replay Owner=%s Target=%s Tag=%s PredictionId=%d"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(TargetActor),
			*ReactionTag.ToString(),
			Context.PredictionId);

		return;
	}
	
	const APawn* TargetPawn = Cast<APawn>(TargetActor);
	if (TargetPawn && TargetPawn->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SP Multicast skip locally controlled target Owner=%s Target=%s Tag=%s PredictionId=%d"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(TargetActor),
			*ReactionTag.ToString(),
			Context.PredictionId);

		return;
	}

	FPMMO_ReactionDataEntry Reaction;
	if (!ReactionData->FindReaction(ReactionTag, Reaction)) return;

	const float StartPosition = GetReactionStartPosition(Reaction);
	
	UE_LOG(LogTemp, Warning,
		TEXT("SP Multicast playing confirmed reaction Owner=%s Target=%s Tag=%s PredictionId=%d Role=%d"),
		*GetNameSafe(OwnerActor),
		*GetNameSafe(TargetActor),
		*ReactionTag.ToString(),
		Context.PredictionId,
		TargetActor ? static_cast<int32>(TargetActor->GetLocalRole()) : -1);

	PlayReactionMontageOnActor(TargetActor, Reaction, StartPosition, true);
}

bool UPMMO_PredictionComponent::ConsumePendingPredictedReaction(const FPMMO_ReactionPredictionContext& Context,
	AActor* TargetActor, FGameplayTag ReactionTag)
{
	if (!Context.IsValid() || !TargetActor || !ReactionTag.IsValid()) return false;

	RemoveExpiredPendingPredictedReactions();

	for (int32 Index = PendingPredictedReactions.Num() - 1; Index >= 0; --Index)
	{
		const FPMMO_PendingPredictedReaction& Entry = PendingPredictedReactions[Index];

		if (Entry.TargetActor.Get() == TargetActor &&
			Entry.ReactionTag == ReactionTag &&
			Entry.PredictionId == Context.PredictionId)
		{
			PendingPredictedReactions.RemoveAtSwap(Index);
			return true;
		}
	}

	return false;
}

void UPMMO_PredictionComponent::ClientPlayOwnerConfirmedReaction_Implementation(FPMMO_ReactionPredictionContext Context,
	AActor* TargetActor, AActor* InstigatorActor, FGameplayTag ReactionTag)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || OwnerActor != TargetActor) return;

	if (!TargetActor || !ReactionData || !ReactionTag.IsValid()) return;

	FPMMO_ReactionDataEntry Reaction;
	if (!ReactionData->FindReaction(ReactionTag, Reaction)) return;

	const float StartPosition = GetReactionStartPosition(Reaction);

	PlayReactionMontageOnActor(TargetActor, Reaction, StartPosition, true);
}

void UPMMO_PredictionComponent::MulticastFinishConfirmedReaction_Implementation(
	FPMMO_ReactionPredictionContext Context, AActor* TargetActor, FGameplayTag ReactionTag, FVector ServerFinalLocation)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || OwnerActor->HasAuthority()) return;

	if (!TargetActor || !ReactionTag.IsValid()) return;

	const APawn* TargetPawn = Cast<APawn>(TargetActor);
	if (TargetPawn && TargetPawn->IsLocallyControlled())
	{
		return;
	}

	if (!ConsumeDeferredPredictedReactionCorrection(Context, TargetActor, ReactionTag))
	{
		return;
	}

	const FVector ClientFinalLocation = TargetActor->GetActorLocation();
	const FVector Delta = ServerFinalLocation - ClientFinalLocation;
	const float Distance = Delta.Size();

	UE_LOG(LogTemp, Warning,
		TEXT("SP FinalCorrection Target=%s ClientLoc=%s ServerLoc=%s Delta=%s Distance=%.2f PredictionId=%d"),
		*GetNameSafe(TargetActor),
		*ClientFinalLocation.ToString(),
		*ServerFinalLocation.ToString(),
		*Delta.ToString(),
		Distance,
		Context.PredictionId);

	if (Distance <= FinalCorrectionTolerance)
	{
		return;
	}

	if (!bApplyInstantFinalCorrection)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SP FinalCorrection skipped instant correction disabled Target=%s Distance=%.2f PredictionId=%d"),
			*GetNameSafe(TargetActor),
			Distance,
			Context.PredictionId);

		return;
	}

	if (Distance > MaxInstantFinalCorrectionDistance)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SP FinalCorrection skipped large snap Target=%s Distance=%.2f MaxInstant=%.2f PredictionId=%d"),
			*GetNameSafe(TargetActor),
			Distance,
			MaxInstantFinalCorrectionDistance,
			Context.PredictionId);

		return;
	}

	TargetActor->SetActorLocation(ServerFinalLocation, false, nullptr, ETeleportType::TeleportPhysics);
}

void UPMMO_PredictionComponent::AddDeferredPredictedReactionCorrection(
	const FPMMO_ReactionPredictionContext& Context, AActor* TargetActor, FGameplayTag ReactionTag)
{
	if (!Context.IsValid() || !TargetActor || !ReactionTag.IsValid()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	RemoveExpiredDeferredPredictedReactionCorrections();

	FPMMO_DeferredPredictedReactionCorrection& Entry =
		DeferredPredictedReactionCorrections.AddDefaulted_GetRef();

	Entry.TargetActor = TargetActor;
	Entry.ReactionTag = ReactionTag;
	Entry.PredictionId = Context.PredictionId;
	Entry.TimeSeconds = World->GetTimeSeconds();

	UE_LOG(LogTemp, Warning,
		TEXT("SP AddDeferredPredictedReactionCorrection Target=%s Tag=%s PredictionId=%d Time=%.3f"),
		*GetNameSafe(TargetActor),
		*ReactionTag.ToString(),
		Context.PredictionId,
		Entry.TimeSeconds);
}

bool UPMMO_PredictionComponent::ConsumeDeferredPredictedReactionCorrection(
	const FPMMO_ReactionPredictionContext& Context, AActor* TargetActor, FGameplayTag ReactionTag)
{
	if (!Context.IsValid() || !TargetActor || !ReactionTag.IsValid()) return false;

	RemoveExpiredDeferredPredictedReactionCorrections();

	for (int32 Index = DeferredPredictedReactionCorrections.Num() - 1; Index >= 0; --Index)
	{
		const FPMMO_DeferredPredictedReactionCorrection& Entry = DeferredPredictedReactionCorrections[Index];

		if (Entry.TargetActor.Get() == TargetActor &&
			Entry.ReactionTag == ReactionTag &&
			Entry.PredictionId == Context.PredictionId)
		{
			DeferredPredictedReactionCorrections.RemoveAtSwap(Index);
			return true;
		}
	}

	return false;
}

void UPMMO_PredictionComponent::RemoveExpiredDeferredPredictedReactionCorrections()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		DeferredPredictedReactionCorrections.Reset();
		return;
	}

	const double Now = World->GetTimeSeconds();

	for (int32 Index = DeferredPredictedReactionCorrections.Num() - 1; Index >= 0; --Index)
	{
		const FPMMO_DeferredPredictedReactionCorrection& Entry = DeferredPredictedReactionCorrections[Index];

		const bool bExpired = Now - Entry.TimeSeconds > DeferredPredictedCorrectionTimeout;
		const bool bInvalid =
			!Entry.TargetActor.IsValid() ||
			!Entry.ReactionTag.IsValid() ||
			Entry.PredictionId == INDEX_NONE;

		if (bExpired || bInvalid)
		{
			DeferredPredictedReactionCorrections.RemoveAtSwap(Index);
		}
	}
}

bool UPMMO_PredictionComponent::CanPlayPredictedReactionOnTargetProxy(AActor* TargetActor,
	const FPMMO_ReactionDataEntry& Reaction) const
{
	if (!TargetActor || !Reaction.Montage) return false;

	const UWorld* World = GetWorld();
	if (!World) return false;

	if (World->GetNetMode() == NM_DedicatedServer) return false;

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return false;

	// This component is on the attacker.
	// Only the attacking client should predict target proxy reaction.
	if (OwnerActor->HasAuthority()) return false;

	const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()) return false;

	// The target we are animating should be a proxy on this machine.
	if (TargetActor->HasAuthority()) return false;

	const APawn* TargetPawn = Cast<APawn>(TargetActor);
	if (TargetPawn && TargetPawn->IsLocallyControlled()) return false;

	const double Now = World->GetTimeSeconds();

	if (const double* LastTime = LastReactionTimeByTarget.Find(TargetActor))
	{
		if (Now - *LastTime < Reaction.MinReplayInterval)
		{
			return false;
		}
	}

	return true;
}

FPMMO_ReactionPredictionContext UPMMO_PredictionComponent::MakeReactionPredictionContext()
{
	FPMMO_ReactionPredictionContext Context;
	Context.PredictionId = NextPredictionId;

	NextPredictionId = (NextPredictionId + 1) % MaxPredictionId;
	if (NextPredictionId == INDEX_NONE)
	{
		NextPredictionId = 0;
	}

	return Context;
}

void UPMMO_PredictionComponent::AddPendingPredictedReaction(const FPMMO_ReactionPredictionContext& Context,
	AActor* TargetActor, FGameplayTag ReactionTag)
{
	if (!Context.IsValid() || !TargetActor || !ReactionTag.IsValid()) return;

	UWorld* World = GetWorld();
	if (!World) return;
	
	RemoveExpiredPendingPredictedReactions();
	
	FPMMO_PendingPredictedReaction& Entry =
		PendingPredictedReactions.AddDefaulted_GetRef();
	
	Entry.TargetActor = TargetActor;
	Entry.ReactionTag = ReactionTag;
	Entry.PredictionId = Context.PredictionId;
	Entry.TimeSeconds = World->GetTimeSeconds();
	
	if (PendingPredictedReactions.Num() > MaxPendingPredictedReactions)
	{
		PendingPredictedReactions.RemoveAt(
			0,
			PendingPredictedReactions.Num() - MaxPendingPredictedReactions,
			EAllowShrinking::No);
	}
	
	UE_LOG(LogTemp, Warning,TEXT("SP AddPendingPredictedReaction Target=%s Tag=%s PredictionId=%d Time=%.3f"),
		*GetNameSafe(TargetActor), *ReactionTag.ToString(), Context.PredictionId, Entry.TimeSeconds);
}

void UPMMO_PredictionComponent::RemoveExpiredPendingPredictedReactions()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		PendingPredictedReactions.Reset();
		return;
	}
	
	const double Now = World->GetTimeSeconds();
	
	for (int32 Index = PendingPredictedReactions.Num() - 1; Index >= 0; --Index)
	{
		const FPMMO_PendingPredictedReaction& Entry = PendingPredictedReactions[Index];

		const bool bExpired = Now - Entry.TimeSeconds > PendingPredictedReactionTimeout;
		const bool bInvalid =
			!Entry.TargetActor.IsValid() ||
			!Entry.ReactionTag.IsValid() ||
			Entry.PredictionId == INDEX_NONE;

		if (bExpired || bInvalid)
		{
			PendingPredictedReactions.RemoveAtSwap(Index);
		}
	}
}

void UPMMO_PredictionComponent::AddTemporaryPredictedTargetMovementIgnore(
	AActor* TargetActor,
	float DurationSeconds) const
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	if (!OwnerPawn || !TargetActor || !World)
	{
		return;
	}

	OwnerPawn->MoveIgnoreActorAdd(TargetActor);

	UE_LOG(LogTemp, Warning, TEXT("SP MovementIgnore add Owner=%s Target=%s Duration=%.3f"),
		*GetNameSafe(OwnerActor),
		*GetNameSafe(TargetActor),
		DurationSeconds);

	TWeakObjectPtr<APawn> WeakOwner(OwnerPawn);
	TWeakObjectPtr<AActor> WeakTarget(TargetActor);

	FTimerHandle TimerHandle;
	World->GetTimerManager().SetTimer(
		TimerHandle,
		[WeakOwner, WeakTarget]()
		{
			APawn* StrongOwner = WeakOwner.Get();
			AActor* StrongTarget = WeakTarget.Get();
			if (!StrongOwner || !StrongTarget)
			{
				return;
			}

			StrongOwner->MoveIgnoreActorRemove(StrongTarget);

			UE_LOG(LogTemp, Warning, TEXT("SP MovementIgnore remove Owner=%s Target=%s"),
				*GetNameSafe(StrongOwner),
				*GetNameSafe(StrongTarget));
		},
		FMath::Max(0.05f, DurationSeconds),
		false);
}

float UPMMO_PredictionComponent::GetReactionStartPosition(const FPMMO_ReactionDataEntry& Reaction) const
{
	if (!Reaction.Montage || Reaction.StartSection == NAME_None)
	{
		return 0.f;
	}

	const int32 SectionIndex = Reaction.Montage->GetSectionIndex(Reaction.StartSection);
	if (SectionIndex == INDEX_NONE)
	{
		return 0.f;
	}

	float SectionStartTime = 0.f;
	float SectionEndTime = 0.f;

	Reaction.Montage->GetSectionStartAndEndTime(SectionIndex, SectionStartTime, SectionEndTime);

	return SectionStartTime;
}

bool UPMMO_PredictionComponent::PlayReactionMontageOnActor(AActor* TargetActor, const FPMMO_ReactionDataEntry& Reaction,
	float StartPosition, bool bForceRestart) const
{
	if (!TargetActor || !Reaction.Montage) return false;

	ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
	if (!TargetCharacter) return false;

	USkeletalMeshComponent* Mesh = TargetCharacter->GetMesh();
	if (!Mesh) return false;

	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (!AnimInstance) return false;

	if (!bForceRestart && AnimInstance->Montage_IsPlaying(Reaction.Montage))
	{
		UE_LOG(LogTemp, Warning, TEXT("SP Reaction montage already playing, not restarting Target=%s Montage=%s"),
			*GetNameSafe(TargetActor), *GetNameSafe(Reaction.Montage));

		return true;
	}
	
	const FVector BeforeLocation = TargetActor->GetActorLocation();

	const float PlayedLength = AnimInstance->Montage_Play(Reaction.Montage, Reaction.PlayRate);

	if (PlayedLength <= 0.f) return false;

	const FVector AfterLocation = TargetActor->GetActorLocation();

	UE_LOG(LogTemp, Warning,
		TEXT("SP MontagePlay Location Target=%s Before=%s After=%s Delta=%s Role=%d"),
		*GetNameSafe(TargetActor), *BeforeLocation.ToString(), *AfterLocation.ToString(), 
		*(AfterLocation - BeforeLocation).ToString(), TargetActor ? static_cast<int32>(TargetActor->GetLocalRole()) : -1);
	
	const float MontageLength = Reaction.Montage->GetPlayLength();
	const float ClampedStartPosition = FMath::Clamp(StartPosition,
		0.f, FMath::Max(0.f, MontageLength - KINDA_SMALL_NUMBER));
	
	if (ClampedStartPosition > KINDA_SMALL_NUMBER)
	{
		AnimInstance->Montage_SetPosition(Reaction.Montage, ClampedStartPosition);
	}
	else if (Reaction.StartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(Reaction.StartSection, Reaction.Montage);
	}
	
	UE_LOG(LogTemp, Warning, TEXT("SP PlayReactionMontage Target=%s Montage=%s Start=%.3f ForceRestart=%d"),
		*GetNameSafe(TargetActor), *GetNameSafe(Reaction.Montage), ClampedStartPosition, bForceRestart);

	return true;
}
