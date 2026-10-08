#include "PFTarget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FLinearColor BodyColor(0.85f, 0.32f, 0.06f);
	const FLinearColor HeadColor(0.95f, 0.5f, 0.12f);
	const FLinearColor PostColor(0.12f, 0.12f, 0.13f);
	const FLinearColor RingColor(0.95f, 0.95f, 0.95f);
	const FLinearColor BullColor(0.7f, 0.02f, 0.02f);
	const FLinearColor BaseColor(0.2f, 0.2f, 0.22f);
	const FLinearColor MovingBaseColor(0.1f, 0.3f, 0.8f);
	const FLinearColor TargetFlashColor(1.f, 1.f, 1.f);

	constexpr float FallAngle = 85.f;
	constexpr float FlashTime = 0.07f;
	constexpr float TextTime = 0.9f;
}

APFTarget::APFTarget()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

void APFTarget::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	BuildVisuals();
	StartLocation = GetActorLocation();
	Health = MaxHealth;
}

UStaticMeshComponent* APFTarget::AddColoredPart(USceneComponent* Parent, EPFShape Shape, const FVector& Size, const FVector& Location, const FRotator& Rotation, const FLinearColor& Color)
{
	UStaticMeshComponent* Part = FPFShapes::AddPart(this, Parent, Shape, Size, Location, Rotation, Color, true);
	ColoredParts.Add(Part);
	PartColors.Add(Color);
	return Part;
}

void APFTarget::BuildVisuals()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;

	const FRotator FacingX(-90.f, 0.f, 0.f); // cylinder axis to +X (towards the shooter)

	BasePlate = FPFShapes::AddPart(this, Root, EPFShape::Cube, FVector(60.f, 60.f, 10.f), FVector(0.f, 0.f, 5.f), FRotator::ZeroRotator, BaseColor, true);

	// Everything above the base hinges at its top edge so the dummy can fall backwards
	Hinge = FPFShapes::AddJoint(this, Root, FVector(0.f, 0.f, 10.f));

	AddColoredPart(Hinge, EPFShape::Cylinder, FVector(8.f, 8.f, 50.f), FVector(0.f, 0.f, 25.f), FRotator::ZeroRotator, PostColor);
	AddColoredPart(Hinge, EPFShape::Cube, FVector(20.f, 46.f, 58.f), FVector(0.f, 0.f, 79.f), FRotator::ZeroRotator, BodyColor);
	AddColoredPart(Hinge, EPFShape::Cylinder, FVector(26.f, 26.f, 2.f), FVector(10.6f, 0.f, 84.f), FacingX, RingColor);
	AddColoredPart(Hinge, EPFShape::Cylinder, FVector(10.f, 10.f, 2.f), FVector(11.4f, 0.f, 84.f), FacingX, BullColor);
	AddColoredPart(Hinge, EPFShape::Cylinder, FVector(12.f, 12.f, 50.f), FVector(0.f, -30.f, 80.f), FRotator(0.f, 0.f, -8.f), BodyColor);
	AddColoredPart(Hinge, EPFShape::Cylinder, FVector(12.f, 12.f, 50.f), FVector(0.f, 30.f, 80.f), FRotator(0.f, 0.f, 8.f), BodyColor);
	AddColoredPart(Hinge, EPFShape::Cylinder, FVector(10.f, 10.f, 8.f), FVector(0.f, 0.f, 112.f), FRotator::ZeroRotator, PostColor);
	Head = AddColoredPart(Hinge, EPFShape::Sphere, FVector(28.f, 28.f, 28.f), FVector(0.f, 0.f, 128.f), FRotator::ZeroRotator, HeadColor);

	DamageText = NewObject<UTextRenderComponent>(this);
	DamageText->SetMobility(EComponentMobility::Movable);
	DamageText->SetupAttachment(Root);
	DamageText->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
	DamageText->SetHorizontalAlignment(EHTA_Center);
	DamageText->SetVerticalAlignment(EVRTA_TextCenter);
	DamageText->SetWorldSize(24.f);
	DamageText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageText->SetCastShadow(false);
	DamageText->SetVisibility(false);
	DamageText->RegisterComponent();
	AddInstanceComponent(DamageText);
}

