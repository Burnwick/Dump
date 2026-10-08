#include "HeavyShotgun.h"

#include "HeavyFx.h"
#include "HeavyPartComponent.h"
#include "HeavySpring.h"
#include "HeavyTargetDummy.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace ShotgunColors
{
	const FLinearColor Steel(0.045f, 0.05f, 0.055f);
	const FLinearColor SteelLight(0.11f, 0.115f, 0.12f);
	const FLinearColor Wood(0.30f, 0.15f, 0.06f);
	const FLinearColor WoodDark(0.11f, 0.055f, 0.022f);
	const FLinearColor Rubber(0.02f, 0.02f, 0.02f);
	const FLinearColor Brass(0.78f, 0.56f, 0.22f);
	const FLinearColor Hull(0.55f, 0.05f, 0.04f);
	const FLinearColor Bead(0.85f, 0.85f, 0.8f);
	const FLinearColor Flash(1.0f, 0.78f, 0.32f);
	const FLinearColor Tracer(1.0f, 0.85f, 0.45f);
	const FLinearColor Spark(1.0f, 0.62f, 0.2f);
	const FLinearColor Dust(0.36f, 0.34f, 0.31f);
}

namespace
{
	bool IsActorDown(const AActor* Actor)
	{
		const AHeavyTargetDummy* Dummy = Cast<AHeavyTargetDummy>(Actor);
		return Dummy && Dummy->IsDown();
	}
}

