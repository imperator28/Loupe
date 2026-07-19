# CAD X-Ray Measurement Highlight Design

## Goal

Make face selection unmistakable during measurement, including while sectioning is active or other bodies occlude the selected face. The interaction should feel familiar to NX and Creo users: preselection confirms the selectable entity, and committed picks remain visually distinct.

## Scope

This amendment changes face highlighting for topology-aware measurement modes. It does not change component selection, edge highlighting, section geometry, measurement calculations, or the general viewport palette.

## Interaction

- A valid face under the pointer receives a teal preselection highlight.
- A committed measurement face receives an amber selection highlight.
- Both treatments show the complete CAD face through bodies and section caps.
- The fill is translucent enough to preserve model context, while the face boundary remains opaque and crisp.
- Committed picks retain their numbered viewport marker.
- Invalid topology for the active measurement mode is not highlighted.
- Highlights are non-pickable and remain attached during orbit, pan, zoom, projection changes, and section movement.
- At most one hover face and two committed faces are rendered.

## Rendering

Face highlights use a dedicated x-ray material path with depth testing and depth writing disabled. The hover and committed layers use separate theme-aware colors and strengths. Rendering does not alter the source body material or section material.

Each highlighted face has two representations:

1. A filled copy of the selected face triangles.
2. A boundary-line copy derived from the selected face topology. Shared tessellation edges inside the face are discarded, leaving only the outer CAD-face boundary.

The highlight represents the full selected face and intentionally does not clip at the active section plane. This preserves entity-level selection feedback even where the source face is occluded or lies behind a section cap.

## Data Flow

1. The viewport pick resolves the hit to a topology ID using the active measurement filter.
2. Hover state copies the matching face and its boundary into reusable highlight geometries.
3. A successful measurement pick stores the source geometry and topology ID in the existing accepted-topology model.
4. The committed highlight delegates copy the same face and boundary and render them with the selected color.
5. Clearing or changing measurement mode removes hover and committed highlight geometry.

The source geometry remains authoritative; highlight geometry never participates in picking or measurement calculation.

## Failure Handling

- If topology lookup or face copying fails, clear the hover highlight and leave measurement state unchanged.
- If boundary extraction produces no lines, retain the filled face highlight rather than dropping all feedback.
- Empty or stale accepted topology references are ignored safely when the scene is replaced.

## Performance

Boundary extraction is linear in the selected face's triangle count and occurs only when the hovered topology changes or a pick is committed. Reusing hover geometry avoids allocations on ordinary pointer movement within the same face. The maximum of three highlighted faces bounds GPU and memory cost independently of assembly size.

## Testing

- Unit-test boundary extraction so internal tessellation edges are excluded.
- Unit-test copying a face and its boundary from a topology ID.
- QML-test hover and committed face layers for distinct colors, non-pickable behavior, and x-ray material use.
- Verify section mode does not clip or occlude either highlight state.
- Manually review dark and light themes with front-facing, edge-on, sectioned, and fully occluded faces.

