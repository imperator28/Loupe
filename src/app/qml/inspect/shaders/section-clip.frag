void MAIN()
{
    float signedDistance = dot(VAR_WORLD_POSITION, sectionNormal) - sectionOffset;
    if (clipEnabled && retainedSign * signedDistance < 0.0)
        discard;

    BASE_COLOR = vec4(baseColor.rgb, baseColor.a * ghostOpacity);
    EMISSIVE_COLOR = emissiveFactor;
    ROUGHNESS = roughnessValue;
    // A low F0 (about 0.005 rather than plastic's 0.04). With the key light riding on the
    // camera, a face pointed at the viewer sits at the peak of the specular lobe, and in an
    // orthographic view the whole face does: at the default it rendered near white and
    // read nothing like the colour picked for it. SPECULAR_AMOUNT does not reach this path.
    IOR = 1.15;
}
