import argparse
import json
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.parse import quote, unquote, urlsplit
from urllib.request import Request, urlopen

from model_tools import ROOT, RESOURCES, add_build_arguments, complete_mesh, conversion_environment, executable, positive_int

REPOSITORY = 'KhronosGroup/glTF-Sample-Assets'


def request(url):
    for attempt in range(3):
        try:
            return urlopen(Request(url, headers={'User-Agent': 'Vulkanizer-model-sync'}), timeout=60)
        except (HTTPError, URLError, TimeoutError) as error:
            retryable = not isinstance(error, HTTPError) or error.code in (429, 500, 502, 503, 504)
            if not retryable or attempt == 2:
                raise
            time.sleep(2 ** attempt)


def fetch_json(url):
    with request(url) as response:
        return json.load(response)


def safe_name(name):
    if not name or name in ('.', '..') or any(c in name for c in '/\\:*?"<>|') or name.endswith(('.', ' ')):
        raise ValueError(f'Invalid model name: {name!r}')
    return name


def resource_path(source, uri, model_root):
    parsed = urlsplit(uri)
    if parsed.scheme == 'data':
        return None
    if parsed.scheme or parsed.netloc or parsed.query or parsed.fragment:
        raise ValueError(f'Unsupported external resource URI: {uri}')
    decoded = unquote(parsed.path)
    if '\\' in decoded or ':' in decoded or PurePosixPath(decoded).is_absolute():
        raise ValueError(f'Invalid resource URI: {uri}')
    result = (source.parent / decoded).resolve()
    if not result.is_relative_to(model_root.resolve()):
        raise ValueError(f'Resource escapes its model directory: {uri}')
    return result


def document(source):
    if source.suffix.lower() == '.gltf':
        return json.loads(source.read_text(encoding='utf-8-sig'))
    with source.open('rb') as stream:
        header = stream.read(12)
        if len(header) != 12 or struct.unpack('<III', header)[:2] != (0x46546c67, 2):
            raise ValueError(f'Invalid GLB: {source}')
        chunk = stream.read(8)
        if len(chunk) != 8:
            raise ValueError(f'Missing GLB JSON chunk: {source}')
        length, kind = struct.unpack('<II', chunk)
        if kind != 0x4e4f534a or length > source.stat().st_size - 20:
            raise ValueError(f'Invalid GLB JSON chunk: {source}')
        return json.loads(stream.read(length))


def download(repository_path, target, revision, files):
    if repository_path not in files:
        raise ValueError(f'Asset is not in the repository snapshot: {repository_path}')
    if target.is_file() and target.stat().st_size:
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    part = target.with_name(target.name + '.part')
    url = f'https://raw.githubusercontent.com/{REPOSITORY}/{revision}/' + quote(repository_path, safe='/')
    try:
        with request(url) as response, part.open('wb') as stream:
            shutil.copyfileobj(response, stream)
        part.replace(target)
    finally:
        part.unlink(missing_ok=True)


def select_variant(model):
    for variant in ('glTF', 'glTF-Embedded', 'glTF-Binary'):
        if variant in model['variants']:
            filename = model['variants'][variant]
            if PurePosixPath(filename).name != filename or '\\' in filename or ':' in filename:
                raise ValueError(f'Invalid source filename: {filename}')
            return variant, filename
    return None


def prepare_source(model, variant, filename, cache, revision, files, local_sources):
    name = safe_name(model['name'])
    root = cache / revision / 'Models' / name
    source = root / variant / filename
    if local_sources is not None:
        source = local_sources / name / variant / filename
        root = local_sources / name
        if not source.is_file():
            raise ValueError(f'Local source is missing: {source}')
    else:
        download(f'Models/{name}/{variant}/{filename}', source, revision, files)
    data = document(source)
    for item in data.get('buffers', []) + data.get('images', []):
        if 'uri' not in item:
            continue
        target = resource_path(source, item['uri'], root)
        if target is None:
            continue
        if local_sources is not None:
            if not target.is_file():
                raise ValueError(f'Local resource is missing: {target}')
        else:
            relative = target.relative_to(root).as_posix()
            download(f'Models/{name}/{relative}', target, revision, files)
    return source, root


