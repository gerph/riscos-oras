# oras for RISC OS

`oras` is a command-line client for OCI registries. It publishes and retrieves
RISC OS files as OCI fileset artifacts, retaining recognised RISC OS file
metadata when an artifact is pulled.

## Requirements

The RISC OS `URLFetcher` modules must be installed and available before
`oras` can contact a registry. This first version supports anonymous access to
public OCI registries; authenticated registries, including Docker Hub, are not
currently supported.

An OCI reference must name an explicit registry host and repository, for
example `registry.example.net/example/tool:1.0`. If no tag is given, `latest`
is used. References may also select a manifest by digest.

## Commands

Run `*oras` with no arguments to display the command syntax.

```text
*oras pull <reference> [<directory>]
*oras push <reference> <path>...
*oras manifest fetch <reference> [<file>]
*oras blob fetch <reference> <digest> [<file>]
*oras tags <repository>
```

`pull` recreates every file in a RISC OS fileset artifact below the optional
destination directory (the current directory by default). Parent directories
are created as necessary and recognised file type, load address, execution
address and attributes are restored. Stored absolute paths and `..` traversal
are rejected.

`push` uploads each supplied file as an individual blob and publishes a
`application/vnd.riscos.fileset.v1` manifest. Input directories are not yet
accepted: list each file explicitly. The stored artifact names are the leaf
names of the supplied paths, so files with the same leaf name cannot be pushed
together.

`manifest fetch` writes the raw manifest JSON to standard output, or to its
optional file argument. `blob fetch` similarly writes a named blob. `tags`
prints one tag per line when the registry returns a normal tag list.

## Examples

Publish two RISC OS files to a public registry:

```text
*oras push registry.example.net/charles/demo:1.0 Apps.Demo,ff8 Docs.ReadMe,fff
```

Retrieve that fileset beneath `Work.Demo`:

```text
*oras pull registry.example.net/charles/demo:1.0 Work.Demo
```

Inspect the manifest and obtain a blob whose digest was listed in it:

```text
*oras manifest fetch registry.example.net/charles/demo:1.0 DemoManifest,fff
*oras blob fetch registry.example.net/charles/demo:1.0 sha256:0123456789abcdef... Blob,fff
```

List the available tags in a repository:

```text
*oras tags registry.example.net/charles/demo
```

The `--self-test` command performs local, non-network checks of reference
handling, fileset metadata and safe extraction paths.

```text
*oras --self-test
All self-tests passed
```

For the complete RISC OS reference manual, see
[`prminxml/oras.xml`](prminxml/oras.xml).
