#include "HeavyTargetDummy.h"

#include "HeavyPartComponent.h"

#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"

namespace
{
	const FLinearColor DummySteel(0.08f, 0.085f, 0.09f);
	const FLinearColor RingWhite(0.85f, 0.85f, 0.82f);
	const FLinearColor RingRed(0.6f, 0.06f, 0.05f);
	const FLinearColor HitColor(1.0f, 0.3f, 0.15f);
	const FLinearColor HeadHitColor(1.0f, 0.85f, 0.2f);

	void MakeSolid(UHeavyPartComponent* Part)
	{
		Part->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

AHeavyTargetDummy::AHeavyTargetDummy()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(true);

	using P = UHeavyPartComponent;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Base = P::CreatePart(this, TEXT("Base"), Root, EHeavyShape::Cylinder, FVector(0.f, 0.f, 4.f), FVector(70.f, 70.f, 8.f), FRotator::ZeroRotator, DummySteel);
	MakeSolid(Base);

	// Everything above the base hinges on Pivot so hits rock it and kills knock it flat.
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, 8.f));

	Post = P::CreatePart(this, TEXT("Post"), Pivot, EHeavyShape::Cylinder, FVector(0.f, 0.f, 37.f), FVector(8.f, 8.f, 74.f), FRotator::ZeroRotator, DummySteel);
	Torso = P::CreatePart(this, TEXT("Torso"), Pivot, EHeavyShape::Cube, FVector(0.f, 0.f, 105.f), FVector(28.f, 46.f, 62.f), FRotator::ZeroRotator, BodyColor);
	Head = P::CreatePart(this, TEXT("Head"), Pivot, EHeavyShape::Sphere, FVector(0.f, 0.f, 154.f), FVector(26.f, 26.f, 28.f), FRotator::ZeroRotator, BodyColor);
	MakeSolid(Post);
	MakeSolid(Torso);
	MakeSolid(Head);

	// Bullseye on the chest.
	P::CreateRodX(this, TEXT("RingOuter"), Pivot, FVector(14.5f, 0.f, 110.f), 24.f, 1.0f, RingWhite);
	P::CreateRodX(this, TEXT("RingInner"), Pivot, FVector(15.0f, 0.f, 110.f), 10.f, 1.2f, RingRed);
}

void AHeavyTargetDummy::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	HomeLocation = GetActorLocation();
	StrafePhase = FMath::FRandRange(0.f, 2.f * PI);
	RefreshColors();
}

FVector AHeavyTargetDummy::GetHealthBarLocation() const
{
	return Head->GetComponentLocation() + FVector(0.f, 0.f, 36.f);
}

float AHeavyTargetDummy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDown || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	float Damage = DamageAmount;
	FVector ShotDirection = GetActorForwardVector() * -1.f;
	bool bHeadshot = false;

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		ShotDirection = PointEvent.ShotDirection;
		bHeadshot = PointEvent.HitInfo.GetComponent() == Head.Get();
		if (bHeadshot)
		{
			Damage *= HeadshotMultiplier;
		}
	}

	Damage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	Damage = FMath::Min(Damage, Health);
	Health -= Damage;

	// Rock away from the shot. Local +X is the dummy's front, so a shot travelling
	// along -X (from the front) pushes the top backwards.
	const FVector LocalShot = GetActorTransform().InverseTransformVectorNoScale(ShotDirection);
	TiltForward.AddImpulse(static_cast<float>(LocalShot.X) * Damage * HitRock);
	TiltRight.AddImpulse(static_cast<float>(LocalShot.Y) * Damage * HitRock);

	HitFlash = 1.f;
	bHeadFlash = bHeadshot;

	if (Health <= 0.f)
	{
		bDown = true;
		DownTime = 0.f;
		FallDirection = FVector2D(LocalShot.X, LocalShot.Y).GetSafeNormal();
		if (FallDirection.IsNearlyZero())
		{
			FallDirection = FVector2D(-1.f, 0.f);
		}
	}

	return Damage;
}

void AHeavyTargetDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDown)
	{
		DownTime += DeltaSeconds;
		if (DownTime >= RespawnDelay)
		{
			bDown = false;
			Health = MaxHealth;
		}
	}

	// Lying flat is ~85 degrees towards the shot; standing is upright. The low damping
	// on the way down gives a heavy slam and bounce.
	const float DownAngle = bDown ? 85.f : 0.f;
	const float Frequency = bDown ? 1.5f : 2.2f;
	const float Damping = bDown ? 0.55f : 0.32f;
	TiltForward.Update(static_cast<float>(FallDirection.X) * DownAngle, DeltaSeconds, Frequency, Damping);
	TiltRight.Update(static_cast<float>(FallDirection.Y) * DownAngle, DeltaSeconds, Frequency, Damping);
	TiltForward.Value = FMath::Clamp(TiltForward.Value, -88.f, 88.f);
	TiltRight.Value = FMath::Clamp(TiltRight.Value, -88.f, 88.f);

	// Negative pitch tips the top towards +X, positive roll tips it towards +Y.
	Pivot->SetRelativeRotation(FRotator(-TiltForward.Value, 0.f, TiltRight.Value));

	if (bStrafe && !bDown && StrafeDistance > 1.f)
	{
		StrafePhase += DeltaSeconds * (StrafeSpeed / StrafeDistance);
		const float Offset = FMath::Sin(StrafePhase) * StrafeDistance;
		SetActorLocation(HomeLocation + GetActorRightVector() * Offset);
	}

	if (HitFlash > 0.f)
	{
		HitFlash = FMath::Max(HitFlash - DeltaSeconds * 5.f, 0.f);
		RefreshColors();
	}
}

void AHeavyTargetDummy::RefreshColors()
{
	const FLinearColor Flash = bHeadFlash ? HeadHitColor : HitColor;
	const FLinearColor Body = FMath::Lerp(BodyColor, Flash, HitFlash);
	Torso->SetPartColor(Body);
	Head->SetPartColor(Body);
}