def main(argv=None):
    parser = argparse.ArgumentParser(description='Check Khronos Sample Assets and convert missing local models.')
    parser.add_argument('--models', nargs='+', metavar='Model', help='Only process these model names (default: entire catalog)')
    parser.add_argument('--limit', type=positive_int, help='Maximum missing models to attempt')
    parser.add_argument('--dry-run', action='store_true', help='List missing models without downloading or converting them')
    parser.add_argument('--resources-dir', type=Path, default=RESOURCES)
    parser.add_argument('--cache-dir', type=Path, default=ROOT / 'build' / 'sample-assets')
    parser.add_argument('--source-dir', type=Path, help='Reuse a local glTF-Sample-Assets/Models folder instead of downloading model data')
    parser.add_argument('--converter', type=Path)
    parser.add_argument('--ktx', type=Path, help='Override the KTX command-line converter')
    parser.add_argument('--report', type=Path, help='JSON report (default: cache-dir/last_sync.json)')
    add_build_arguments(parser)
    args = parser.parse_args(argv)
    resources, cache = args.resources_dir.resolve(), args.cache_dir.resolve()
    try:
        commit = fetch_json(f'https://api.github.com/repos/{REPOSITORY}/commits/main')
        revision = commit['sha']
        tree = fetch_json(f'https://api.github.com/repos/{REPOSITORY}/git/trees/{commit["commit"]["tree"]["sha"]}?recursive=1')
        if tree.get('truncated'):
            raise ValueError('GitHub returned an incomplete repository tree; cannot safely inventory the assets.')
        files = {item['path'] for item in tree['tree'] if item['type'] == 'blob'}
        catalog = fetch_json(f'https://raw.githubusercontent.com/{REPOSITORY}/{revision}/Models/model-index.json')
        selected = {name.casefold() for name in args.models} if args.models else None
        if selected:
            unknown = selected - {model['name'].casefold() for model in catalog}
            if unknown:
                raise ValueError('Unknown model(s): ' + ', '.join(sorted(unknown)))
            catalog = [model for model in catalog if model['name'].casefold() in selected]
        present = {p.stem.casefold() for p in resources.rglob('*.vkmesh') if complete_mesh(p)}
        missing = [model for model in catalog if model['name'].casefold() not in present]
        if args.limit:
            missing = missing[:args.limit]
        print(f'Snapshot {revision[:12]}: {len(catalog)} catalog models selected, {sum(m["name"].casefold() in present for m in catalog)} already converted, {len(missing)} to check.', flush=True)
        if args.dry_run:
            for model in missing:
                variant = select_variant(model)
                print(f'{model["name"]}: {variant[0] if variant else "no supported source variant"}')
            return 0
        if not missing:
            return 0
        converter = executable('vulkanizer_mesh_convert_test', args.config, args.build_dir, args.converter)
        environment = conversion_environment(args.ktx)
        output = resources / 'models'
        output.mkdir(parents=True, exist_ok=True)
        logs = cache / 'logs'
        logs.mkdir(parents=True, exist_ok=True)
        results = []
        for model in missing:
            name = safe_name(model['name'])
            entry = {'model': name}
            print(f'Converting {name}...', flush=True)
            try:
                variant = select_variant(model)
                if variant is None:
                    raise ValueError('No uncompressed glTF, embedded glTF, or GLB variant')
                source, source_root = prepare_source(model, *variant, cache, revision, files, args.source_dir.resolve() if args.source_dir else None)
                with tempfile.TemporaryDirectory(prefix='.sync-', dir=output) as temporary:
                    staging = Path(temporary).resolve()
                    if not staging.is_relative_to(output.resolve()):
                        raise ValueError('Invalid conversion staging directory')
                    log_path = logs / f'{name}.log'
                    with log_path.open('w', encoding='utf-8') as log:
                        result = subprocess.run([str(converter), str(source), str(staging / f'{name}.vkmesh')], env=environment, stdout=log, stderr=subprocess.STDOUT)
                    if result.returncode or not complete_mesh(staging / f'{name}.vkmesh'):
                        raise ValueError(f'Conversion failed (exit {result.returncode}); see {log_path}')
                    prefix = f'Models/{name}/'
                    legal_files = [p for p in files if p.startswith(prefix) and '/' not in p[len(prefix):]
                                   and (PurePosixPath(p).name.lower() in ('metadata.json', 'readme.md', 'notice') or PurePosixPath(p).name.lower().startswith('license'))]
                    for repository_path in legal_files:
                        filename = PurePosixPath(repository_path).name
                        target = staging / ('SOURCE.md' if filename.lower() == 'readme.md' else filename)
                        local = source_root / filename
                        if args.source_dir and local.is_file():
                            shutil.copy2(local, target)
                        else:
                            download(repository_path, target, revision, files)
                    provenance = {'repository': f'https://github.com/{REPOSITORY}', 'revision': revision,
                                  'model': name, 'variant': variant[0], 'source': str(source),
                                  'local_source': bool(args.source_dir),
                                  'conversion': 'Converted to Vulkanizer mesh/material/bounds files; textures embedded as KTX2. Static node transforms only.'}
                    (staging / 'conversion.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
                    destination = output / name
                    destination.mkdir(parents=True, exist_ok=True)
                    for path in sorted(staging.iterdir(), key=lambda p: p.suffix == '.vkmesh'):
                        path.replace(destination / path.name)
                entry.update(status='converted', mesh=str(destination / f'{name}.vkmesh'))
                print(f'  Saved {entry["mesh"]}', flush=True)
            except (OSError, ValueError, subprocess.SubprocessError) as error:
                entry.update(status='failed', error=str(error))
                print(f'  Failed: {error}', file=sys.stderr, flush=True)
            results.append(entry)
        report = (args.report or cache / 'last_sync.json').resolve()
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps({'revision': revision, 'results': results}, indent=2) + '\n', encoding='utf-8')
        failed = sum(item['status'] == 'failed' for item in results)
        print(f'Converted {len(results) - failed}; failed {failed}. Report: {report}')
        return 1 if failed else 0
    except (OSError, ValueError, KeyError) as error:
        print(f'Error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
