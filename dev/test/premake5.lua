-- require "lfs"
-- Clean Function --
newaction {
    trigger = 'clean',
    description = 'clean the build',
    execute = function()
        print('clean the build...')
        os.rmdir('build')
        os.remove('Makefile')
        -- no wildcards in os.remove, so use shell
        os.execute('rm *.make')
        print('build cleaned')
    end
}

workspace 'rive'
configurations {'debug'}

dofile(path.join(path.getabsolute('../../dependencies/'), 'premake5_harfbuzz.lua'))
dofile(path.join(path.getabsolute('../../dependencies/'), 'premake5_sheenbidi.lua'))

project('tests')
do
    kind 'ConsoleApp'
    language 'C++'
    cppdialect 'C++17'
    toolset (_OPTIONS["toolset"] or "clang")
    targetdir 'build/bin/%{cfg.buildcfg}'
    objdir 'build/obj/%{cfg.buildcfg}'
    flags {'FatalWarnings'}
    buildoptions {'-Wall', '-fno-exceptions', '-fno-rtti'}

    includedirs {
        './include',
        '../../include',
        harfbuzz .. '/src',
        sheenbidi .. '/Headers'
    }
    links {
        'rive_harfbuzz',
        'rive_sheenbidi'
    }

    files {
        '../../src/**.cpp', -- the Rive runtime source
        '../../test/**.cpp', -- the tests
        '../../utils/**.cpp' -- no_op utils
    }

    defines {'TESTING', 'ENABLE_QUERY_FLAT_VERTICES', 'WITH_RIVE_TOOLS', 'WITH_RIVE_TEXT'}

    filter 'configurations:debug'
    do
        defines {'DEBUG'}
        symbols 'On'
    end

    filter 'system:windows'
    do
        removebuildoptions {
            -- vs clang doesn't recognize these on windows
            '-fno-exceptions',
            '-fno-rtti'
        }
        architecture 'x64'
        defines {
            '_USE_MATH_DEFINES',
            '_CRT_SECURE_NO_WARNINGS',
            '_CRT_NONSTDC_NO_DEPRECATE'
        }
    end

    filter {'system:windows', 'options:toolset=clang'}
    do
        buildoptions {
            '-Wno-c++98-compat',
            '-Wno-c++20-compat',
            '-Wno-c++98-compat-pedantic',
            '-Wno-c99-extensions',
            '-Wno-ctad-maybe-unsupported',
            '-Wno-deprecated-copy-with-user-provided-dtor',
            '-Wno-deprecated-declarations',
            '-Wno-documentation',
            '-Wno-documentation-pedantic',
            '-Wno-documentation-unknown-command',
            '-Wno-double-promotion',
            '-Wno-exit-time-destructors',
            '-Wno-float-equal',
            '-Wno-global-constructors',
            '-Wno-implicit-float-conversion',
            '-Wno-newline-eof',
            '-Wno-old-style-cast',
            '-Wno-reserved-identifier',
            '-Wno-shadow',
            '-Wno-sign-compare',
            '-Wno-sign-conversion',
            '-Wno-unused-macros',
            '-Wno-unused-parameter',
            '-Wno-four-char-constants',
            '-Wno-unreachable-code',
            '-Wno-switch-enum',
            '-Wno-missing-field-initializers'
        }
    end

    filter {'system:windows', 'options:toolset=msc'}
    do
        -- We currently suppress several warnings for the MSVC build, some serious. Once this build
        -- is fully integrated into GitHub actions, we will definitely want to address these.
        disablewarnings {
            "4061", -- enumerator 'identifier' in switch of enum 'enumeration' is not explicitly
                    -- handled by a case label
            "4100", -- 'identifier': unreferenced formal parameter
            "4201", -- nonstandard extension used: nameless struct/union
            "4244", -- 'conversion_type': conversion from 'type1' to 'type2', possible loss of data
            "4245", -- 'conversion_type': conversion from 'type1' to 'type2', signed/unsigned
                    --  mismatch
            "4267", -- 'variable': conversion from 'size_t' to 'type', possible loss of data
            "4355", -- 'this': used in base member initializer list
            "4365", -- 'expression': conversion from 'type1' to 'type2', signed/unsigned mismatch
            "4388", -- 'expression': signed/unsigned mismatch
            "4389", -- 'operator': signed/unsigned mismatch
            "4458", -- declaration of 'identifier' hides class member
            "4514", -- 'function': unreferenced inline function has been removed
            "4583", -- 'Catch::clara::detail::ResultValueBase<T>::m_value': destructor is not
                    -- implicitly called
            "4623", -- 'Catch::AssertionInfo': default constructor was implicitly defined as deleted
            "4625", -- 'derived class': copy constructor was implicitly defined as deleted because a
                    -- base class copy constructor is inaccessible or deleted
            "4626", -- 'derived class': assignment operator was implicitly defined as deleted
                    --  because a base class assignment operator is inaccessible or deleted
            "4820", -- 'bytes' bytes padding added after construct 'member_name'
            "4868", -- (catch.hpp) compiler may not enforce left-to-right evaluation order in braced
                    -- initializer list
            "5026", -- 'type': move constructor was implicitly defined as deleted
            "5027", -- 'type': move assignment operator was implicitly defined as deleted
            "5039", -- (catch.hpp) 'AddVectoredExceptionHandler': pointer or reference to
                    -- potentially throwing function passed to 'extern "C"' function under -EHc.
                    -- Undefined behavior may occur if this function throws an exception.
            "5045", -- Compiler will insert Spectre mitigation for memory load if /Qspectre switch
                    -- specified
            "5204", -- 'Catch::Matchers::Impl::MatcherMethod<T>': class has virtual functions, but
                    -- its trivial destructor is not virtual; instances of objects derived from this
                    -- class may not be destructed correctly
            "5219", -- implicit conversion from 'type-1' to 'type-2', possible loss of data
            "5262", -- MSVC\14.34.31933\include\atomic(917,9): implicit fall-through occurs here;
                    -- are you missing a break statement?
            "5264", -- 'rive::math::PI': 'const' variable is not used
        }
    end
end

newoption {
    trigger = 'toolset',
    value = 'type',
    description = 'Choose which toolchain to build with',
    allowed = {
        {'clang', 'Build with Clang'},
        {'msc', 'Build with the Microsoft C/C++ compiler'}
    },
    default = 'clang'
}