AHeavyShotgun::AHeavyShotgun()
{
	// Ticked by the owning character (UpdateWeapon) so the pose and the pump stay in sync.
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	using namespace ShotgunColors;
	using P = UHeavyPartComponent;

	// Local space: X forward along the barrel, Y right, Z up. The origin sits roughly in
	// the firing hand on the wrist of the stock. Dimensions are a ~100 cm pump gun.
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Receiver, barrel and magazine tube.
	P::CreatePart(this, TEXT("Receiver"), Root, EHeavyShape::Cube, FVector(15.f, 0.f, 5.f), FVector(22.f, 4.6f, 8.f), FRotator::ZeroRotator, Steel);
	P::CreateRodX(this, TEXT("Barrel"), Root, FVector(50.f, 0.f, 7.f), 3.0f, 48.f, Steel);
	P::CreatePart(this, TEXT("Rib"), Root, EHeavyShape::Cube, FVector(50.f, 0.f, 8.7f), FVector(46.f, 0.9f, 0.5f), FRotator::ZeroRotator, SteelLight);
	P::CreateRodX(this, TEXT("MagTube"), Root, FVector(46.f, 0.f, 3.4f), 2.6f, 40.f, Steel);
	P::CreateRodX(this, TEXT("MagCap"), Root, FVector(66.5f, 0.f, 3.4f), 2.9f, 1.6f, SteelLight);
	P::CreatePart(this, TEXT("BarrelClamp"), Root, EHeavyShape::Cube, FVector(62.5f, 0.f, 5.2f), FVector(2.2f, 3.2f, 6.2f), FRotator::ZeroRotator, Steel);
	P::CreatePart(this, TEXT("FrontBead"), Root, EHeavyShape::Sphere, FVector(73.4f, 0.f, 8.9f), FVector(0.9f), FRotator::ZeroRotator, Bead);
	P::CreatePart(this, TEXT("EjectionPortCut"), Root, EHeavyShape::Cube, FVector(16.f, 2.33f, 6.4f), FVector(6.5f, 0.2f, 2.4f), FRotator::ZeroRotator, Rubber);

	// Trigger group.
	P::CreatePart(this, TEXT("GuardBar"), Root, EHeavyShape::Cube, FVector(11.f, 0.f, -1.4f), FVector(10.f, 1.2f, 0.8f), FRotator::ZeroRotator, Steel);
	P::CreatePart(this, TEXT("GuardFront"), Root, EHeavyShape::Cube, FVector(15.6f, 0.f, -0.2f), FVector(0.8f, 1.2f, 2.6f), FRotator::ZeroRotator, Steel);
	P::CreatePart(this, TEXT("GuardRear"), Root, EHeavyShape::Cube, FVector(6.4f, 0.f, -0.2f), FVector(0.8f, 1.2f, 2.6f), FRotator::ZeroRotator, Steel);
	P::CreatePart(this, TEXT("Trigger"), Root, EHeavyShape::Cube, FVector(10.f, 0.f, -0.1f), FVector(0.6f, 0.7f, 2.2f), FRotator(-12.f, 0.f, 0.f), SteelLight);

	// Walnut stock with a rubber butt pad. Positive pitch drops the rear.
	P::CreatePart(this, TEXT("StockWrist"), Root, EHeavyShape::Cube, FVector(-1.f, 0.f, 4.2f), FVector(11.f, 3.8f, 5.2f), FRotator(8.f, 0.f, 0.f), Wood);
	P::CreatePart(this, TEXT("Stock"), Root, EHeavyShape::Cube, FVector(-18.f, 0.f, 2.2f), FVector(26.f, 4.2f, 8.5f), FRotator(7.f, 0.f, 0.f), Wood);
	P::CreatePart(this, TEXT("ButtPad"), Root, EHeavyShape::Cube, FVector(-31.6f, 0.f, 1.0f), FVector(1.8f, 4.5f, 12.f), FRotator(7.f, 0.f, 0.f), Rubber);

	// Side saddle of spare shells on the left of the receiver.
	P::CreatePart(this, TEXT("SaddlePlate"), Root, EHeavyShape::Cube, FVector(14.f, -2.55f, 5.f), FVector(11.f, 0.5f, 5.6f), FRotator::ZeroRotator, Rubber);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float X = 10.f + 2.6f * static_cast<float>(Index);
		P::CreatePart(this, *FString::Printf(TEXT("SaddleShell%d"), Index), Root, EHeavyShape::Cylinder,
			FVector(X, -3.6f, 5.4f), FVector(1.9f, 1.9f, 5.f), FRotator::ZeroRotator, Hull);
		P::CreatePart(this, *FString::Printf(TEXT("SaddleBrass%d"), Index), Root, EHeavyShape::Cylinder,
			FVector(X, -3.6f, 2.4f), FVector(2.0f, 2.0f, 1.2f), FRotator::ZeroRotator, Brass);
	}

	// Ribbed walnut fore-end. Everything under PumpRoot slides when racking.
	PumpRestLocation = FVector(40.f, 0.f, 3.4f);
	PumpRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PumpRoot"));
	PumpRoot->SetupAttachment(Root);
	PumpRoot->SetRelativeLocation(PumpRestLocation);
	P::CreateRodX(this, TEXT("PumpBody"), PumpRoot, FVector::ZeroVector, 5.0f, 17.f, Wood);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		P::CreateRodX(this, *FString::Printf(TEXT("PumpGroove%d"), Index), PumpRoot,
			FVector(-6.f + 3.f * static_cast<float>(Index), 0.f, 0.f), 5.2f, 0.6f, WoodDark);
	}

	// Attachment points for hands and effects.
	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Root);
	Muzzle->SetRelativeLocation(FVector(74.2f, 0.f, 7.f));

	TriggerHandSocket = CreateDefaultSubobject<USceneComponent>(TEXT("TriggerHand"));
	TriggerHandSocket->SetupAttachment(Root);
	TriggerHandSocket->SetRelativeLocation(FVector(0.f, 1.0f, 3.2f));

	PumpHandSocket = CreateDefaultSubobject<USceneComponent>(TEXT("PumpHand"));
	PumpHandSocket->SetupAttachment(PumpRoot);
	PumpHandSocket->SetRelativeLocation(FVector(0.f, 0.f, -3.0f));

	EjectionPort = CreateDefaultSubobject<USceneComponent>(TEXT("EjectionPort"));
	EjectionPort->SetupAttachment(Root);
	EjectionPort->SetRelativeLocation(FVector(16.f, 3.0f, 6.4f));

	LoadingPort = CreateDefaultSubobject<USceneComponent>(TEXT("LoadingPort"));
	LoadingPort->SetupAttachment(Root);
	LoadingPort->SetRelativeLocation(FVector(21.f, 0.f, 0.3f));

	// Muzzle flash: a cone pointing down-range, a hot core and a brief light.
	FlashCone = P::CreatePart(this, TEXT("FlashCone"), Muzzle, EHeavyShape::Cone, FVector(9.f, 0.f, 0.f), FVector(8.f, 8.f, 18.f), FRotator(-90.f, 0.f, 0.f), Flash);
	FlashCore = P::CreatePart(this, TEXT("FlashCore"), Muzzle, EHeavyShape::Sphere, FVector(1.5f, 0.f, 0.f), FVector(6.f), FRotator::ZeroRotator, Flash);
	FlashCone->SetCastShadow(false);
	FlashCore->SetCastShadow(false);
	FlashCone->SetVisibility(false);
	FlashCore->SetVisibility(false);

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(Muzzle);
	MuzzleLight->SetRelativeLocation(FVector(8.f, 0.f, 0.f));
	MuzzleLight->IntensityUnits = ELightUnits::Candelas;
	MuzzleLight->Intensity = 0.f;
	MuzzleLight->AttenuationRadius = 650.f;
	MuzzleLight->LightColor = FColor(255, 190, 110);
	MuzzleLight->CastShadows = false;
	MuzzleLight->SetVisibility(false);

	Shells = Capacity;
}

