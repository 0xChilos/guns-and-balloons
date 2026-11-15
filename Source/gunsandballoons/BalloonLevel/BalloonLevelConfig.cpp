#include "BalloonLevelConfig.h"

bool UBalloonLevelConfig::Validate(TArray<FString>& Errors) const
{
	const int32 Before = Errors.Num();
 for (float Value : {EnemyDensity, ObstacleDensity})
  if (!FMath::IsFinite(Value) || Value < 0.f || Value > 10.f) { Errors.Add(TEXT("Placement densities must be finite and in [0,10]")); break; }
 if (TerrainMode != EBalloonTerrainMode::Flat && TerrainMode != EBalloonTerrainMode::WorldCollision && TerrainMode != EBalloonTerrainMode::Generated) Errors.Add(TEXT("Invalid TerrainMode"));
 if (!FMath::IsFinite(TerrainHeightVariationMeters) || TerrainHeightVariationMeters < 0.f ||
     !FMath::IsFinite(TerrainWavelengthMeters) || TerrainWavelengthMeters < 1.f ||
     !FMath::IsFinite(TerrainCellMeters) || TerrainCellMeters < 1.f ||
     !FMath::IsFinite(TerrainPaddingMeters) || TerrainPaddingMeters < 0.f)
  Errors.Add(TEXT("Terrain requires finite nonnegative height/padding and wavelength/cell size >= 1m"));
 if (!FMath::IsFinite(PlacementGroundZMeters)) Errors.Add(TEXT("PlacementGroundZMeters must be finite"));
 if (TerrainMode == EBalloonTerrainMode::WorldCollision)
 {
  if (TerrainActorTag.IsNone()) Errors.Add(TEXT("WorldCollision terrain requires TerrainActorTag"));
  if (!FMath::IsFinite(TerrainTraceTopMeters) || !FMath::IsFinite(TerrainTraceBottomMeters) || TerrainTraceTopMeters <= TerrainTraceBottomMeters)
   Errors.Add(TEXT("Terrain trace bounds must be finite with top above bottom"));
 }
 const float Nonnegative[] = {BorderDensityPerKm, BorderRadiusMinMeters, BorderRadiusMaxMeters, BorderMinSpacingMeters, BalloonRadiusMeters, BalloonHalfHeightMeters};
 for (float Value : Nonnegative) if (!FMath::IsFinite(Value) || Value < 0.f) { Errors.Add(TEXT("Border density/radii/spacing/envelope must be finite and nonnegative")); break; }
 if (BorderExactCount < -1 || BorderExactCount > 10000) Errors.Add(TEXT("BorderExactCount must be -1 or [0,10000]"));
 if (BorderRadiusMinMeters <= 0.f || BorderRadiusMaxMeters < BorderRadiusMinMeters) Errors.Add(TEXT("Border radii must be positive and ordered"));
 if (!FMath::IsFinite(BorderIntrusionFraction) || BorderIntrusionFraction < 0.f || BorderIntrusionFraction > 1.f) Errors.Add(TEXT("BorderIntrusionFraction must be [0,1]"));
 if (!FMath::IsFinite(BorderHeightOffsetMeters)) Errors.Add(TEXT("BorderHeightOffsetMeters must be finite"));
 if (static_cast<uint8>(BorderPolicy) > static_cast<uint8>(EBalloonBorderInfluencePolicy::VisualOnly)) Errors.Add(TEXT("Invalid BorderPolicy"));
	if (!FMath::IsFinite(LengthMeters) || LengthMeters <= 0.f) Errors.Add(TEXT("LengthMeters must be finite and positive"));
	if (!FMath::IsFinite(BaseWidthMeters) || BaseWidthMeters <= 0.f) Errors.Add(TEXT("BaseWidthMeters must be finite and positive"));
	if (!FMath::IsFinite(SegmentTargetMeters) || SegmentTargetMeters <= 0.f) Errors.Add(TEXT("SegmentTargetMeters must be finite and positive"));
	if (!FMath::IsFinite(FlightHeightMeters)) Errors.Add(TEXT("FlightHeightMeters must be finite"));
	if (StartTransform.ContainsNaN()) Errors.Add(TEXT("StartTransform must be finite"));
	if (BiomeId.IsNone()) Errors.Add(TEXT("BiomeId must be nonempty"));
	if (Preset != EBalloonLightingPreset::Day && Preset != EBalloonLightingPreset::Night) Errors.Add(TEXT("Preset must be Day or Night"));
	if (!FMath::IsFinite(MaxHeadingDegrees) || MaxHeadingDegrees < 0.f || MaxHeadingDegrees > 45.f) Errors.Add(TEXT("MaxHeadingDegrees must be in [0,45]"));
	if (!FMath::IsFinite(MinimumTurningRadiusMeters) || MinimumTurningRadiusMeters <= 0.f) Errors.Add(TEXT("MinimumTurningRadiusMeters must be positive"));
	if (!FMath::IsFinite(GeometrySampleMeters) || GeometrySampleMeters <= 0.f) Errors.Add(TEXT("GeometrySampleMeters must be positive"));
	if (!FMath::IsFinite(MinWidthMeters) || MinWidthMeters <= 0.f) Errors.Add(TEXT("MinWidthMeters must be positive"));
	if (GeometrySampleMeters > 0.f && FMath::CeilToDouble(double(LengthMeters) / GeometrySampleMeters) > 100000.) Errors.Add(TEXT("Geometry sample limit exceeded (100000)"));
	for (const FRichCurveKey& Key : WidthProfile.GetRichCurveConst()->GetConstRefOfKeys())
	{
		if (!FMath::IsFinite(Key.Time) || !FMath::IsFinite(Key.Value) || !FMath::IsFinite(Key.ArriveTangent) || !FMath::IsFinite(Key.LeaveTangent) ||
			!FMath::IsFinite(Key.ArriveTangentWeight) || !FMath::IsFinite(Key.LeaveTangentWeight))
		{ Errors.Add(TEXT("WidthProfile keys and tangents must be finite")); break; }
	}
	// Explicit allocation guard rather than truncating an authored level.
	if (Errors.Num() == Before && FMath::CeilToDouble(double(LengthMeters) / SegmentTargetMeters) > 100000.)
		Errors.Add(TEXT("Segment count exceeds M1 planning limit (100000); increase SegmentTargetMeters"));
	return Errors.Num() == Before;
}

