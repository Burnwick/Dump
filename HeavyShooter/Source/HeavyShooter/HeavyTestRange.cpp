#include "HeavyTestRange.h"

#include "HeavyPartComponent.h"
#include "HeavyTargetDummy.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace RangeColors
{
	const FLinearColor TileA(0.30f, 0.31f, 0.32f);
	const FLinearColor TileB(0.23f, 0.24f, 0.25f);
	const FLinearColor Wall(0.42f, 0.40f, 0.37f);
	const FLinearColor Cover(0.33f, 0.35f, 0.29f);
	const FLinearColor Platform(0.40f, 0.38f, 0.34f);
	const FLinearColor Marker(0.9f, 0.68f, 0.08f);
	const FLinearColor MarkerAlt(0.12f, 0.4f, 0.75f);
	const FLinearColor Crate(0.45f, 0.31f, 0.16f);
	const FLinearColor CrateDark(0.32f, 0.22f, 0.11f);
	const FLinearColor BarrelBlue(0.13f, 0.27f, 0.45f);
	const FLinearColor BarrelRed(0.58f, 0.14f, 0.07f);
}

AHeavyTestRange::AHeavyTestRange()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AHeavyTestRange::BeginPlay()
{
	Super::BeginPlay();

	if (bAddLightingIfMissing)
	{
		BuildLighting();
	}
	if (bBuildFloor)
	{
		BuildGeometry();
	}
	if (bSpawnTargets)
	{
		SpawnTargets();
	}
	if (bSpawnPhysicsProps)
	{
		SpawnProps();
	}
}

void AHeavyTestRange::Block(const FVector& Center, const FVector& Size, const FLinearColor& Color, const FRotator& Rotation)
{
	UHeavyPartComponent::SpawnPart(this, Root, EHeavyShape::Cube, Center, Size, Rotation, Color, true);
}

void AHeavyTestRange::BuildLighting()
{
	UWorld* World = GetWorld();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->FindComponentByClass<UDirectionalLightComponent>())
		{
			return; // The level already has a sun; leave its lighting alone.
		}
	}

	UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(this, TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-38.f, 40.f, 0.f));
	Sun->Intensity = 8.f;
	Sun->bAtmosphereSunLight = true;
	Sun->RegisterComponent();
	AddInstanceComponent(Sun);

	USkyAtmosphereComponent* Atmosphere = NewObject<USkyAtmosphereComponent>(this, TEXT("SkyAtmosphere"));
	Atmosphere->SetupAttachment(Root);
	Atmosphere->RegisterComponent();
	AddInstanceComponent(Atmosphere);

	USkyLightComponent* SkyLight = NewObject<USkyLightComponent>(this, TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->RegisterComponent();
	AddInstanceComponent(SkyLight);

	UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this, TEXT("HeightFog"));
	Fog->SetupAttachment(Root);
	Fog->FogDensity = 0.006f;
	Fog->FogHeightFalloff = 0.2f;
	Fog->RegisterComponent();
	AddInstanceComponent(Fog);
}