void APFTarget::SetMovement(float Distance, float Speed)
{
	MoveDistance = Distance;
	MoveSpeed = Speed;
	if (BasePlate)
	{
		FPFShapes::SetColor(BasePlate, Distance > 0.f ? MovingBaseColor : BaseColor);
	}
}

void APFTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Fall over quickly, get back up a little slower
	FallAlpha = FMath::FInterpConstantTo(FallAlpha, bDown ? 1.f : 0.f, DeltaSeconds, bDown ? 5.f : 2.5f);
	if (Hinge)
	{
		Hinge->SetRelativeRotation(FRotator(FallAngle * FMath::SmoothStep(0.f, 1.f, FallAlpha), 0.f, 0.f));
	}

	if (bDown)
	{
		DownTimer -= DeltaSeconds;
		if (DownTimer <= 0.f)
		{
			ResetTarget();
		}
	}

	if (FlashTimer > 0.f)
	{
		FlashTimer -= DeltaSeconds;
		if (FlashTimer <= 0.f)
		{
			SetFlash(false);
		}
	}

	if (TextTimer > 0.f && DamageText)
	{
		TextTimer -= DeltaSeconds;
		if (TextTimer <= 0.f)
		{
			DamageText->SetVisibility(false);
			DamageShown = 0.f;
		}
		else if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			// Billboard the number towards the player and let it drift upwards
			const FVector ToCam = Cam->GetCameraLocation() - DamageText->GetComponentLocation();
			DamageText->SetWorldRotation(FRotator(0.f, ToCam.Rotation().Yaw, 0.f));
			DamageText->SetRelativeLocation(FVector(0.f, 0.f, 180.f + (TextTime - TextTimer) * 30.f));
		}
	}

	if (MoveDistance > 0.f && !bDown)
	{
		MoveTime += DeltaSeconds;
		const float Offset = MoveDistance * FMath::Sin(MoveTime * MoveSpeed * 2.f * UE_PI);
		SetActorLocation(StartLocation + GetActorRightVector() * Offset);
	}
}

float APFTarget::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDown)
	{
		return 0.f;
	}

	float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	bool bHead = false;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		if (PointEvent.HitInfo.GetComponent() == Head)
		{
			Damage *= HeadshotMultiplier;
			bHead = true;
		}
	}

	if (Damage <= 0.f)
	{
		return 0.f;
	}

	Health -= Damage;
	FlashTimer = FlashTime;
	SetFlash(true);

	if (Health <= 0.f)
	{
		bDown = true;
		DownTimer = ResetDelay;
	}

	if (DamageText)
	{
		DamageShown += Damage;
		const int32 Shown = FMath::RoundToInt(DamageShown);
		FString Label = FString::Printf(TEXT("%d"), Shown);
		FColor Color = FColor::White;
		if (bDown)
		{
			Label += TEXT("  DOWN");
			Color = FColor(255, 70, 50);
		}
		else if (bHead)
		{
			Color = FColor(255, 220, 60);
		}
		DamageText->SetText(FText::FromString(Label));
		DamageText->SetTextRenderColor(Color);
		DamageText->SetVisibility(true);
		TextTimer = TextTime;
	}

	return Damage;
}

void APFTarget::ResetTarget()
{
	bDown = false;
	Health = MaxHealth;
	DownTimer = 0.f;
}

void APFTarget::SetFlash(bool bOn)
{
	for (int32 i = 0; i < ColoredParts.Num(); ++i)
	{
		FPFShapes::SetColor(ColoredParts[i], bOn ? TargetFlashColor : PartColors[i]);
	}
}