void AHeavyShotgun::BeginPlay()
{
	Super::BeginPlay();
	Shells = Capacity;
	bChamberEmpty = false;
}

// ---------------------------------------------------------------------------------------
// Queries

bool AHeavyShotgun::IsReadyToFire() const
{
	return State == EShotgunState::Ready && Shells > 0 && !bChamberEmpty;
}

bool AHeavyShotgun::WantsReloadPose() const
{
	return State == EShotgunState::Reloading && ReloadPhase != EShotgunReloadPhase::End;
}

float AHeavyShotgun::GetInsertPhase() const
{
	if (State == EShotgunState::Reloading && ReloadPhase == EShotgunReloadPhase::Insert)
	{
		return HeavyMath::Clamp01(StateTime / FMath::Max(ReloadInsertTime, 0.01f));
	}
	return -1.f;
}

FVector AHeavyShotgun::GetMuzzleLocation() const { return Muzzle->GetComponentLocation(); }
FVector AHeavyShotgun::GetMuzzleDirection() const { return Muzzle->GetForwardVector(); }
FVector AHeavyShotgun::GetTriggerHandLocation() const { return TriggerHandSocket->GetComponentLocation(); }
FVector AHeavyShotgun::GetPumpHandLocation() const { return PumpHandSocket->GetComponentLocation(); }
FVector AHeavyShotgun::GetLoadingPortLocation() const { return LoadingPort->GetComponentLocation(); }

float AHeavyShotgun::DamageScaleAtDistance(float Distance) const
{
	return HeavyMath::MapClamped(FalloffStart, FalloffEnd, 1.f, MinDamageScale, Distance);
}

// ---------------------------------------------------------------------------------------
// Firing

