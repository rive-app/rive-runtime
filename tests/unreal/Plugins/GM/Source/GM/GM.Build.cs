// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class GM : ModuleRules
{
	public GM(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ... 
			}
			);

		var fullPluginPath =  Path.GetFullPath(Path.Combine(PluginDirectory, "../")); 
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				Path.Combine(fullPluginPath, "Rive", "Source", "RiveRenderer", "Private")
			});
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Projects",
				"RHI",
				"RenderCore",
				"Rive",
				"RiveLibrary",
				"RiveRenderer", 
				"Engine",
				"GMLibrary",
				// ... add other public dependencies that you statically link with here ...
			}
			);
		
		#if UE_UE_5_0_OR_LATER
		PublicDependencyModuleNames.Add("RHICore");
		#else
		#endif
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...	
            }
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
