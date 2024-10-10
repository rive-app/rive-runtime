// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class GMLibrary : ModuleRules
{
	public GMLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		PublicSystemIncludePaths.Add("$(ModuleDir)/Public");

		PublicDependencyModuleNames.Add("RiveLibrary");

		PublicDefinitions.Add("RIVE_UNREAL");
		PublicDefinitions.Add("RIVE_UNREAL_IMPORT");
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Add the import library
			PublicAdditionalLibraries.Add("/WHOLEARCHIVE:" + Path.Combine(ModuleDirectory, "x64", "Release", "gms.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "x64", "Release", "goldens.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "x64", "Release", "tools_common.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDelayLoadDLLs.Add(Path.Combine(ModuleDirectory, "Mac", "Release", "gms.dylib"));
			RuntimeDependencies.Add("$(PluginDir)/Source/ThirdParty/GMLibrary/Mac/Release/gms.dylib");
			
			PublicDelayLoadDLLs.Add(Path.Combine(ModuleDirectory, "Mac", "Release", "goldens.dylib"));
			RuntimeDependencies.Add("$(PluginDir)/Source/ThirdParty/GMLibrary/Mac/Release/goldens.dylib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string ExampleSoPath = Path.Combine("$(PluginDir)", "Binaries", "ThirdParty", "GMLibrary", "Linux", "x86_64-unknown-linux-gnu", "gms.so");
			PublicAdditionalLibraries.Add(ExampleSoPath);
			PublicDelayLoadDLLs.Add(ExampleSoPath);
			RuntimeDependencies.Add(ExampleSoPath);
		}
	}
}
