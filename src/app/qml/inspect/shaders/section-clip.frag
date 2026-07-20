void MAIN()
{
    float signedDistance = dot(VAR_WORLD_POSITION, sectionNormal) - sectionOffset;
    if (clipEnabled && retainedSign * signedDistance < 0.0)
        discard;

    BASE_COLOR = vec4(baseColor.rgb, baseColor.a);
    EMISSIVE_COLOR = emissiveFactor;
    ROUGHNESS = roughnessValue;
}