bool AHeavyShotgun::Fire(const FVector& AimPoint, float SpreadDegrees, TArray<FShotgunHitReport>& OutHits)
{
	OutHits.Reset();

	if (State == EShotgunState::Reloading)
	{
		InterruptReload();
		return false;
	}
	if (State != EShotgunState::Ready)
	{
		return false;
	}
	if (Shells <= 0)
	{
		PlayWeaponSound(DryFireSound);
		StartReload();
		return false;
	}
	if (bChamberEmpty)
	{
		// Rounds in the tube but nothing chambered (reload was interrupted from empty): rack it.
		BeginCycle(false, 0.f);
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	--Shells;
	bChamberEmpty = (Shells == 0);
	BeginCycle(true, PumpDelay);
	TriggerMuzzleFlash();
	PlayWeaponSound(FireSound);

	const FVector MuzzleLocation = GetMuzzleLocation();
	const FVector BarrelDirection = GetMuzzleDirection();
	FVector AimDirection = (AimPoint - MuzzleLocation).GetSafeNormal();
	if (AimDirection.IsNearlyZero() || FVector::DotProduct(AimDirection, BarrelDirection) < 0.7f)
	{
		// Aim point is behind or beside the muzzle (hugging a wall): shoot where the barrel points.
		AimDirection = BarrelDirection;
	}

	FVector Up;
	FVector Right;
	AimDirection.FindBestAxisVectors(Up, Right);

	const float SpreadTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(SpreadDegrees, 0.f, 45.f)));
	const float PatternRoll = FMath::FRandRange(0.f, 2.f * PI);
	const int32 RingPellets = FMath::Max(PelletCount - 1, 1);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeavyShotgunPellet), true);
	Params.AddIgnoredActor(this);
	if (AActor* MyOwner = GetOwner())
	{
		Params.AddIgnoredActor(MyOwner);
	}

	AController* InstigatorController = GetInstigatorController();

	for (int32 Index = 0; Index < PelletCount; ++Index)
	{
		// One pellet near the centre and the rest in a jittered ring: a readable,
		// consistent pattern instead of pure noise.
		float Radius;
		float Angle;
		if (Index == 0)
		{
			Radius = FMath::FRandRange(0.f, 0.2f);
			Angle = FMath::FRandRange(0.f, 2.f * PI);
		}
		else
		{
			Radius = FMath::FRandRange(0.55f, 1.f);
			Angle = PatternRoll + 2.f * PI * static_cast<float>(Index - 1) / static_cast<float>(RingPellets) + FMath::FRandRange(-0.25f, 0.25f);
		}

		const FVector Offset = (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * (Radius * SpreadTan);
		const FVector PelletDirection = (AimDirection + Offset).GetSafeNormal();
		const FVector TraceEnd = MuzzleLocation + PelletDirection * MaxRange;

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, MuzzleLocation, TraceEnd, ECC_Visibility, Params);
		const FVector PelletEnd = bHit ? FVector(Hit.ImpactPoint) : TraceEnd;

		if (bSpawnTracers)
		{
			SpawnTracer(MuzzleLocation, PelletEnd);
		}
		if (!bHit)
		{
			continue;
		}

		SpawnImpactFx(Hit.ImpactPoint, Hit.ImpactNormal);

		const float DistanceScale = DamageScaleAtDistance(static_cast<float>(Hit.Distance));

		if (UPrimitiveComponent* HitComponent = Hit.GetComponent())
		{
			if (HitComponent->IsSimulatingPhysics())
			{
				HitComponent->AddImpulseAtLocation(PelletDirection * (ImpulsePerPellet * DistanceScale), Hit.ImpactPoint);
			}
		}

		AActor* HitActor = Hit.GetActor();
		if (!HitActor || !HitActor->CanBeDamaged())
		{
			continue;
		}

		const bool bWasStanding = !IsActorDown(HitActor);
		const float BaseDamage = DamagePerPellet * DistanceScale;
		const float Dealt = UGameplayStatics::ApplyPointDamage(HitActor, BaseDamage, PelletDirection, Hit,
			InstigatorController, this, UDamageType::StaticClass());
		// Damage still goes to everything (for Blueprint hooks), but only targets and
		// characters produce hit markers; walls and props don't.
		const bool bReportable = HitActor->IsA<AHeavyTargetDummy>() || HitActor->IsA<APawn>();
		if (Dealt <= 0.f || !bReportable)
		{
			continue;
		}

		FShotgunHitReport* Report = OutHits.FindByPredicate([HitActor](const FShotgunHitReport& Existing)
		{
			return Existing.Actor.Get() == HitActor;
		});
		if (!Report)
		{
			Report = &OutHits.AddDefaulted_GetRef();
			Report->Actor = HitActor;
			Report->Location = Hit.ImpactPoint;
		}

		Report->Pellets += 1;
		Report->Damage += Dealt;
		Report->Location += (FVector(Hit.ImpactPoint) - Report->Location) / static_cast<float>(Report->Pellets);
		Report->bCritical |= Dealt > BaseDamage * 1.05f;
		Report->bKilled |= bWasStanding && IsActorDown(HitActor);
	}

	return true;
}

void AHeavyShotgun::BeginCycle(bool bEjectHull, float Delay)
{
	State = EShotgunState::Cycling;
	StateTime = 0.f;
	CycleDelay = Delay;
	bCycleEjects = bEjectHull;
	bCycleEjected = false;
	bCycleBackSoundPlayed = false;
	bCycleForwardSoundPlayed = false;
	if (!bEjectHull)
	{
		// Racking to chamber a round from the tube.
		bChamberEmpty = false;
	}
}

// ---------------------------------------------------------------------------------------
// Reloading

