#!/usr/bin/env bash
# Builds FacialHairPlacement.dll for every supported RimWorld version into ../<version>/Assemblies.
set -euo pipefail
cd "$(dirname "$0")"
for version in 1.5 1.6; do
  dotnet build FacialHairPlacement/FacialHairPlacement.csproj -c Release -p:RimWorldVersion="$version"
done
