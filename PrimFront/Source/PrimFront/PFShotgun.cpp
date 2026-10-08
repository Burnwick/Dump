#include "PFShotgun.h"
#include "PFShapes.h"
#include "PFTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FLinearColor MetalColor(0.06f, 0.06f, 0.07f);
	const FLinearColor DarkMetalColor(0.03f, 0.03f, 0.035f);
	const FLinearColor StockColor(0.22f, 0.12f, 0.06f);
	const FLinearColor PumpColor(0.12f, 0.08f, 0.05f);
	const FLinearColor SightColor(0.9f, 0.55f, 0.05f);
	const FLinearColor FlashColor(1.0f, 0.75f, 0.2f);
	const FLinearColor TracerColor(1.0f, 0.9f, 0.4f);
	const FLinearColor ImpactColor(0.02f, 0.02f, 0.02f);

	constexpr int32 NumImpactMarkers = 64;
	constexpr float TracerLifetime = 0.06f;
	constexpr float FlashLifetime = 0.05f;
	constexpr float PumpTravel = 9.f;
}

APFShotgun::APFShotgun()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

void APFShotgun::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	BuildVisuals();
	Ammo = MagSize;
	Reserve = MaxReserve;
}

void APFShotgun::BuildVisuals()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;

	// Local frame: X along the barrel, origin at the top of the pistol grip (where the trigger hand sits).
	const FRotator AlongX(-90.f, 0.f, 0.f); // turns a cylinder/cone's +Z axis to +X

	// Receiver, barrel and magazine tube
	FPFShapes::AddPart(this, Root, EPFShape::Cube, FVector(30.f, 5.5f, 9.f), FVector(8.f, 0.f, 4.5f), FRotator::ZeroRotator, MetalColor);
	FPFShapes::AddPart(this, Root, EPFShape::Cylinder, FVector(3.2f, 3.2f, 40.f), FVector(43.f, 0.f, 6.5f), AlongX, DarkMetalColor);
	FPFShapes::AddPart(this, Root, EPFShape::Cylinder, FVector(3.f, 3.f, 32.f), FVector(39.f, 0.f, 2.2f), AlongX, MetalColor);
	FPFShapes::AddPart(this, Root, EPFShape::Cube, FVector(3.f, 4.f, 4.f), FVector(55.f, 0.f, 4.f), FRotator::ZeroRotator, MetalColor); // barrel clamp

	// Stock, butt pad, pistol grip, trigger guard
	FPFShapes::AddPart(this, Root, EPFShape::Cube, FVector(30.f, 4.5f, 7.5f), FVector(-20.f, 0.f, 2.f), FRotator(-6.f, 0.f, 0.f), StockColor);
	FPFShapes::AddPart(this, Root, EPFShape::Cube, FVector(2.f, 5.f, 12.f), FVector(-35.5f, 0.f, 0.5f), FRotator(-6.f, 0.f, 0.f), DarkMetalColor);
	FPFShapes::AddPart(this, Root, EPFShape::Cube, FVector(4.5f, 4.f, 10.f), FVector(-4.f, 0.f, -4.5f), FRotator(-18.f, 0.f, 0.f), StockColor);
	FPFShapes::AddPart(this, Root, EPFShape::Cube, FVector(7.f, 1.2f, 1.2f), FVector(2.f, 0.f, -2.2f), FRotator::ZeroRotator, MetalColor);

	// Sights
	FPFShapes::AddPart(this, Root, EPFShape::Sphere, FVector(1.4f, 1.4f, 1.4f), FVector(61.f, 0.f, 8.6f), FRotator::ZeroRotator, SightColor);
	FPFShapes::AddPart(this, Root, EPFShape::Cube, FVector(2.f, 3.f, 2.5f), FVector(16.f, 0.f, 10.f), FRotator::ZeroRotator, DarkMetalColor);

	// Pump (forend) on its own pivot so it can slide back and forth
	PumpRoot = FPFShapes::AddJoint(this, Root, FVector::ZeroVector);
	FPFShapes::AddPart(this, PumpRoot, EPFShape::Cylinder, FVector(5.4f, 5.4f, 16.f), FVector(30.f, 0.f, 2.2f), AlongX, PumpColor);
	PumpSocket = FPFShapes::AddJoint(this, PumpRoot, FVector(30.f, 0.f, -1.f));

	GripSocket = FPFShapes::AddJoint(this, Root, FVector(-3.f, 0.f, -4.f));
	PortSocket = FPFShapes::AddJoint(this, Root, FVector(8.f, 0.f, -1.f));
	MuzzleSocket = FPFShapes::AddJoint(this, Root, FVector(64.f, 0.f, 6.5f));

	// Muzzle flash: a ball and a cone, shown for a couple of frames per shot
	FlashParts.Add(FPFShapes::AddPart(this, MuzzleSocket, EPFShape::Sphere, FVector(10.f, 10.f, 10.f), FVector(4.f, 0.f, 0.f), FRotator::ZeroRotator, FlashColor, false, false));
	FlashParts.Add(FPFShapes::AddPart(this, MuzzleSocket, EPFShape::Cone, FVector(12.f, 12.f, 24.f), FVector(14.f, 0.f, 0.f), AlongX, FlashColor, false, false));
	for (UStaticMeshComponent* Part : FlashParts)
	{
		Part->SetVisibility(false);
	}

	// Pellet tracers live in world space so they don't follow the gun after the shot
	for (int32 i = 0; i < PelletCount; ++i)
	{
		UStaticMeshComponent* Tracer = FPFShapes::AddPart(this, Root, EPFShape::Cylinder, FVector(1.f, 1.f, 1.f), FVector::ZeroVector, FRotator::ZeroRotator, TracerColor, false, false);
		Tracer->SetUsingAbsoluteLocation(true);
		Tracer->SetUsingAbsoluteRotation(true);
		Tracer->SetUsingAbsoluteScale(true);
		Tracer->SetVisibility(false);
		Tracers.Add(Tracer);
		TracerTimers.Add(0.f);
	}

	// Small cubes left where pellets land (re-used round robin)
	for (int32 i = 0; i < NumImpactMarkers; ++i)
	{
		UStaticMeshComponent* Marker = FPFShapes::AddPart(this, Root, EPFShape::Cube, FVector(3.f, 3.f, 3.f), FVector::ZeroVector, FRotator::ZeroRotator, ImpactColor, false, false);
		Marker->SetUsingAbsoluteLocation(true);
		Marker->SetUsingAbsoluteRotation(true);
		Marker->SetUsingAbsoluteScale(true);
		Marker->SetVisibility(false);
		ImpactMarkers.Add(Marker);
	}
}