bool AHeavyShotgun::StartReload()
{
	if (State != EShotgunState::Ready || Shells >= Capacity)
	{
		return false;
	}
	State = EShotgunState::Reloading;
	ReloadPhase = EShotgunReloadPhase::Start;
	StateTime = 0.f;
	bShellInsertedThisStep = false;
	return true;
}

void AHeavyShotgun::InterruptReload()
{
	if (State != EShotgunState::Reloading || Shells <= 0)
	{
		// Nothing to shoot yet: keep loading.
		return;
	}
	if (bChamberEmpty)
	{
		BeginCycle(false, 0.05f);
	}
	else
	{
		State = EShotgunState::Ready;
		StateTime = 0.f;
	}
}

// ---------------------------------------------------------------------------------------
// Per-frame

void AHeavyShotgun::UpdateWeapon(float DeltaTime)
{
	StateTime += DeltaTime;

	switch (State)
	{
	case EShotgunState::Cycling:
		UpdateCycle();
		break;
	case EShotgunState::Reloading:
		UpdateReload();
		PumpAlpha = 0.f;
		break;
	default:
		PumpAlpha = 0.f;
		break;
	}

	PumpRoot->SetRelativeLocation(PumpRestLocation - FVector(PumpAlpha * PumpTravel, 0.f, 0.f));
	UpdateMuzzleFlash(DeltaTime);
}

void AHeavyShotgun::UpdateCycle()
{
	const float T = (StateTime - CycleDelay) / FMath::Max(PumpDuration, 0.01f);
	if (T <= 0.f)
	{
		PumpAlpha = 0.f;
		return;
	}

	if (T >= 1.f)
	{
		PumpAlpha = 0.f;
		State = EShotgunState::Ready;
		StateTime = 0.f;
		return;
	}

	// Back stroke, a short dwell at the rear, then slam forward.
	constexpr float BackEnd = 0.42f;
	constexpr float ForwardStart = 0.55f;
	if (T < BackEnd)
	{
		PumpAlpha = HeavyMath::Ease(T / BackEnd);
	}
	else if (T < ForwardStart)
	{
		PumpAlpha = 1.f;
	}
	else
	{
		PumpAlpha = 1.f - HeavyMath::Ease((T - ForwardStart) / (1.f - ForwardStart));
	}

	if (!bCycleBackSoundPlayed)
	{
		bCycleBackSoundPlayed = true;
		PlayWeaponSound(PumpBackSound);
	}
	if (bCycleEjects && !bCycleEjected && T >= BackEnd * 0.9f)
	{
		bCycleEjected = true;
		EjectShell();
	}
	if (!bCycleForwardSoundPlayed && T >= ForwardStart)
	{
		bCycleForwardSoundPlayed = true;
		PlayWeaponSound(PumpForwardSound);
	}
}

void AHeavyShotgun::UpdateReload()
{
	switch (ReloadPhase)
	{
	case EShotgunReloadPhase::Start:
		if (StateTime >= ReloadStartTime)
		{
			ReloadPhase = EShotgunReloadPhase::Insert;
			StateTime = 0.f;
			bShellInsertedThisStep = false;
		}
		break;

	case EShotgunReloadPhase::Insert:
		// The shell goes in near the end of the hand's motion.
		if (!bShellInsertedThisStep && StateTime >= ReloadInsertTime * 0.72f)
		{
			bShellInsertedThisStep = true;
			Shells = FMath::Min(Shells + 1, Capacity);
			PlayWeaponSound(InsertShellSound);
		}
		if (StateTime >= ReloadInsertTime)
		{
			StateTime = 0.f;
			bShellInsertedThisStep = false;
			if (Shells >= Capacity)
			{
				ReloadPhase = EShotgunReloadPhase::End;
			}
		}
		break;

	case EShotgunReloadPhase::End:
	default:
		if (StateTime >= ReloadEndTime)
		{
			if (bChamberEmpty && Shells > 0)
			{
				BeginCycle(false, 0.f);
			}
			else
			{
				State = EShotgunState::Ready;
				StateTime = 0.f;
			}
		}
		break;
	}
}

// ---------------------------------------------------------------------------------------
// Effects

