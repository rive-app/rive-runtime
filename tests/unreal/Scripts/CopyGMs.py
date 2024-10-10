from argparse import ArgumentParser
from pathlib import Path

# premake command for GMs
''' 
premake5 --scripts="./premake5.lua" --with_rive_text --with_rive_audio=external --for_unreal --config=release --os=windows --out="out\release" vs2022
'''
UNREAL_INSERTS_START =  \
'''THIRD_PARTY_INCLUDES_START
#undef PI
#pragma warning( push )
#pragma warning( disable : 4611 )
'''

UNREAL_INSERTS_END = \
'''THIRD_PARTY_INCLUDES_END
#define PI (3.1415926535897932f)
#pragma warning( pop )
'''

ProgramDescription = 'Copy Gms from Rive test folder to Unreal source modifything to work for unreal'
ScriptPath = Path(__file__).parent.parent.resolve()

'''
List of files to skip when processing src GM files
'''

ExcludeGMList = \
[
    'gmmain.cpp',
]

'''
List of files to include from common folder
'''

IncludeCommmonList = \
[
    'write_png_file.hpp',
    'write_png_file.cpp',
    'testing_window.hpp',
    'testing_window.cpp',
    'queue.hpp',
    'rand.hpp',
    'test_harness.hpp',
    'test_harness.cpp',
    'tcp_client.hpp',
    'tcp_client.cpp',
    'rive_mgr.hpp',
    'rive_mgr.cpp',
]

'''
updates @file_data to work with unreal build system 
We only update .cpp files though, that way we dont need to redefine things at then end

First it tries to insert UNREAL_INSERTS_START before the first #include line
if there are no #include lines it will attempt to insert at the first empty line
if there are no empty lines it will just insert at the very front

UNREAL_INSERTS_END always insert at the end
'''

def update_file_data(file_data : str, file_path: Path):

    if file_path.suffix != '.cpp':
        return file_data
    
    first_include = file_data.find('#include')
    if first_include != -1:
        if first_include != 0:
            return file_data[:first_include-1] + UNREAL_INSERTS_START + file_data[first_include-1:] + UNREAL_INSERTS_END
        else:
            return UNREAL_INSERTS_START + file_data + UNREAL_INSERTS_END
        
    first_empty_line = file_data.find('#include')
    if first_include != -1:
        if first_include != 0:
            return file_data[:first_empty_line-1] + UNREAL_INSERTS_START + file_data[first_empty_line-1:] + UNREAL_INSERTS_END
    
    return UNREAL_INSERTS_START + file_data + UNREAL_INSERTS_END

'''
Iterates through @src_path collecting all files with extension .cpp|c.h|.hpp
Then prepends BEGIN_THRID_PARTY__INCLUDE and appends END_THRID_PARTY_INCLUDE constants
Then writes the file out to dst_path with the same name as the input
@excludes is removed from the generated list of src files before iteratation begins
'''

def copy_from_path_excluding(src_path, dst_path, excludes=[]):
    src_files =  []
    src_files.extend(src_path.glob('**/*.cpp'))
    src_files.extend(src_path.glob('**/*.h'))
    src_files.extend(src_path.glob('**/*.hpp'))

    for file in excludes:
        full_file = src_path.joinpath(file)
        if full_file in src_files:
            src_files.remove(full_file)

    for src in src_files:
        print(f'processing {src.name}')
        src_data = None
        with open(src, 'r') as src_file:
            src_data = update_file_data(src_file.read(), src)
        
        if src_data is None:
            print(f'Failed to read {src}, skipping')
            continue

        with open(dst_path.joinpath(src.name), 'wt') as dst_file:
            dst_file.write(src_data)

'''
Iterates through @includes appending each value like this "src_path + include_value" 
and reading the contents of the file with that path
Then prepends BEGIN_THRID_PARTY__INCLUDE and appends END_THRID_PARTY_INCLUDE constants
Then writes the file out to dst_path with the same name as the input
'''

def copy_from_path_including(src_path, dst_path, includes=[]):
    for src_name in includes:
        src = src_path.joinpath(src_name)

        print(f'processing {src.name}')
        src_data = None
        with open(src, 'r') as src_file:
            src_data = update_file_data(src_file.read(), src)
        
        if src_data is None:
            print(f'Failed to read {src}, skipping')
            continue

        with open(dst_path.joinpath(src.name), 'wt') as dst_file:
            dst_file.write(src_data)

if __name__ == '__main__':
    parser = ArgumentParser(prog='CopyGMs',
                                     description=ProgramDescription)
    
    parser.add_argument('rive_tests_path')
    parser.add_argument('-u','--unreal_path', default=ScriptPath.joinpath("Source/rive_unreal"))

    args = parser.parse_args()

    gm_src_path = Path(args.rive_tests_path).joinpath("gm")
    gm_dst_path = Path(args.unreal_path).joinpath("gm")

    common_src_path = Path(args.rive_tests_path).joinpath("common")
    common_dst_path = Path(args.unreal_path).joinpath("common")
    
    copy_from_path_excluding(gm_src_path, gm_dst_path, excludes=ExcludeGMList)
    copy_from_path_including(common_src_path, common_dst_path, includes=IncludeCommmonList)
