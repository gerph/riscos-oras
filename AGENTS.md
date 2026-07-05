# oras agent notes

## Purpose

`oras` is a RISC OS command-line client for OCI registries.

The first implementation target is a read/write client for a RISC OS-specific
OCI artifact format that stores one blob per file and preserves recognised
RISC OS metadata on pull.

Primary commands:

- `*oras pull <reference> [<directory>]`
- `*oras push <reference> <path>...`
- `*oras manifest fetch <reference> [<file>]`
- `*oras blob fetch <reference> <digest> [<file>]`
- `*oras tags <repository>`

## Artifact format

Manifest conventions:

- manifest `artifactType`: `application/vnd.riscos.fileset.v1`
- manifest annotation: `org.riscos.artifact.format = fileset-v1`
- one OCI layer descriptor per file
- no tar bundling in v1
- no sidecar metadata blob in v1

Per-file conventions:

- blob bytes are the exact file contents
- `org.opencontainers.image.title` stores the relative output path
- RISC OS metadata is stored in descriptor annotations

Supported metadata annotations:

- `org.riscos.filetype`
- `org.riscos.loadaddr`
- `org.riscos.execaddr`
- `org.riscos.attr`

Pull behaviour:

- always recreates files from layer descriptors
- always applies recognised RISC OS metadata after writing the file
- creates parent directories from the stored relative path
- rejects unsafe output paths such as absolute paths or `..` traversal

Push behaviour:

- uploads each input file as its own blob
- captures available RISC OS metadata from the source file
- publishes a manifest using the fileset artifact type

## Module split

Keep `c/main` as dispatch and command glue only.

Planned reusable modules:

- `url_fetch`: URL Fetcher wrapper and HTTP-style response parsing
- `oci_registry`: reference parsing and anonymous registry operations
- `oci_manifest`: manifest/index parsing and writing via `cJSON`
- `riscos_metadata`: read/apply/validate RISC OS metadata
- `riscos_fileset`: convert local files to descriptors and extract filesets
- `selftest`: project-local non-network tests

## Scope notes

In scope for the first cut:

- anonymous/public registry access
- manifest fetch
- blob fetch
- tags listing
- fileset push planning
- fileset pull extraction
- index selection support

Deferred for later:

- authenticated registry access
- delete/copy/referrers
- richer manifest config metadata

## Test intent

The project should gain a local `test` target backed by an internal
`--self-test` path so the core behaviour can be exercised without live
registry dependencies.
