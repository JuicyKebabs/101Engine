# ActorImprint asset schema v1

ET-08 owns the immutable definition and `.imprint` persistence format. AssetGUID and relative path belong to the Asset catalog and `.meta` file, and are absent from this document. The canonical example is [Minimal.imprint](../Tests/Fixtures/ActorImprint/Minimal.imprint).

## Envelope and structure

Every field below is required. Unknown fields are rejected at each structural object. Null is accepted only where explicitly stated.

| Object | Field | Representation and validation |
| --- | --- | --- |
| Asset | `version` | Integer `1`; floating `1.0` is invalid |
| Asset | `definitionRevision` | Nonzero GUID string; serialized using `Guid::ToString()` |
| Asset | `rootActorLocalObjectId` | ID of the sole root Actor |
| Asset | `nextLocalObjectId` | ID strictly greater than every present Actor and Component ID |
| Asset | `actors` | Nonempty array of Actor records |
| Actor | `localObjectId` | Actor identity |
| Actor | `parentLocalObjectId` | `null` for the declared root; otherwise an Actor ID within this definition |
| Actor | `properties` | Reflected Actor property object |
| Actor | `components` | Array of Component records; exactly one Transform-family member is required |
| Component | `localObjectId` | Component identity in the same namespace as Actor IDs |
| Component | `type` | Registered stable Component name, e.g. `Transform`, never a compiler type name |
| Component | `properties` | Object matching that registered type's Reflection metadata |

All IDs are JSON integers in `[1, UINT64_MAX]`. Zero, negative, fractional, floating, boolean and string forms are rejected. IDs cannot overlap across Actors and Components. The next-ID rule makes `UINT64_MAX` unavailable as an assigned object ID; allocation must fail rather than wrap when exhausted. Deleted IDs are never reused by editors. ET-08 can validate the current high watermark; preservation across edits belongs to ET-16.

The parent graph must be closed, connected and acyclic. Root and parent references cannot target Components. Input Actor/Component array order has no meaning; serialized arrays are sorted by LocalObjectID. Property arrays retain their order.

## Properties and references

Actor metadata currently defines required `name` (string), `tag` (string), and `is_active` (boolean). Setters preserve Actor behavior; tag names use the existing TagRegistry contract, including automatic registration. Existing Scene field placement and Inspector UI are unaffected.

Component property shape, optionality, nullability, numeric representation, ranges and whole-object invariants are owned by registered Reflection metadata. For example, Transform requires `name`, `position`, `rotation`, and `scale`. Float/vector values use Reflection's finite floating-number representation; an integer JSON value is not silently treated as a float. Optional properties may be omitted only when declared optional by metadata. Unknown properties are rejected, including nested properties.

Values pass through Reflection Deserialize → Serialize, then a second round-trip must produce an equal result. Stored defaults therefore reflect C++ value precision and domain setters. Non-idempotent normalization is rejected. No per-Component serialization schema is introduced here.

An ActorReference property accepts exactly:

```json
null
```

or:

```json
{"type":"ActorReference","scope":"local","localObjectId":10}
```

The target must be an Actor in this definition. Additional fields, raw GUID strings, Component references and scene-scope references are rejected. The existing ActorReferenceCodec interface translates local IDs through a temporary GUID map during validation; runtime ActorReference remains GUID-based. The approved `scope: "scene"` / `actorGuid` form is reserved for instance overrides in ET-12 and is invalid in an asset definition.

AssetReference properties retain the existing AssetReferenceCodec representation: `null` or a valid GUID string, with expected AssetType supplied by metadata. Non-null references require a caller-provided validating catalog context; missing assets and mismatched types fail. ET-08 does not scan, load or own that catalog.

## Revision, publication and diagnostics

DefinitionRevision is a dedicated GUID-backed value. Reading or serializing a definition does not issue a revision. The approved editing contract issues a new revision after meaningful normalized content changes and retains it for equivalent saves; ET-16 implements that save workflow.

`Deserialize` and `Load` publish `unique_ptr<const ActorImprint>` only after all phases succeed. Validation creates detached Actor/Component objects through existing factories and policies, uses their setters, and destroys them before return. It does not register or attach them to a Scene. The immutable definition retains normalized JSON and resolved runtime type information, but no candidate object pointers, AssetGUID or path. Component code and metadata must remain loaded while its resolved type information is used.

On failure the API returns null with an error category, JSON Pointer and message. File I/O and syntax failures identify the file instead. `Load` also rejects duplicate JSON keys during parsing; `Deserialize` receives an already parsed DOM, which cannot retain duplicate keys. Neither API writes the input or replaces any existing definition.

Serialization uses the normalized immutable records and the JSON library's sorted object keys. With the same `dump` options, Serialize → Deserialize → Serialize produces identical output. Formatting whitespace from the source file is not preserved.

## Editor asset lifecycle

Editor creation writes the `.imprint` and its `.meta` sidecar through a same-directory staged workflow. The staged definition must pass this schema reader before publication. Metadata is made visible first and the discoverable `.imprint` file last; catalog failure rolls the pair back. The initial definition contains root Actor ID 1, its required Transform ID 2, and `nextLocalObjectId` 3.

Editor deletion requires the catalogued type and sidecar AssetGUID to agree. It is refused while an ActorImprint Document is open for the asset or while any loaded Scene contains one of its Instances. A permitted deletion stages both files out of catalog-visible names, publishes the catalog removal, restores both on publication failure, and then removes the staging files.
