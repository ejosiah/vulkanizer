import argparse
from pathlib import Path
import subprocess
import sys

from model_tools import RESOURCES, add_build_arguments, display_command, executable, find_mesh


def main(argv=None):
    parser = argparse.ArgumentParser(description='Render a converted model with the Vulkanizer reference test.')
    parser.add_argument('model', metavar='Model', help='Model name, or path to a .vkmesh file')
    parser.add_argument('--resources-dir', type=Path, default=RESOURCES)
    add_build_arguments(parser)
    parser.add_argument('--executable', type=Path, help='Override the reference viewer executable')
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--ray-traced', action='store_true')
    modes.add_argument('--wireframe', action='store_true')
    parser.add_argument('--samples', type=int, help='Ray tracing: 1–32 (default 8); raster: MSAA count (default 1)')
    parser.add_argument('--camera', choices=('orbit', 'fps'), help='Defaults to FPS for Sponza, orbit otherwise')
    parser.add_argument('--blur', action='store_true')
    parser.add_argument('--no-ibl', action='store_true')
    parser.add_argument('--exposure', type=float)
    parser.add_argument('--validation', action='store_true')
    parser.add_argument('--timeout', type=float, help='Close after this many seconds')
    parser.add_argument('--screenshot', type=Path)
    parser.add_argument('--no-game-controller', action='store_true')
    parser.add_argument('--dry-run', action='store_true', help='Print the viewer command without launching it')
    args = parser.parse_args(argv)
    try:
        mesh = find_mesh(args.model, args.resources_dir.resolve())
        viewer = executable('vulkanizer_material_extensions_test', args.config, args.build_dir, args.executable)
        command = [str(viewer), '--mesh', str(mesh)]
        camera = args.camera or ('fps' if mesh.stem.casefold() == 'sponza' else 'orbit')
        if camera == 'fps':
            command.append('--fps-camera')
        for name in ('ray_traced', 'wireframe', 'blur', 'no_ibl', 'validation', 'no_game_controller'):
            if getattr(args, name):
                command.append('--' + name.replace('_', '-'))
        for name in ('samples', 'exposure', 'timeout', 'screenshot'):
            value = getattr(args, name)
            if value is not None:
                command.extend(['--' + name, str(value)])
        print(display_command(command), flush=True)
        if args.dry_run:
            return 0
        return subprocess.run(command, check=False).returncode
    except (OSError, ValueError) as error:
        print(f'Error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