void AHeavyTestRange::BuildGeometry()
{
	using namespace RangeColors;

	// Checkered floor, 80 x 80 m in 10 m tiles. The pattern sells speed and momentum.
	constexpr int32 Tiles = 8;
	constexpr float TileSize = 1000.f;
	constexpr float Half = Tiles * TileSize * 0.5f;
	for (int32 X = 0; X < Tiles; ++X)
	{
		for (int32 Y = 0; Y < Tiles; ++Y)
		{
			const FVector Center(-Half + (X + 0.5f) * TileSize, -Half + (Y + 0.5f) * TileSize, -10.f);
			Block(Center, FVector(TileSize, TileSize, 20.f), ((X + Y) % 2 == 0) ? TileA : TileB);
		}
	}

	// Perimeter walls.
	constexpr float WallHeight = 450.f;
	Block(FVector(Half + 50.f, 0.f, WallHeight * 0.5f), FVector(100.f, Half * 2.f + 200.f, WallHeight), Wall);
	Block(FVector(-Half - 50.f, 0.f, WallHeight * 0.5f), FVector(100.f, Half * 2.f + 200.f, WallHeight), Wall);
	Block(FVector(0.f, Half + 50.f, WallHeight * 0.5f), FVector(Half * 2.f, 100.f, WallHeight), Wall);
	Block(FVector(0.f, -Half - 50.f, WallHeight * 0.5f), FVector(Half * 2.f, 100.f, WallHeight), Wall);

	// Shooting lane: distance lines at 6 / 12 / 20 / 30 m from the start, plus posts either side.
	const FVector Start = GetPlayerStartOffset();
	const TArray<float> Distances = { 600.f, 1200.f, 2000.f, 3000.f };
	for (int32 Index = 0; Index < Distances.Num(); ++Index)
	{
		const float X = static_cast<float>(Start.X) + Distances[Index];
		const FLinearColor& Color = (Index % 2 == 0) ? Marker : MarkerAlt;
		Block(FVector(X, 0.f, 0.5f), FVector(12.f, 1400.f, 1.f), Color);
		Block(FVector(X, -720.f, 140.f), FVector(14.f, 14.f, 280.f), Color);
		Block(FVector(X, 720.f, 140.f), FVector(14.f, 14.f, 280.f), Color);
	}

	// Right side: a cover course of low (crouch) and high (stand) walls and pillars.
	Block(FVector(-2000.f, 1600.f, 55.f), FVector(40.f, 420.f, 110.f), Cover);
	Block(FVector(-1400.f, 2050.f, 115.f), FVector(420.f, 40.f, 230.f), Cover);
	Block(FVector(-800.f, 1500.f, 150.f), FVector(70.f, 70.f, 300.f), Cover);
	Block(FVector(-800.f, 2600.f, 150.f), FVector(70.f, 70.f, 300.f), Cover);
	Block(FVector(0.f, 2050.f, 150.f), FVector(70.f, 70.f, 300.f), Cover);
	Block(FVector(-200.f, 2900.f, 55.f), FVector(320.f, 40.f, 110.f), Cover);
	Block(FVector(600.f, 1700.f, 55.f), FVector(40.f, 520.f, 110.f), Cover);
	Block(FVector(-1850.f, 2650.f, 60.f), FVector(120.f, 120.f, 120.f), CrateDark);
	Block(FVector(-1720.f, 2650.f, 60.f), FVector(120.f, 120.f, 120.f), CrateDark);
	Block(FVector(-1785.f, 2650.f, 180.f), FVector(120.f, 120.f, 120.f), CrateDark);

	// Left side: a 3 m platform with a ramp up and a 1.5 m step down. Drop off the edges
	// to feel the landings.
	Block(FVector(-1200.f, -2200.f, 150.f), FVector(800.f, 800.f, 300.f), Platform);
	Block(FVector(-500.f, -2200.f, 75.f), FVector(600.f, 600.f, 150.f), Platform);

	// Ramp: 9 m run, 3 m rise. The slab's top surface meets the floor and the platform top.
	const float Run = 900.f;
	const float Rise = 300.f;
	const float SlopeDegrees = FMath::RadiansToDegrees(FMath::Atan2(Rise, Run));
	const float Length = FMath::Sqrt(Run * Run + Rise * Rise);
	const float Thickness = 20.f;
	const float LiftCorrection = Thickness * 0.5f / FMath::Cos(FMath::DegreesToRadians(SlopeDegrees));
	Block(FVector(-1600.f - Run * 0.5f, -2200.f, Rise * 0.5f - LiftCorrection), FVector(Length, 400.f, Thickness), Platform,
		FRotator(SlopeDegrees, 0.f, 0.f));

	// A few low walls in no-man's-land to fight around.
	Block(FVector(800.f, -900.f, 55.f), FVector(300.f, 40.f, 110.f), Cover);
	Block(FVector(1600.f, 400.f, 115.f), FVector(40.f, 400.f, 230.f), Cover);
	Block(FVector(2400.f, -1200.f, 150.f), FVector(70.f, 70.f, 300.f), Cover);
}

