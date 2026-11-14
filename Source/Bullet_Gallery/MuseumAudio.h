#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundAttenuation.h"

/**
 * Default 3D falloff for the NPC sounds (guard shots and alerts, statue alerts): full volume inside InnerRadius,
 * fading out linearly to silence over the next Falloff cm. Used when no SoundAttenuation asset is assigned.
 */
inline USoundAttenuation* NewNpcSoundAttenuation(UObject* Outer, float InnerRadius = 600.f, float Falloff = 3000.f)
{
	USoundAttenuation* Result = NewObject<USoundAttenuation>(Outer);
	Result->Attenuation.bAttenuate = true;
	Result->Attenuation.bSpatialize = true;
	Result->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Result->Attenuation.AttenuationShapeExtents = FVector(InnerRadius, 0.f, 0.f);
	Result->Attenuation.FalloffDistance = Falloff;
	Result->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
	return Result;
}
