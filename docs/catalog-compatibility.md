# Catalog Compatibility Audit

This is a finite audit of the existing parameter families, not a claim
of complete ParAnimator support. Reference source revision:
`d2ebbe5aada17d3e4be5adad85582cc67a60b761`.

Source contracts are in `ParFile/ParameterCatalog.cpp`,
`ParFile/ResolvedAnimation.cpp`, `ParFile/Interpolant.cpp`, and
`ParFile/Interpolator.cpp`. Evidence below lives under
`tests/test-timeline-paranimator`.

## Covered Contracts

- Scalar and integer policies: `IntegerOutput` and `Extrapolation` cover
  linear, hold, step, clamp, base, omit, cycle, and ping-pong. Integer
  output rounds half away from zero after interpolation/extrapolation.
  Authored fractional values remain distinct from parameter output.
  Bounds and integral/int-range keys are checked before output rules.
- Tuple component metadata: `CatalogCompatibility` covers point2's
  implicit arity, explicit vector3 arity, generic numeric-tuple arity,
  raw bounds, catalog defaults, and destination-key overrides. Integer
  tuples retain their existing arity, bounds, and rounding coverage.
  Each numeric key retains component index, arity, and owned catalog and
  track definitions through copying, inspection, comparison, and layout.
  Existing `AnimationImport` paths cover point3/vector2 aliases and
  invalid declared arities through the shared arity validator.
- Specialized output aliases: `Id3DView` and `JulibrotView` cover catalog
  output names, types, integer outputs, layer sources, unused inputs,
  and explicit overrides against reference-generated goldens.
  `CameraImport` preserves source center-mag/corners output semantics;
  function-slot PWM resolves catalog fractal-type metadata. These
  synthesized nested targets do not use ordinary-parameter validation.
- Normalized keyed vectors: `NormalizedVector` covers vector2/vector3
  numeric arrays, arity, raw bounds, linear/hold/step curves, and
  normalization after interpolation. Keyed vectors do not clean tiny
  components. Points ignore normalize; raw vectors remain unchanged.
- Normalized control-point vectors: the Bezier and Catmull-Rom goldens
  cover arity, source cleanup below `1e-12`, normalization after path
  evaluation, and final cleanup. Source control-point evaluators do not
  apply catalog min/max; timeline curve bounds are display bounds, not
  a substitute catalog constraint. Continuous validation additionally
  rejects singular or unresolved intervals between frame samples.

`catalog-audit.json` generates nine lanes. Its five-frame reference PAR
is `gold-catalog-audit.par`; tests compare all 45 component outputs.
At frame 1, position is `2/4`, direction is `1/2/3`, tuple is `2/4`,
scalar is `4.75`, and maxiter's `100.75` signal outputs `101`.

## Reference-Rejected Forms

Each of the fifteen tracks in `catalog-audit-invalid.json` was run
individually through the reference executable and rejected:

- Wrong point/vector arity and nonscalar double values.
- Slash strings instead of numeric arrays for numeric/integer tuples.
- Scalar or raw tuple keys outside catalog bounds.
- Missing catalog default curves, even with an explicit key override.
- Constant/line point/vector paths: string-valued endpoints cannot feed
  the reference numeric-array tuple evaluator.
- Geometric curves on ordinary double/point targets.

Importer diagnostics are indexed and transactional. The existing
normalized-vector and integer-output partial fixtures retain valid
tracks without leaking rejected components into the document.

## Unresolved Review Items

These are explicit compatibility gaps, not automatically scheduled work.
Resolve their scope before Qt; neither adds a new parameter family or
changes the completed camera algorithms.

1. Full catalog/resolver validation. The source requires typed metadata
   and descriptions, validates discrete values, and rejects duplicate
   catalog names and unknown ordinary parameters. The adapter currently
   merges catalogs with last-wins semantics and can infer undeclared
   targets. Non-numeric defaults, complex/specialized generic metadata,
   and malformed unused entries do not have equivalent whole-catalog
   validation. Decide whether strict validation should replace this
   permissive compatibility behavior. Existing synthetic catalogs need
   correction if strict source validation is adopted.
2. Extreme-magnitude normalization. A reference Bezier vector with both
   control points `1e200/1` outputs `0/0` at every frame: squared length
   overflows to infinity and division yields zeros. The adapter explicitly
   rejects that case, as it does unresolved continuous proofs. Decide
   whether to preserve this conservative diagnostic or reproduce the
   reference's overflow output. Do not label this reference-rejected.

Additional key counts, vector extrapolation, camera3d paths, editing,
and new parameter families remain outside this checkpoint. The audit
does not certify those forms merely because generic lanes can hold them.
