# GObject Paranoid Pirate (GPP)

GPP is a simple GObject implementation of the [Paranoid Pirate Pattern](http://rfc.zeromq.org/spec:6),
which uses zeromq internally.

GPP is *not* a zeromq GLib wrapper.

It provides a #GPPQueue object, which relays requests from a #GPPClient to a #GPPWorker, and replies
from the worker to the client, as simple strings.

It implements heartbeating, which means that if a worker fails in some way, the client will be able
to make its request again, with a per-request retries limit.

Workers are picked on a least-recently-used basis.

One can indifferently instantiate and use all these objects in the same process or in separate ones.

It doesn't implement client prioritization, this is up to the user.

# Build

Get the [meson build system](https://github.com/mesonbuild/meson).

```
mkdir build
cd build
meson ..
ninja
```

You can see available options with:

```
mesonconf
```

You canset them with the -D flag,
for example:

```
meson -Ddisable-introspection ..
```

# Documentation and Usage

Visit [the slate documentation](http://mathieuduponchelle.github.io/gpp_documentation) or read the source.
