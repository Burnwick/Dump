#pragma once

#include "CoreMinimal.h"

/**
 * Small float-only math helpers. Kept separate from FMath so mixed float/double
 * (LWC) arguments never hit template ambiguity.
 */
namespace HeavyMath
{
	FORCEINLINE float Lerp(float A, float B, float Alpha) { return A + (B - A) * Alpha; }
	FORCEINLINE float Clamp01(float V) { return V < 0.f ? 0.f : (V > 1.f ? 1.f : V); }

	/** Remaps Value from [InMin, InMax] to [OutMin, OutMax], clamped. */
	FORCEINLINE float MapClamped(float InMin, float InMax, float OutMin, float OutMax, float Value)
	{
		const float Range = InMax - InMin;
		const float T = FMath::IsNearlyZero(Range) ? 0.f : Clamp01((Value - InMin) / Range);
		return Lerp(OutMin, OutMax, T);
	}

	/** Smoothstep ease, 0..1 -> 0..1. */
	FORCEINLINE float Ease(float T)
	{
		T = Clamp01(T);
		return T * T * (3.f - 2.f * T);
	}

	/** Signed shortest delta from A to B in degrees. */
	FORCEINLINE float DeltaAngle(float A, float B)
	{
		float D = FMath::Fmod(B - A, 360.f);
		if (D > 180.f) { D -= 360.f; }
		else if (D < -180.f) { D += 360.f; }
		return D;
	}

	FORCEINLINE float NormalizeAngle(float A)
	{
		return DeltaAngle(0.f, A);
	}
}

/**
 * A damped harmonic oscillator. This is the core of the "heavy" feel: every bit of
 * motion (body turn, weapon aim, recoil, sway, camera punch) is a spring chasing a
 * target, so things have mass, carry momentum and settle instead of snapping.
 *
 *  Frequency    - natural frequency in Hz. Lower = heavier / more lag.
 *  DampingRatio - < 1 overshoots and wobbles, 1 is critical, > 1 is sluggish.
 */
struct FHeavySpring
{
	float Value = 0.f;
	float Velocity = 0.f;

	void Reset(float InValue)
	{
		Value = InValue;
		Velocity = 0.f;
	}

	float Update(float Target, float DeltaTime, float Frequency, float DampingRatio)
	{
		if (DeltaTime <= 0.f)
		{
			return Value;
		}

		const float Omega = 2.f * PI * FMath::Max(Frequency, 0.01f);
		const float Stiffness = Omega * Omega;
		const float Damping = 2.f * DampingRatio * Omega;

		// Sub-step so the springs behave the same at 30 fps and 240 fps.
		constexpr float MaxStep = 1.f / 240.f;
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaTime / MaxStep), 1, 32);
		const float H = DeltaTime / static_cast<float>(Steps);

		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float Accel = Stiffness * (Target - Value) - Damping * Velocity;
			Velocity += Accel * H;
			Value += Velocity * H;
		}
		return Value;
	}

	/** Same as Update, but for angles in degrees (takes the shortest way round). */
	float UpdateAngle(float TargetDegrees, float DeltaTime, float Frequency, float DampingRatio)
	{
		const float UnwoundTarget = Value + HeavyMath::DeltaAngle(Value, TargetDegrees);
		Update(UnwoundTarget, DeltaTime, Frequency, DampingRatio);
		const float Normalized = HeavyMath::NormalizeAngle(Value);
		Value = Normalized;
		return Value;
	}

	/** Never let the angle trail its target by more than MaxLag degrees. */
	void ClampAngleLag(float TargetDegrees, float MaxLag)
	{
		const float Lag = HeavyMath::DeltaAngle(Value, TargetDegrees);
		if (FMath::Abs(Lag) > MaxLag)
		{
			Value = HeavyMath::NormalizeAngle(TargetDegrees - FMath::Sign(Lag) * MaxLag);
		}
	}

	/** Distance-only variant for non-angular values. */
	void ClampLag(float Target, float MaxLag)
	{
		const float Lag = Target - Value;
		if (FMath::Abs(Lag) > MaxLag)
		{
			Value = Target - FMath::Sign(Lag) * MaxLag;
		}
	}

	void AddImpulse(float DeltaVelocity) { Velocity += DeltaVelocity; }
};

/** Three independent springs sharing a tuning. */
struct FHeavySpringVector
{
	FHeavySpring X;
	FHeavySpring Y;
	FHeavySpring Z;

	FVector Update(const FVector& Target, float DeltaTime, float Frequency, float DampingRatio)
	{
		X.Update(static_cast<float>(Target.X), DeltaTime, Frequency, DampingRatio);
		Y.Update(static_cast<float>(Target.Y), DeltaTime, Frequency, DampingRatio);
		Z.Update(static_cast<float>(Target.Z), DeltaTime, Frequency, DampingRatio);
		return Get();
	}

	FVector Get() const { return FVector(X.Value, Y.Value, Z.Value); }

	void AddImpulse(const FVector& DeltaVelocity)
	{
		X.Velocity += static_cast<float>(DeltaVelocity.X);
		Y.Velocity += static_cast<float>(DeltaVelocity.Y);
		Z.Velocity += static_cast<float>(DeltaVelocity.Z);
	}

	void Reset(const FVector& InValue)
	{
		X.Reset(static_cast<float>(InValue.X));
		Y.Reset(static_cast<float>(InValue.Y));
		Z.Reset(static_cast<float>(InValue.Z));
	}
};
