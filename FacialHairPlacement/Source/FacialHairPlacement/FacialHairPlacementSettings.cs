using UnityEngine;
using Verse;

namespace FacialHairPlacement
{
    public class FacialHairPlacementSettings : ModSettings
    {
        // Slider range, in map units (1.0 = one tile).
        public const float MaxOffset = 0.3f;

        public bool enabled = true;

        // X is screen-horizontal, Y is screen-vertical.
        public float southX;
        public float southY;

        // Side X is "forward" (toward the face). East uses it as-is; west mirrors it.
        public float sideX;
        public float sideY;

        public float northX;
        public float northY;

        // RimWorld draws on the XZ plane: world Z is screen-up, world Y is draw altitude.
        // This is called from the render tree, possibly off the main thread, so it only reads fields.
        public Vector3 OffsetFor(Rot4 facing)
        {
            switch (facing.AsInt)
            {
                case Rot4.NorthInt:
                    return new Vector3(northX, 0f, northY);
                case Rot4.EastInt:
                    return new Vector3(sideX, 0f, sideY);
                case Rot4.WestInt:
                    return new Vector3(-sideX, 0f, sideY);
                default:
                    return new Vector3(southX, 0f, southY);
            }
        }

        public void ResetOffsets()
        {
            southX = southY = 0f;
            sideX = sideY = 0f;
            northX = northY = 0f;
        }

        public override void ExposeData()
        {
            base.ExposeData();
            Scribe_Values.Look(ref enabled, "enabled", true);
            Scribe_Values.Look(ref southX, "southX");
            Scribe_Values.Look(ref southY, "southY");
            Scribe_Values.Look(ref sideX, "sideX");
            Scribe_Values.Look(ref sideY, "sideY");
            Scribe_Values.Look(ref northX, "northX");
            Scribe_Values.Look(ref northY, "northY");
        }
    }
}
