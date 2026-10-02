import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

from model_tools import SUFFIXES, add_build_arguments, complete_mesh, conversion_environment, executable


def main(argv=None):
    parser = argparse.ArgumentParser(description='Convert a glTF or GLB file into an output directory, keeping the source name.')
    parser.add_argument('source', type=Path, help='Source .gltf or .glb file')
    parser.add_argument('output', type=Path, help='Output directory (not a filename)')
    add_build_arguments(parser)
    parser.add_argument('--converter', type=Path, help='Override the mesh converter executable')
    parser.add_argument('--ktx', type=Path, help='Override the KTX command-line converter')
    args = parser.parse_args(argv)
    try:
        source = args.source.expanduser().resolve()
        output = args.output.expanduser().resolve()
        if source.suffix.lower() not in ('.gltf', '.glb'):
            raise ValueError('Source must be a .gltf or .glb file.')
        if not source.is_file():
            raise ValueError(f'Source does not exist: {source}')
        if output.exists() and not output.is_dir():
            raise ValueError(f'Output must be a directory: {output}')
        converter = executable('vulkanizer_mesh_convert_test', args.config, args.build_dir, args.converter)
        environment = conversion_environment(args.ktx)
        output.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.convert-', dir=output) as temporary:
            staging = Path(temporary).resolve()
            if not staging.is_relative_to(output):
                raise ValueError('Invalid conversion staging directory')
            mesh = staging / (source.stem + '.vkmesh')
            result = subprocess.run([str(converter), str(source), str(mesh)], env=environment, check=False)
            if result.returncode:
                return result.returncode
            if not complete_mesh(mesh):
                raise ValueError('Converter did not produce all mesh, material and bounds files.')
            for suffix in reversed(SUFFIXES):
                asset = mesh.with_suffix(suffix)
                destination = output / asset.name
                asset.replace(destination)
                print(f'Saved {destination}', flush=True)
        return 0
    except (OSError, ValueError) as error:
        print(f'Error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
