void MAIN()
{
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
    // Edge polylines are coincident with the shaded face they outline, which
    // z-fights unpredictably. A world-space nudge toward the camera cannot fix
    // this: the same world offset yields a different depth-buffer separation
    // depending on view angle and distance, so edges drop out at some
    // rotations and reappear at others. Biasing clip-space depth directly
    // (scaled by POSITION.w so it is uniform after the perspective divide)
    // gives a constant depth-buffer separation regardless of view, while
    // still depth-testing normally so genuinely hidden edges stay hidden.
    POSITION.z -= depthBiasAmount * POSITION.w;
}
