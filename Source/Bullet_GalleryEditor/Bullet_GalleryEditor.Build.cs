using System.IO;
using UnrealBuildTool;

/** Editor-only helpers of the project: builds the guard's Behavior Tree and Blackboard assets (Lab 5) */
public class Bullet_GalleryEditor : ModuleRules
{
	public Bullet_GalleryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Bullet_Gallery",
			"AIModule",
			"AIGraph",
			"BehaviorTreeEditor",
			"GraphEditor",
			"UnrealEd",
			"AssetRegistry",
			"Kismet"
		});

		// The game module keeps its headers next to the sources (no Public folder)
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "Bullet_Gallery"));
	}
}
