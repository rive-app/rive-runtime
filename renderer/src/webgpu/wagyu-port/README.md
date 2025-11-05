# WebGPU Port - WebGPU bindings for Emscripten

This is an implementation of WebGPU bindings for Emscripten.

This package provides webgpu.h header provided by emdawnwgpu.

This package optionally provides Wagyu extensions to the WebGPU API, which
are available from the webgpu_wagyu.h header. These extensions are not part of
the WebGPU standard, and are specific to Wagyu use cases.

## Usage

You can either use this as an Emscripten "port" or as a "remote port". They are
essentially the same thing, but a remote port downloads and provides the actual
port indirectly.

To use the remote port file, pass it as an option like this to your
emscripten build commands:

```bash
--use-port=[your_path]/webgpu-remoteport.py
```

You can also directly use the local port file `webgpu-port.py` from the
webgpu-port repository (or a copy of it).

```bash
--use-port=[your_path]/webgpu-port.py
```

## Configuration

You can configure the port by passing options to `--use-port` as key=value
pairs delimited by colons.

For example:

```bash
--use-port=webgpu-remoteport.py:wagyu=true
```

Supported options:

- `wagyu`:

    Controls whether Wagyu extensions should be used.

    true to enable, false to disable (default is false).
