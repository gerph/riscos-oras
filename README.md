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
*oras manifest fetch [--pretty] <reference> [<file>]
*oras blob fetch <reference> <digest> [<file>]
*oras tags <repository>
*oras tag <reference> <tag>...
*oras attach <reference> <path>...
*oras login <registry> <username>
*oras logout <registry>
```

`pull` recreates every file in a RISC OS fileset artifact below the optional
destination directory (the current directory by default). Parent directories
are created as necessary and recognised file type, load address, execution
address and attributes are restored. Stored absolute paths and `..` traversal
are rejected.

`push` uploads each supplied file as an individual `application/riscos` blob
and publishes a `application/vnd.riscos.fileset.v1` manifest. The layer media
type includes its registered `name`, `load`, `exec`, and `access` parameters
where available; the numeric filetype is the `,xyz` suffix of `name`. Input
directories are not yet accepted: list each file explicitly. The stored
artifact names are the leaf names of the supplied paths, so files with the
same leaf name cannot be pushed together. RISC OS filenames such as `oras/xml`
are retained when pulled; the OCI title uses its portable form, `oras.xml`.

The current fileset format records one leaf name for each explicit input file;
it does not retain the input directory prefix. For example,
`aif26.oras`, `aif32.oras`, and `aif64.oras` all have the leaf name `oras` and
cannot be pushed together. Give architecture variants distinct leaf names
before pushing, such as `oras-26`, `oras-32`, and `oras-64`; otherwise `push`
reports `Duplicate input leaf name 'oras'`.

Each layer records the RISC OS filetype, load address, execution address and
access attributes as OCI annotations. This metadata is restored on pull.

Use `--source` to attach an OCI source annotation to a particular artifact.
For GitHub repositories, `github:<owner>/<repository>` expands to the matching
`https://github.com/...` URI, allowing GHCR to associate that artifact with its
source repository. Source metadata is never added unless requested.

`manifest fetch` writes the raw manifest JSON to standard output, ending it
with a newline, or writes the unmodified JSON to its optional file argument.
Pass `--pretty` to format the JSON for human reading before writing it.
`blob fetch` similarly writes a named blob. `tags` prints one tag per line
when the registry returns a normal tag list.

`tag` makes one or more additional names for the manifest selected by its
source reference. It copies neither blobs nor files, and always creates tags
in the source reference's own registry and repository. Tag arguments are tag
names, not full OCI references.

`attach` publishes the supplied files as a RISC OS fileset attachment to an
existing manifest. Attachments are stored through the OCI referrers tag scheme;
repeated attachments retain the earlier referrer descriptors.

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

Inspect the raw manifest, or print an indented version for people to read:

```text
*oras manifest fetch registry.example.net/charles/demo:1.0 DemoManifest,fff
*oras manifest fetch --pretty registry.example.net/charles/demo:1.0
*oras manifest fetch --pretty ghcr.io/gerph/riscos-oras:0.02
```

The file form writes the registry's JSON unchanged. The screen form always
ends with a newline; `--pretty` is the only option that reformats it.

Obtain a blob whose digest was listed in the manifest:

```text
*oras blob fetch registry.example.net/charles/demo:1.0 sha256:0123456789abcdef... Blob,fff
```

List the available tags in a repository:

```text
*oras tags registry.example.net/charles/demo
```

Give an existing manifest a release tag without reuploading its files:

```text
*oras tag registry.example.net/charles/demo:1.0 stable release-1
```

Attach release notes to a published artifact:

```text
*oras attach registry.example.net/charles/demo:1.0 Docs.ReleaseNotes,fff
```

Log in before publishing to GHCR:

```text
*oras login ghcr.io gerph
Secret for gerph at ghcr.io:
*oras push ghcr.io/gerph/riscos-oras:1.0 Apps.Demo,ff8
```

Associate an artifact with its GitHub source repository:

```text
*oras push --source github:gerph/riscos-oras ghcr.io/gerph/riscos-oras:0.02 prminxml.oras/xml
```

Retrieve that artifact. The native RISC OS filename is restored below the
destination, including its filetype and other recognised metadata:

```text
*oras pull ghcr.io/gerph/riscos-oras:0.02 Work.ORAS
```

For the artifact above, the restored file is `Work.ORAS.oras/xml`.

The `--self-test` command performs local, non-network checks of reference
handling, fileset metadata and safe extraction paths.

```text
*oras --self-test
All self-tests passed
```

For the complete RISC OS reference manual, see
[`prminxml/oras.xml,f80`](prminxml/oras.xml,f80).
