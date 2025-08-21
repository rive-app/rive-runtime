# Emscripten WebGPU port with optional Wagyu extensions


import os
import zlib
from typing import Dict, Optional


OPTIONS = {
    'wagyu': "Enable Wagyu extensions (default: false)",
}
_VALID_OPTION_VALUES = {
    'wagyu': {'true', 'false'},
}
_opts: Dict[str, Optional[str]] = {
    'wagyu': 'false',
}


def _get_base_dir():
    return os.path.dirname(os.path.realpath(__file__))


def _get_include_dir():
    return os.path.join(_get_base_dir(), 'include')


def _get_src_dir():
    return os.path.join(_get_base_dir(), 'src')


def _get_srcs():
    return []


def _recurse_dir(path):
    for (dirpath, dirnames, filenames) in os.walk(path):
        for filename in filenames:
            yield os.path.join(dirpath, filename)


def _get_build_files():
    return sorted([
        __file__,
        *_get_srcs(),
        *_recurse_dir(_get_include_dir()),
    ])


# compiler

def _check_option(option, value, error_handler):
    if value not in _VALID_OPTION_VALUES[option]:
        error_handler(
            f'[{option}] can be {list(_VALID_OPTION_VALUES[option])}, got [{value}]'
        )
    return value


def handle_options(options, error_handler):
    for option, value in options.items():
        value = value.lower()
        _opts[option] = _check_option(option, value, error_handler)


def process_args(ports):
    args = ['-isystem', _get_include_dir()]
    return args


# linker

def _get_flags(settings):
    lib_name_suffix = ''
    flags = ['-lexports.js']
    return (lib_name_suffix, flags)


def _get_name(settings):
    hash_value = 0

    def add(x):
        nonlocal hash_value
        hash_value = zlib.adler32(x, hash_value)

    build_files = _get_build_files()
    for filename in build_files:
        add(open(filename, 'rb').read())

    (lib_name_suffix, _) = _get_flags(settings)
    return f'libwebgpu-{hash_value:08x}{lib_name_suffix}.a'


def clear(ports, settings, shared):
    shared.cache.erase_lib(_get_name(settings))


def get(ports, settings, shared):
    if settings.allowed_settings:
        return []

    def create(final):
        includes = [_get_include_dir()]
        (_, flags) = _get_flags(settings)
        flags += ['-g', '-std=c++20', '-fno-exceptions', '-fno-rtti']
        ports.build_port(_get_src_dir(), final, 'webgpu', includes=includes, flags=flags, srcs=_get_srcs())

    lib_name = _get_name(settings)
    return [shared.cache.get_lib(lib_name, create, what='port')]


def linker_setup(ports, settings):
    if settings.USE_WEBGPU:
        raise Exception('webgpu-port is not compatible with deprecated Emscripten USE_WEBGPU option')

    src_dir = _get_src_dir()

    settings.JS_LIBRARIES += [ os.path.join(src_dir, 'library_webgpu_stubs.js') ]

    if _opts['wagyu'] == 'true':
        settings.JS_LIBRARIES += [ os.path.join(src_dir, 'library_webgpu_wagyu_stubs.js') ]
