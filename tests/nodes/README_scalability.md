# Obol Scene Graph Scalability Analysis

This document summarises the results of the `unit_scalability` benchmark, which
characterises the cost of building and traversing large Obol scene graphs
representative of a BRL-CAD CSG tree.

## Test structures

| Structure | Description | Total nodes |
|-----------|-------------|-------------|
| `flat` | Single root `SoSeparator` with N × (`SoTranslation` + `SoCube`) groups | 1 + 3N |
| `binary_tree` | Balanced binary tree of `SoSeparator` nodes; `SoCube` at each leaf | ≈ 4N |
| `linear_chain` | Depth-N chain of `SoSeparator` nodes, single `SoCube` at tip (top-down build) | N+1 |
| `chain_bottomup` | Same depth-N chain, built leaf-first | N+1 |
| `chain_deferred` | Same depth-N chain, top-down with `enableNotify(FALSE)` during build | N+1 |

## Measured operations

* **build** – allocate all nodes and wire up the hierarchy  
* **bbox** – `SoGetBoundingBoxAction` traversal (CPU only; no OpenGL required)  
* **search** – `SoSearchAction` for all `SoCube` nodes  
* **destroy** – `root->unref()` (reclaims all node memory)

## Representative results

Results below are from a typical CI runner (times in milliseconds).

```
# structure, leaves, total_nodes, build_ms, bbox_ms, search_ms, destroy_ms
flat,          100,    301,   0.4,   0.5,   0.1,  ~0
flat,        1 000,  3 001,   2.2,   2.1,   0.7,  ~0
flat,       10 000, 30 001,  21.4,  21.4,   9.3,  ~0
flat,       20 000, 60 001,  42.2,  42.3,  20.2,  ~0

binary_tree,   100,    399,   0.3,   0.4,   0.1,  ~0
binary_tree, 1 000,  3 999,   2.6,   3.5,   1.5,  ~0
binary_tree,10 000, 39 999,  25.8,  42.1,  25.0,  ~0
binary_tree,20 000, 79 999,  68.8,  95.1,  60.0,  ~0

linear_chain,  100,    101,   0.6,   0.3,   0.0,  ~0
linear_chain,  500,    501,  11.2,   4.2,   0.1,  ~0
linear_chain,1 000,  1 001,  50.3,  13.9,   0.3,  ~0
linear_chain,2 000,  2 001, 207.7,  53.7,   0.7,  ~0
linear_chain,5 000,  5 001,1265.2, 339.2,   1.9,  ~0

chain_bottomup,100,    101,   0.1,   0.3,   0.0,  ~0
chain_bottomup,500,    501,   0.5,   3.8,   0.1,  ~0
chain_bottomup,1000, 1 001,   0.9,  14.4,   0.3,  ~0
chain_bottomup,5000, 5 001,   4.7, 338.8,   1.9,  ~0

chain_deferred,100,    101,   0.1,   0.3,   0.0,  ~0
chain_deferred,500,    501,   0.5,   3.7,   0.1,  ~0
chain_deferred,1000, 1 001,   0.9,  14.2,   0.3,  ~0
chain_deferred,5000, 5 001,   4.9, 332.8,   2.0,  ~0
```

## Findings

### 1. Flat and balanced binary trees — O(n) — scale well

Both flat assemblies and balanced binary CSG combination trees exhibit
linear-time construction and traversal.  20 000-node scenes (60 000 – 80 000
total nodes) complete in under 100 ms for all operations.  **These structures
are safe to use for large BRL-CAD assemblies.**

### 2. Deep linear chain — construction — O(n²) bottleneck

**Symptom:** Building a depth-5 000 chain top-down takes ~1 265 ms; 10× the
depth would take ~126 seconds.

**Root cause** (`SoChildList::append`):  
When `SoGroup::addChild(child)` is called, `SoChildList::append` calls
`parent->startNotify()`, which propagates a change notification *upward*
through every ancestor.  For a top-down chain being built node-by-node,
the i-th `addChild` traverses i ancestor nodes → total O(n²).

