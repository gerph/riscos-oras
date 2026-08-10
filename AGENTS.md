# oras agent notes

## Purpose

`oras` is a RISC OS command-line client for OCI registries.

The first implementation target is a read/write client for a RISC OS-specific
OCI artifact format that stores one blob per file and preserves recognised
RISC OS metadata on pull.

Primary commands:

- `*oras pull <reference> [<directory>]`
- `*oras push [--source <uri|github:owner/repository>] <reference> <path>...`
- `*oras manifest fetch [--pretty] <reference> [<file>]`
- `*oras manifest fetch-config <reference> [<file>]`
- `*oras manifest push <reference> <file>`
- `*oras blob fetch <reference> <digest> [<file>]`
- `*oras tags <repository>`
- `*oras login <registry> <username>`
- `*oras logout <registry>`

## Artifact format

Manifest conventions:

- manifest `artifactType`: `application/vnd.riscos.fileset.v1`
- manifest annotation: `org.riscos.artifact.format = fileset-v1`
- one OCI layer descriptor per file
- no tar bundling in v1
- no sidecar metadata blob in v1

Per-file conventions:

- blob bytes are the exact file contents
- blob media type is `application/riscos`, with registered metadata parameters
- `org.opencontainers.image.title` stores a portable relative output path
- `org.riscos.filename` preserves the native RISC OS leaf name
- RISC OS metadata is stored in descriptor annotations

Supported metadata annotations:

- `org.riscos.filetype`
- `org.riscos.loadaddr`
- `org.riscos.execaddr`
- `org.riscos.attr`

Push currently records the native leaf name only; it does not preserve input
directory prefixes. Inputs with the same leaf name must be rejected with a
clear error rather than silently overwriting one another.

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
- `oci_auth`: Docker-compatible credential storage and challenge handling
- `oci_manifest`: manifest/index parsing and writing via `cJSON`
- `riscos_metadata`: read/apply/validate RISC OS metadata
- `riscos_fileset`: convert local files to descriptors and extract filesets
- `selftest`: project-local non-network tests

## Scope notes

In scope:

- anonymous, Basic and Bearer-authenticated registry access
- manifest fetch
- formatted manifest fetch on request
- config blob fetch from a manifest descriptor
- prepared manifest and index push
- blob fetch
- tags listing
- fileset push planning
- fileset pull extraction
- index selection support

Deferred for later:

- delete/copy/referrers
- richer manifest config metadata
- recursive input-directory preservation and multi-manifest architecture indexes

## Test intent

The project has a local `--self-test` path for non-network behaviour and a
local fake-registry smoke test for anonymous, Basic and Bearer workflows. Do
not add live registry credentials to CI.
