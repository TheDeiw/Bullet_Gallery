// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Bullet_GalleryEditorTarget : TargetRules
{
	public Bullet_GalleryEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Bullet_Gallery");
		ExtraModuleNames.Add("Bullet_GalleryEditor");
	}
}