bool BalloonLevelPlanning::BuildStraightPlan(const UBalloonLevelConfig& Config, FBalloonLevelPlan& Plan, TArray<FString>& Errors)
{
	Plan = FBalloonLevelPlan();
	if (!Config.Validate(Errors)) return false;
	Plan.Seed = Config.Seed;
	Plan.BiomeId = Config.BiomeId;
	Plan.CatalogVersion = Config.CatalogVersion;
	Plan.LightingPreset = Config.Preset;
	const double Length = double(Config.LengthMeters) * 100.;
	const double Target = double(Config.SegmentTargetMeters) * 100.;
	const double HalfWidth = double(Config.BaseWidthMeters) * 50.;
	const double Yaw = Config.StartTransform.Rotator().Yaw;
	const FVector Forward = FRotator(0., Yaw, 0.).Vector();
	const FVector Right(-Forward.Y, Forward.X, 0.);
	const FVector Origin(Config.StartTransform.GetLocation().X, Config.StartTransform.GetLocation().Y, double(Config.FlightHeightMeters) * 100.);
	const int32 Count = int32(FMath::CeilToDouble(Length / Target));
	Plan.SplineSamples.Add(Origin);
	for (int32 I = 0; I < Count; ++I)
	{
		FBalloonLevelSegment Segment;
		Segment.SegmentId = I;
		Segment.StartS = I * Target;
		Segment.EndS = FMath::Min((I + 1) * Target, Length);
		const FVector A = Origin + Forward * Segment.StartS;
		const FVector B = Origin + Forward * Segment.EndS;
		Segment.Samples = {A, B};
		for (const FVector& P : Segment.Samples)
		{
			Segment.BaseBounds += P - Right * HalfWidth;
			Segment.BaseBounds += P + Right * HalfWidth;
		}
		Plan.SplineSamples.Add(B);
		Plan.Segments.Add(MoveTemp(Segment));
	}
	return true;
}