void AHeavyTestRange::SpawnTargets()
{
	UWorld* World = GetWorld();
	const FTransform& RangeTransform = GetActorTransform();
	const FVector Start = GetPlayerStartOffset();

	struct FDummySpot
	{
		float Distance;
		float Y;
		bool bStrafe;
	};

	const FDummySpot Spots[] =
	{
		{ 600.f, -260.f, false }, { 600.f, 0.f, false }, { 600.f, 260.f, false },
		{ 1200.f, -220.f, false }, { 1200.f, 220.f, false },
		{ 2000.f, 0.f, false },
		{ 3000.f, -300.f, false }, { 3000.f, 300.f, false },
		{ 900.f, -950.f, true }, { 1600.f, 950.f, true },
	};

	// Face back down the lane towards the player.
	const FQuat Facing = RangeTransform.GetRotation() * FRotator(0.f, 180.f, 0.f).Quaternion();

	for (const FDummySpot& Spot : Spots)
	{
		const FVector Local(static_cast<float>(Start.X) + Spot.Distance, Spot.Y, 0.f);
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AHeavyTargetDummy* Dummy = World->SpawnActor<AHeavyTargetDummy>(AHeavyTargetDummy::StaticClass(),
			RangeTransform.TransformPosition(Local), Facing.Rotator(), Params);
		if (Dummy)
		{
			Dummy->bStrafe = Spot.bStrafe;
		}
	}
}

void AHeavyTestRange::SpawnProps()
{
	using namespace RangeColors;

	UWorld* World = GetWorld();
	const FTransform& RangeTransform = GetActorTransform();

	auto Spawn = [&](const FVector& Local, bool bBarrel, const FVector& Size, const FLinearColor& Color, float Mass)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AHeavyPhysicsProp* Prop = World->SpawnActor<AHeavyPhysicsProp>(AHeavyPhysicsProp::StaticClass(),
			RangeTransform.TransformPosition(Local), RangeTransform.Rotator(), Params);
		if (Prop)
		{
			Prop->Setup(bBarrel, Size, Color, Mass);
		}
	};

	// A pyramid of crates off to the right of the start.
	const FVector CrateSize(70.f);
	const FVector StackOrigin(-2050.f, 650.f, 0.f);
	const TArray<int32> Rows = { 3, 2, 1 };
	for (int32 Level = 0; Level < Rows.Num(); ++Level)
	{
		for (int32 Index = 0; Index < Rows[Level]; ++Index)
		{
			const float Y = (static_cast<float>(Index) - (Rows[Level] - 1) * 0.5f) * 72.f;
			const float Z = 35.5f + static_cast<float>(Level) * 71.f;
			Spawn(StackOrigin + FVector(0.f, Y, Z), false, CrateSize, (Index + Level) % 2 == 0 ? Crate : CrateDark, 35.f);
		}
	}

	// Barrels to the left.
	const FVector BarrelSize(60.f, 60.f, 90.f);
	const TArray<FVector> BarrelSpots =
	{
		FVector(-1950.f, -650.f, 46.f), FVector(-1950.f, -720.f, 46.f), FVector(-2020.f, -685.f, 46.f),
		FVector(-1700.f, -500.f, 46.f), FVector(-1500.f, -820.f, 46.f),
	};
	for (int32 Index = 0; Index < BarrelSpots.Num(); ++Index)
	{
		Spawn(BarrelSpots[Index], true, BarrelSize, Index % 2 == 0 ? BarrelBlue : BarrelRed, 60.f);
	}

	// Loose crates scattered in the lane at mid range.
	Spawn(FVector(-1700.f, 300.f, 36.f), false, CrateSize, Crate, 35.f);
	Spawn(FVector(-1150.f, -420.f, 36.f), false, CrateSize, CrateDark, 35.f);
}

// -----------------------------------------------------------------------------------------

AHeavyPhysicsProp::AHeavyPhysicsProp()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	Body = CreateDefaultSubobject<UHeavyPartComponent>(TEXT("Body"));
	Body->SetStaticMesh(UHeavyPartComponent::GetShapeMesh(EHeavyShape::Cube));
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCanEverAffectNavigation(false);
	Body->SetLinearDamping(0.15f);
	Body->SetAngularDamping(0.3f);
	RootComponent = Body;
}

void AHeavyPhysicsProp::Setup(bool bBarrel, const FVector& SizeCm, const FLinearColor& Color, float MassKg)
{
	Body->SetStaticMesh(UHeavyPartComponent::GetShapeMesh(bBarrel ? EHeavyShape::Cylinder : EHeavyShape::Cube));
	Body->SetWorldScale3D(SizeCm / 100.f);
	Body->SetPartColor(Color);
	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	Body->SetSimulatePhysics(true);
}
