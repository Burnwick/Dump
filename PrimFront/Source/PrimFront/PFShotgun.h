#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PFShotgun.generated.h"

class UStaticMeshComponent;

/** What one trigger pull did, so the owner can update hit markers and range stats. */
struct FPFShotResult
{
	int32 Pellets = 0;
	int32 PelletsOnTarget = 0;
	int32 Kills = 0;
	int32 HeadshotKills = 0;
	bool bHeadshot = false;
};

/**
 * Pump shotgun built from primitives. It does not tick on its own: the owning character
 * positions it every frame and calls TickWeapon so it stays in sync with the procedural arms.
 */
UCLASS()
class PRIMFRONT_API APFShotgun : public AActor
{
	GENERATED_BODY()

public:
	APFShotgun();

	virtual void PostInitializeComponents() override;

	/** Fires one shell of pellets along AimDir from TraceStart (usually on the camera line). */
	FPFShotResult FireShot(const FVector& TraceStart, const FVector& AimDir, float SpreadDegrees, AController* InstigatorController);

	void TickWeapon(float DeltaSeconds);

	/** 0 = pump forward, 1 = pump racked fully back. Driven by the character's animation. */
	void SetPumpAlpha(float Alpha);

	int32 GetAmmo() const { return Ammo; }
	int32 GetReserve() const { return Reserve; }
	int32 GetMagSize() const { return MagSize; }
	float GetFireInterval() const { return FireInterval; }
	bool CanReload() const { return Ammo < MagSize && Reserve > 0; }
	void InsertShell();
	void Refill();

	FVector GetGripLocation() const;
	FVector GetPumpLocation() const;
	FVector GetLoadingPortLocation() const;
	FVector GetMuzzleLocation() const;

	UPROPERTY(EditAnywhere, Category = "Shotgun")
	int32 MagSize = 8;

	UPROPERTY(EditAnywhere, Category = "Shotgun")
	int32 MaxReserve = 40;

	UPROPERTY(EditAnywhere, Category = "Shotgun")
	int32 PelletCount = 9;

	UPROPERTY(EditAnywhere, Category = "Shotgun")
	float PelletDamage = 12.f;

	UPROPERTY(EditAnywhere, Category = "Shotgun")
	float Range = 6000.f;

	/** Pellets do full damage up to this distance (cm)... */
	UPROPERTY(EditAnywhere, Category = "Shotgun")
	float FalloffStart = 1200.f;

	/** ...and MinDamageScale from this distance on. */
	UPROPERTY(EditAnywhere, Category = "Shotgun")
	float FalloffEnd = 4000.f;

	UPROPERTY(EditAnywhere, Category = "Shotgun")
	float MinDamageScale = 0.35f;

	/** Seconds between shots (time to work the pump). */
	UPROPERTY(EditAnywhere, Category = "Shotgun")
	float FireInterval = 0.85f;

	/** Impulse each pellet gives a physics object. */
	UPROPERTY(EditAnywhere, Category = "Shotgun")
	float PelletImpulse = 3000.f;

private:
	void BuildVisuals();
	void ShowTracer(int32 Index, const FVector& From, const FVector& To);
	void PlaceImpactMarker(const FHitResult& Hit);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<USceneComponent> PumpRoot;

	UPROPERTY()
	TObjectPtr<USceneComponent> GripSocket;

	UPROPERTY()
	TObjectPtr<USceneComponent> PumpSocket;

	UPROPERTY()
	TObjectPtr<USceneComponent> PortSocket;

	UPROPERTY()
	TObjectPtr<USceneComponent> MuzzleSocket;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FlashParts;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Tracers;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ImpactMarkers;

	TArray<float> TracerTimers;
	int32 NextImpactMarker = 0;
	float FlashTimer = 0.f;
	int32 Ammo = 0;
	int32 Reserve = 0;
	bool bBuilt = false;
};
