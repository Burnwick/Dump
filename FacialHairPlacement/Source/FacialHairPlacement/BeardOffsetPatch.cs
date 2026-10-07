using HarmonyLib;
using UnityEngine;
using Verse;

namespace FacialHairPlacement
{
    // PawnRenderNodeWorker_Beard.OffsetFor positions the beard node relative to the head.
    // Adding to its result moves facial hair everywhere it is drawn: map, portraits and the colonist bar.
    [HarmonyPatch(typeof(PawnRenderNodeWorker_Beard), nameof(PawnRenderNodeWorker_Beard.OffsetFor))]
    public static class BeardOffsetPatch
    {
        public static void Postfix(PawnDrawParms parms, ref Vector3 __result)
        {
            FacialHairPlacementSettings settings = FacialHairPlacementMod.Settings;
            if (settings != null && settings.enabled)
            {
                __result += settings.OffsetFor(parms.facing);
            }
        }
    }
}
