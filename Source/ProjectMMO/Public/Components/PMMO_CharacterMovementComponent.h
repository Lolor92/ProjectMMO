#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PMMO_CharacterMovementComponent.generated.h"

class UPrimitiveComponent;
class UAnimMontage;

UCLASS()
class PROJECTMMO_API UPMMO_CharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	void ConfigureRootMotionContactStop(bool bEnabled, float StopAngleDegrees, float ReleasePercent,
		float CapsuleTolerance, UObject* SourceObject = nullptr);
	void ClearRootMotionContactStop(UObject* SourceObject = nullptr);
	void SetRootMotionContactBlock(bool bBlocked, const FVector& InBlockedDirection, float Duration = 0.0f);
	void ClearRootMotionContactBlock();

	bool IsRootMotionContactBlocked() const { return bRootMotionContactBlocked; }

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ApplyRootMotionToVelocity(float DeltaTime) override;

	virtual FVector CalcAnimRootMotionVelocity(
		const FVector& RootMotionDeltaMove,
		float DeltaSeconds,
		const FVector& CurrentVelocity) const override;

private:
	UFUNCTION()
	void HandleOwnerCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	void BindOwnerCapsuleHit();
	void UnbindOwnerCapsuleHit();
	bool ShouldBlockRootMotionForCapsuleHit(const AActor* OtherActor, const UPrimitiveComponent* OtherComp,
		const FHitResult& Hit) const;
	bool TryStartRootMotionContactBlockFromCapsuleProbe(float DeltaTime, const FVector& InVelocity);
	bool GetRootMotionContactOwnerCapsuleLocation(FVector& OutLocation) const;
	bool GetRootMotionContactOtherCapsuleLocation(const AActor* OtherActor, FVector& OutLocation) const;
	bool AreRootMotionContactCapsulesTouching(const UCapsuleComponent* OwnerCapsule, const UCapsuleComponent* OtherCapsule,
		const FVector& OwnerLocation, const FVector& OtherLocation, float& OutHorizontalDistance,
		float& OutMaxHorizontalDistance, float& OutVerticalDistance, float& OutMaxVerticalDistance) const;
	bool IsRootMotionContactWithinAngle(const FVector& OwnerLocation, const FVector& OtherLocation,
		float& OutForwardDot, float& OutMinForwardDot) const;
	bool HasReachedRootMotionContactReleasePercent() const;
	UAnimMontage* GetRootMotionContactMontage() const;
	FVector RemoveVelocityIntoRootMotionContactBlock(const FVector& InVelocity) const;

	UPROPERTY(Transient)
	bool bRootMotionContactStopEnabled = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> RootMotionContactStopSource;

	UPROPERTY(Transient)
	bool bRootMotionContactBlocked = false;

	UPROPERTY(Transient)
	FVector RootMotionContactBlockedDirection = FVector::ZeroVector;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAnimMontage> RootMotionContactMontage;

	FTimerHandle RootMotionContactBlockTimerHandle;

	float RootMotionContactStopAngleDegrees = 40.0f;

	float RootMotionContactReleasePercent = 100.0f;

	float RootMotionContactCapsuleTolerance = 5.0f;

	float RootMotionContactBlockDuration = 0.15f;

	mutable double LastRootMotionContactClampLogTime = -1000.0;
};
