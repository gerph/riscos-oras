# oras for RISC OS

`oras` is a command-line client for OCI registries. It publishes and retrieves
RISC OS files as OCI fileset artifacts, retaining recognised RISC OS file
metadata when an artifact is pulled.

## Requirements

The RISC OS `URLFetcher` modules must be installed and available before
`oras` can contact a registry. Anonymous registries work without setup.
For authenticated registries, `login` stores Docker-compatible credentials in
the file named by `<ORASAuthentication$Write>`. This is not encrypted; protect
the file with normal RISC OS access controls.

An OCI reference must name an explicit registry host and repository, for
example `registry.example.net/example/tool:1.0`. If no tag is given, `latest`
is used. References may also select a manifest by digest.

## Commands

Run `*oras` with no arguments to display the command syntax.

```text
*oras pull <reference> [<directory>]
*oras push [--source <uri|github:owner/repository>] <reference> <path>...
*oras manifest fetch <reference> [<file>]
*oras blob fetch <reference> <digest> [<file>]
*oras tags <repository>
*oras login <registry> <username>
*oras logout <registry>
```

`pull` recreates every file in a RISC OS fileset artifact below the optional
destination directory (the current directory by default). Parent directories
are created as necessary and recognised file type, load address, execution
address and attributes are restored. Stored absolute paths and `..` traversal
are rejected.

`push` uploads each supplied file as an individual `application/riscos` blob
and publishes a `application/vnd.riscos.fileset.v1` manifest. Input directories
are not yet accepted: list each file explicitly. The stored artifact names are
the leaf names of the supplied paths, so files with the same leaf name cannot
be pushed together. RISC OS filenames such as `oras/xml` are retained when
pulled; the OCI title uses its portable form, `oras.xml`.

Each layer records the RISC OS filetype, load address, execution address and
access attributes as OCI annotations. This metadata is restored on pull.

Use `--source` to attach an OCI source annotation to a particular artifact.
For GitHub repositories, `github:<owner>/<repository>` expands to the matching
`https://github.com/...` URI, allowing GHCR to associate that artifact with its
source repository. Source metadata is never added unless requested.

`manifest fetch` writes the raw manifest JSON to standard output, or to its
optional file argument. `blob fetch` similarly writes a named blob. `tags`
prints one tag per line when the registry returns a normal tag list.

`login` prompts for the secret without echoing it, then replaces the matching
`auths.<registry>.auth` entry. `logout` removes only that registry entry.
Credentials are used after a registry has sent a standard Basic or Bearer
challenge. They are sent only over HTTPS, except to `localhost` and
`127.0.0.1` development registries.

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

Log in before publishing to GHCR:

```text
*oras login ghcr.io gerph
Secret for gerph at ghcr.io:
*oras push ghcr.io/gerph/riscos-oras:1.0 Apps.Demo,ff8
```

Associate an artifact with its GitHub source repository:

```text
*oras push --source github:gerph/riscos-oras ghcr.io/gerph/riscos-oras:0.03 prminxml.oras/xml
```

The `--self-test` command performs local, non-network checks of reference
handling, fileset metadata and safe extraction paths.

```text
*oras --self-test
All self-tests passed
```

For the complete RISC OS reference manual, see
[`prminxml/oras.xml`](prminxml/oras.xml).
