from pathlib import Path
import argparse
import os
import subprocess
import shutil

ROOT = Path(__file__).resolve().parents[3]
RESOURCES = ROOT / 'test' / 'mesh' / 'resources'
SUFFIXES = ('.vkmesh', '.vkmat', '.vkbounds')


def complete_mesh(path):
    return all(path.with_suffix(suffix).is_file() for suffix in SUFFIXES)


def find_mesh(model, resources=RESOURCES):
    supplied = Path(model).expanduser()
    if supplied.suffix.lower() == '.vkmesh':
        if not complete_mesh(supplied):
            raise ValueError(f'Incomplete mesh asset: {supplied}; expected .vkmesh, .vkmat and .vkbounds')
        return supplied.resolve()
    matches = [p for p in resources.rglob('*.vkmesh') if p.stem.casefold() == model.casefold() and complete_mesh(p)]
    if not matches:
        raise ValueError(f'Model {model!r} is not converted under {resources}. Run sync_models.py --models "{model}" first.')
    if len(matches) > 1:
        raise ValueError('Ambiguous model name; supply a .vkmesh path:\n' + '\n'.join(str(p) for p in matches))
    return matches[0].resolve()


def add_build_arguments(parser):
    parser.add_argument('--config', choices=('Debug', 'Release'), default='Release')
    parser.add_argument('--build-dir', type=Path, help='Defaults to cmake-build-debug or cmake-build-release')


def executable(name, config, build_dir=None, override=None):
    if override:
        candidates = [Path(override).expanduser()]
    else:
        build_dir = (build_dir or ROOT / f'cmake-build-{config.lower()}').resolve()
        filename = name + ('.exe' if os.name == 'nt' else '')
        candidates = [build_dir / 'test' / config / filename, build_dir / 'test' / filename,
                      build_dir / 'test' / 'mesh' / config / filename, build_dir / 'test' / 'mesh' / filename]
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise ValueError(f'Cannot find {name}. Build the {config} configuration or supply an executable path. Tried: ' + ', '.join(map(str, candidates)))


def positive_int(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError('must be positive')
    return number


def display_command(command):
    return subprocess.list2cmdline([str(part) for part in command])


def conversion_environment(ktx=None):
    environment = os.environ.copy()
    candidate = ktx or environment.get('VKZ_KTX_EXECUTABLE') or ROOT / 'build' / 'tools' / 'ktx' / 'bin' / ('ktx.exe' if os.name == 'nt' else 'ktx')
    if not Path(candidate).is_file():
        if ktx:
            raise ValueError(f'KTX converter does not exist: {ktx}')
        candidate = shutil.which(str(candidate)) or shutil.which('ktx')
    if not candidate:
        raise ValueError('KTX converter not found; supply --ktx or set VKZ_KTX_EXECUTABLE.')
    environment['VKZ_KTX_EXECUTABLE'] = str(Path(candidate).resolve())
    return environment