FPFShotResult APFShotgun::FireShot(const FVector& TraceStart, const FVector& AimDir, float SpreadDegrees, AController* InstigatorController)
{
	FPFShotResult Result;
	UWorld* World = GetWorld();
	if (Ammo <= 0 || !World)
	{
		return Result;
	}

	--Ammo;
	FlashTimer = FlashLifetime;
	for (UStaticMeshComponent* Part : FlashParts)
	{
		Part->SetVisibility(true);
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PFShotgun), false, this);
	Params.AddIgnoredActor(GetOwner());

	const FVector Muzzle = GetMuzzleLocation();
	const float HalfAngle = FMath::DegreesToRadians(SpreadDegrees);
	const FVector Forward = AimDir.GetSafeNormal();

	for (int32 i = 0; i < PelletCount; ++i)
	{
		const FVector Dir = FMath::VRandCone(Forward, HalfAngle);
		const FVector End = TraceStart + Dir * Range;
		FVector TracerEnd = End;
		++Result.Pellets;

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, End, ECC_Visibility, Params))
		{
			TracerEnd = Hit.ImpactPoint;

			if (UPrimitiveComponent* HitComp = Hit.GetComponent())
			{
				if (HitComp->IsSimulatingPhysics())
				{
					HitComp->AddImpulseAtLocation(Dir * PelletImpulse, Hit.ImpactPoint);
				}
				else if (!Hit.GetActor() || !Hit.GetActor()->IsA<APFTarget>())
				{
					// Only mark static scenery; targets fall over and props roll away
					PlaceImpactMarker(Hit);
				}
			}

			if (APFTarget* Target = Cast<APFTarget>(Hit.GetActor()))
			{
				if (!Target->IsDown())
				{
					const float Dist = (float)FVector::Dist(Muzzle, Hit.ImpactPoint);
					const float FalloffAlpha = FMath::Clamp((Dist - FalloffStart) / FMath::Max(FalloffEnd - FalloffStart, 1.f), 0.f, 1.f);
					const float Damage = PelletDamage * FMath::Lerp(1.f, MinDamageScale, FalloffAlpha);
					const bool bHead = Target->IsHeadComponent(Hit.GetComponent());

					UGameplayStatics::ApplyPointDamage(Target, Damage, Dir, Hit, InstigatorController, this, UDamageType::StaticClass());

					++Result.PelletsOnTarget;
					Result.bHeadshot |= bHead;
					if (Target->IsDown())
					{
						++Result.Kills;
						if (bHead)
						{
							++Result.HeadshotKills;
						}
					}
				}
			}
		}

		ShowTracer(i, Muzzle, TracerEnd);
	}

	return Result;
}

