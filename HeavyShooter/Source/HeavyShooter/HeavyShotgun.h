#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeavyShotgun.generated.h"

class UHeavyPartComponent;
class UPointLightComponent;
class USceneComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EShotgunState : uint8
{
	Ready,
	/** Working the pump: after a shot, or to chamber a round after reloading from empty. */
	Cycling,
	Reloading
};

UENUM(BlueprintType)
enum class EShotgunReloadPhase : uint8
{
	Start,
	Insert,
	End
};

/** Everything one trigger pull did to one actor (all pellets summed). */
struct FShotgunHitReport
{
	TWeakObjectPtr<AActor> Actor;
	FVector Location = FVector::ZeroVector;
	float Damage = 0.f;
	int32 Pellets = 0;
	bool bCritical = false;
	bool bKilled = false;
};

/**
 * A pump-action shotgun assembled from primitives: blued receiver and barrel, magazine
 * tube, walnut stock and ribbed fore-end, bead sight and a side-saddle of spare shells.
 *
 * Fire -> short delay -> the fore-end racks back (spent hull ejects) and forward.
 * Reloads go one shell at a time and can be interrupted by firing. Reloading from
 * empty finishes with a pump to chamber a round.
 *
 * The owning character poses the gun and then calls UpdateWeapon() so its hand IK
 * always reads the pump where it actually is this frame.
 */
UCLASS()
class HEAVYSHOOTER_API AHeavyShotgun : public AActor
{
	GENERATED_BODY()

public:
	AHeavyShotgun();

	void UpdateWeapon(float DeltaTime);

	bool IsReadyToFire() const;

	/** Fires PelletCount pellets from the muzzle towards AimPoint. Returns false if the gun couldn't fire. */
	bool Fire(const FVector& AimPoint, float SpreadDegrees, TArray<FShotgunHitReport>& OutHits);

	bool StartReload();

	/** Stops a reload early so the next trigger pull can fire (pumps first if the chamber is empty). */
	void InterruptReload();

	EShotgunState GetState() const { return State; }
	int32 GetShells() const { return Shells; }
	int32 GetCapacity() const { return Capacity; }
	bool IsReloading() const { return State == EShotgunState::Reloading; }
	bool IsCycling() const { return State == EShotgunState::Cycling; }
	float GetPumpAlpha() const { return PumpAlpha; }

	/** True while the support hand should leave the pump to feed shells. */
	bool WantsReloadPose() const;

	/** 0..1 progress through feeding the current shell, or -1 when the hand isn't feeding. */
	float GetInsertPhase() const;

	FVector GetMuzzleLocation() const;
	FVector GetMuzzleDirection() const;
	FVector GetTriggerHandLocation() const;
	FVector GetPumpHandLocation() const;
	FVector GetLoadingPortLocation() const;

	// ---- Damage -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Damage", meta = (ClampMin = "1"))
	int32 PelletCount = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Damage")
	float DamagePerPellet = 14.f;

	/** Full damage up to this distance (cm)... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Damage")
	float FalloffStart = 900.f;

	/** ...dropping linearly to MinDamageScale at this distance (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Damage")
	float FalloffEnd = 2800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Damage")
	float MinDamageScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Damage")
	float MaxRange = 8000.f;

	/** Impulse each pellet applies to physics objects (kg*cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Damage")
	float ImpulsePerPellet = 2800.f;

	// ---- Handling -----------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Handling", meta = (ClampMin = "1"))
	int32 Capacity = 6;

	/** Time between the shot and the hand starting to rack the pump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Handling")
	float PumpDelay = 0.16f;

	/** Full back-and-forward stroke of the pump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Handling")
	float PumpDuration = 0.44f;

	/** How far the fore-end travels (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Handling")
	float PumpTravel = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Handling")
	float ReloadStartTime = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Handling")
	float ReloadInsertTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Handling")
	float ReloadEndTime = 0.22f;

	// ---- Effects ------------------------------------------------------------------------

	/** Peak muzzle flash brightness in candelas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Effects")
	float MuzzleFlashIntensity = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Effects")
	float MuzzleFlashDuration = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Effects")
	bool bSpawnTracers = true;

	// ---- Audio (optional: assign any sound assets you have) -----------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Audio")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Audio")
	TObjectPtr<USoundBase> PumpBackSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Audio")
	TObjectPtr<USoundBase> PumpForwardSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Audio")
	TObjectPtr<USoundBase> InsertShellSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shotgun|Audio")
	TObjectPtr<USoundBase> DryFireSound;

protected:
	virtual void BeginPlay() override;

private:
	void BeginCycle(bool bEjectHull, float Delay);
	void UpdateCycle();
	void UpdateReload();
	void UpdateMuzzleFlash(float DeltaTime);
	void TriggerMuzzleFlash();
	void EjectShell();
	void SpawnTracer(const FVector& Start, const FVector& End) const;
	void SpawnImpactFx(const FVector& Location, const FVector& Normal) const;
	void PlayWeaponSound(USoundBase* Sound) const;
	float DamageScaleAtDistance(float Distance) const;

	UPROPERTY(VisibleAnywhere, Category = "Shotgun")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Shotgun")
	TObjectPtr<USceneComponent> PumpRoot;

	UPROPERTY(VisibleAnywhere, Category = "Shotgun")
	TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY(VisibleAnywhere, Category = "Shotgun")
	TObjectPtr<USceneComponent> TriggerHandSocket;

	UPROPERTY(VisibleAnywhere, Category = "Shotgun")
	TObjectPtr<USceneComponent> PumpHandSocket;

	UPROPERTY(VisibleAnywhere, Category = "Shotgun")
	TObjectPtr<USceneComponent> EjectionPort;

	UPROPERTY(VisibleAnywhere, Category = "Shotgun")
	TObjectPtr<USceneComponent> LoadingPort;

	UPROPERTY(VisibleAnywhere, Category = "Shotgun")
	TObjectPtr<UHeavyPartComponent> FlashCone;

	UPROPERTY(VisibleAnywhere, Category = "Shotgun")
	TObjectPtr<UHeavyPartComponent> FlashCore;

	UPROPERTY(VisibleAnywhere, Category = "Shotgun")
	TObjectPtr<UPointLightComponent> MuzzleLight;

	FVector PumpRestLocation = FVector::ZeroVector;

	EShotgunState State = EShotgunState::Ready;
	EShotgunReloadPhase ReloadPhase = EShotgunReloadPhase::Start;
	float StateTime = 0.f;
	int32 Shells = 0;
	bool bChamberEmpty = false;

	float CycleDelay = 0.f;
	bool bCycleEjects = false;
	bool bCycleEjected = false;
	bool bCycleBackSoundPlayed = false;
	bool bCycleForwardSoundPlayed = false;
	bool bShellInsertedThisStep = false;

	float PumpAlpha = 0.f;
	float MuzzleFlashTimeLeft = 0.f;
};