**Mitigations:**

| Technique | Build speedup | Traversal speedup |
|-----------|--------------|-------------------|
| **Bottom-up construction** — build leaf first, wrap in parents | O(n²) → O(n) | none |
| **Deferred notify** — `enableNotify(FALSE)` on each new node; re-enable after | O(n²) → O(n) | none |
| Flatten very deep unary chains before attaching to the scene root | O(n²) → O(n) | O(n²) → O(n) |

For BRL-CAD integration: always build CSG sub-trees in isolation (no parent
yet) or bottom-up; attach the completed sub-tree to the scene root in a
single `addChild` call.

### 3. Deep linear chain — traversal — O(n²) regardless of construction

**Symptom:** `SoGetBoundingBoxAction` on a depth-5 000 chain takes ~339 ms
whether built top-down or bottom-up (same underlying structure).

**Root cause** (`SoCacheElement::addElement`):  
`SoBoundingBoxCache` recording relies on `SoCacheElement::addElement()`, which
iterates through *all currently open `SoCacheElement`* entries in the action
state stack.  For a chain at depth d, there are d open cache elements; each
state-element access during traversal therefore costs O(d).  Total BBox
traversal = Σ d (d = 0..n) = O(n²).

This is a fundamental property of Coin's nested-cache dependency system and
affects *any* action that creates per-separator caches (e.g. GL render caches).

**Mitigations:**

| Technique | Effect |
|-----------|--------|
| Prefer balanced trees (max depth O(log n)) | Reduces to O(n log n) |
| Set `separator->boundingBoxCaching.setValue(SoSeparator::OFF)` on non-leaf separators in the chain | Removes `addElement` overhead; eliminates caching benefit |
| Pre-flatten long unary CSG chains into a flat `SoSeparator` + transform list before display | O(n²) → O(n) |
| Implement a custom action that does not record per-separator bbox caches | O(n²) → O(n) |

### 4. Search action — O(n) for all structures

`SoSearchAction` (find all `SoCube`) is linear for all three structures.  A
5 000-leaf flat scene searches in ~4 ms; a 5 000-node deep chain in ~2 ms.
Search is safe for any scene size tested.

### 5. Destruction — effectively O(n)

`root->unref()` is always below measurement resolution (~0 ms), indicating
that Coin's reference-counting destruction is fast even for large graphs.

## Recommendations for BRL-CAD / Obol integration

1. **Prefer flat or balanced CSG trees in the Obol scene graph.**  A BRL-CAD
   CSG tree with *n* leaves and balanced combining operations maps to O(log n)
   depth in Obol, giving O(n log n) traversal — acceptable for n ≤ 100 000.

2. **Build sub-trees in isolation, attach once.**  Create all nodes and wire
   them up before attaching the root to the main scene, or use `enableNotify`
   bracketing.  This avoids O(n²) construction for deep branches.

3. **Pre-flatten long unary chains.**  A BRL-CAD object modified by a long
   sequence of single-child Boolean operations (subtract, union of one subtree)
   should be collapsed to a flat list of transforms under a single separator,
   not a depth-n chain.

4. **Consider disabling bbox caching on known deep separators.**  If a subtree
   will be frequently modified (animated CSG), disabling `boundingBoxCaching`
   removes the O(depth) addElement overhead per traversal step.

5. **Stack depth is not yet a limiting factor at n ≤ 5 000** in testing.
   Larger depths have not been tested; system stack limits (typically 1–8 MB)
   may impose practical limits beyond ~10 000 depth for recursive traversal.

## Running the benchmark

```sh
./bin/obol_test run unit_scalability
```

Output is a CSV table printed to stdout.  Redirect to a file and process with
your favourite spreadsheet or Python/pandas for graphing.

```sh
./bin/obol_test run unit_scalability | grep -v '^#' > scalability.csv
```