void APFShotgun::TickWeapon(float DeltaSeconds)
{
	if (FlashTimer > 0.f)
	{
		FlashTimer -= DeltaSeconds;
		if (FlashTimer <= 0.f)
		{
			for (UStaticMeshComponent* Part : FlashParts)
			{
				Part->SetVisibility(false);
			}
		}
	}

	for (int32 i = 0; i < Tracers.Num(); ++i)
	{
		if (TracerTimers[i] > 0.f)
		{
			TracerTimers[i] -= DeltaSeconds;
			if (TracerTimers[i] <= 0.f)
			{
				Tracers[i]->SetVisibility(false);
			}
		}
	}
}

void APFShotgun::SetPumpAlpha(float Alpha)
{
	if (PumpRoot)
	{
		PumpRoot->SetRelativeLocation(FVector(-PumpTravel * FMath::Clamp(Alpha, 0.f, 1.f), 0.f, 0.f));
	}
}

void APFShotgun::InsertShell()
{
	if (CanReload())
	{
		++Ammo;
		--Reserve;
	}
}

void APFShotgun::Refill()
{
	Ammo = MagSize;
	Reserve = MaxReserve;
}

void APFShotgun::ShowTracer(int32 Index, const FVector& From, const FVector& To)
{
	if (!Tracers.IsValidIndex(Index))
	{
		return;
	}

	const FVector Delta = To - From;
	const float Length = (float)Delta.Size();
	if (Length < 1.f)
	{
		return;
	}

	UStaticMeshComponent* Tracer = Tracers[Index];
	Tracer->SetWorldLocationAndRotation((From + To) * 0.5, FPFShapes::AlongUp(Delta));
	Tracer->SetWorldScale3D(FVector(0.008f, 0.008f, Length / 100.f));
	Tracer->SetVisibility(true);
	TracerTimers[Index] = TracerLifetime;
}

void APFShotgun::PlaceImpactMarker(const FHitResult& Hit)
{
	if (ImpactMarkers.Num() == 0)
	{
		return;
	}

	UStaticMeshComponent* Marker = ImpactMarkers[NextImpactMarker];
	NextImpactMarker = (NextImpactMarker + 1) % ImpactMarkers.Num();

	Marker->SetWorldLocationAndRotation(Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
	Marker->SetWorldScale3D(FVector(0.03f));
	Marker->SetVisibility(true);
}

FVector APFShotgun::GetGripLocation() const
{
	return GripSocket ? GripSocket->GetComponentLocation() : GetActorLocation();
}

FVector APFShotgun::GetPumpLocation() const
{
	return PumpSocket ? PumpSocket->GetComponentLocation() : GetActorLocation();
}

FVector APFShotgun::GetLoadingPortLocation() const
{
	return PortSocket ? PortSocket->GetComponentLocation() : GetActorLocation();
}

FVector APFShotgun::GetMuzzleLocation() const
{
	return MuzzleSocket ? MuzzleSocket->GetComponentLocation() : GetActorLocation();
}