void AHeavyShotgun::TriggerMuzzleFlash()
{
	MuzzleFlashTimeLeft = MuzzleFlashDuration;

	const float Length = FMath::FRandRange(14.f, 24.f);
	const float Width = FMath::FRandRange(7.f, 10.f);
	FlashCone->SetRelativeScale3D(FVector(Width, Width, Length) / 100.f);
	FlashCone->SetRelativeLocation(FVector(Length * 0.5f, 0.f, 0.f));
	FlashCone->SetVisibility(true);
	FlashCore->SetVisibility(true);

	MuzzleLight->SetVisibility(true);
	MuzzleLight->SetIntensity(MuzzleFlashIntensity);
}

void AHeavyShotgun::UpdateMuzzleFlash(float DeltaTime)
{
	if (MuzzleFlashTimeLeft <= 0.f)
	{
		return;
	}

	MuzzleFlashTimeLeft -= DeltaTime;
	if (MuzzleFlashTimeLeft <= 0.f)
	{
		FlashCone->SetVisibility(false);
		FlashCore->SetVisibility(false);
		MuzzleLight->SetVisibility(false);
		return;
	}

	const float Alpha = MuzzleFlashTimeLeft / FMath::Max(MuzzleFlashDuration, 0.001f);
	MuzzleLight->SetIntensity(MuzzleFlashIntensity * Alpha);
}

void AHeavyShotgun::EjectShell()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AHeavyShellCasing* Shell = World->SpawnActor<AHeavyShellCasing>(AHeavyShellCasing::StaticClass(),
		EjectionPort->GetComponentLocation(), GetActorRotation(), SpawnParams);
	if (!Shell)
	{
		return;
	}

	const FVector Inherited = GetOwner() ? GetOwner()->GetVelocity() : FVector::ZeroVector;
	const FVector Velocity = Inherited
		+ GetActorRightVector() * FMath::FRandRange(170.f, 260.f)
		+ GetActorUpVector() * FMath::FRandRange(120.f, 210.f)
		- GetActorForwardVector() * FMath::FRandRange(20.f, 70.f);
	const FVector Spin(FMath::FRandRange(-720.f, 720.f), FMath::FRandRange(-360.f, 360.f), FMath::FRandRange(900.f, 1600.f));
	Shell->Launch(Velocity, Spin);
}

void AHeavyShotgun::SpawnTracer(const FVector& Start, const FVector& End) const
{
	const FVector Delta = End - Start;
	const float Distance = static_cast<float>(Delta.Size());
	if (Distance < 50.f)
	{
		return;
	}

	// A short streak that flies down the pellet's path rather than a static beam.
	constexpr float Speed = 32000.f;
	const float StreakLength = FMath::Min(Distance * 0.5f, 220.f);
	const FVector Direction = Delta / Distance;
	const float Lifetime = FMath::Max((Distance - StreakLength) / Speed, 0.03f);

	const FTransform Transform(
		FRotationMatrix::MakeFromZ(Direction).Rotator(),
		Start + Direction * (StreakLength * 0.5f + 20.f),
		FVector(1.1f, 1.1f, StreakLength) / 100.f);

	AHeavyTransientFx::Spawn(GetWorld(), EHeavyShape::Cylinder, Transform, ShotgunColors::Tracer, Lifetime,
		Transform.GetScale3D() * FVector(0.4f, 0.4f, 1.f), Direction * Speed);
}

void AHeavyShotgun::SpawnImpactFx(const FVector& Location, const FVector& Normal) const
{
	UWorld* World = GetWorld();

	// Hot flash at the impact point.
	AHeavyTransientFx::Spawn(World, EHeavyShape::Sphere,
		FTransform(FRotator::ZeroRotator, Location + Normal * 2.f, FVector(FMath::FRandRange(5.f, 8.f)) / 100.f),
		ShotgunColors::Spark, 0.14f, FVector::ZeroVector);

	// A couple of chips of debris kicked off the surface.
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FVector Kick = (Normal + FMath::VRand() * 0.7f).GetSafeNormal() * FMath::FRandRange(250.f, 520.f);
		AHeavyTransientFx::Spawn(World, EHeavyShape::Cube,
			FTransform(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f), Location + Normal * 2.f, FVector(2.4f) / 100.f),
			ShotgunColors::Dust, FMath::FRandRange(0.3f, 0.5f), FVector::ZeroVector, Kick, 980.f);
	}
}

void AHeavyShotgun::PlayWeaponSound(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}
