using UnrealBuildTool;
using System.Collections.Generic;

public class HeavyShooterTarget : TargetRules
{
	public HeavyShooterTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HeavyShooter");
	}
}
