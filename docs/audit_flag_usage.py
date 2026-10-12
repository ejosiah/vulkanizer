"""Reproduce the sibling-project static flag inventory (Python standard library)."""
import collections
import csv
import pathlib
import re
import subprocess

BASE = pathlib.Path(__file__).resolve().parents[2]
OUT = pathlib.Path(__file__).resolve().parent / 'flag-usage'
PROJECTS = ['light_transport', 'vista', 'VulkanXR']
TOKEN = r'(?:VK|XR|VMA)_[A-Z0-9_]+(?:_BIT(?:_[A-Z0-9]+)?|_NONE(?:_[A-Z0-9]+)?|_ALL(?:_[A-Z0-9]+)?)\b|VMA_MEMORY_USAGE_[A-Z0-9_]+'
ATOM = '(?:' + TOKEN + ')'
EXPR = re.compile(ATOM + r'(?:\s*\|(?!\|)\s*' + ATOM + ')*')
STRIP = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')

def family(token):
    if token.startswith('VMA_MEMORY_USAGE_'):
        return 'VMA_MEMORY_USAGE (enum)'
    for prefix in ('VK_BUFFER_USAGE_2', 'VK_BUFFER_USAGE', 'VK_IMAGE_USAGE', 'VK_PIPELINE_STAGE_2', 'VK_PIPELINE_STAGE', 'VK_ACCESS_2', 'VK_ACCESS', 'VK_SHADER_STAGE', 'VK_IMAGE_ASPECT', 'VK_SAMPLE_COUNT', 'VK_COLOR_COMPONENT', 'VK_QUEUE', 'VK_MEMORY_PROPERTY', 'VK_MEMORY_HEAP', 'VK_IMAGE_CREATE', 'VK_BUFFER_CREATE', 'VK_COMMAND_POOL_CREATE', 'VK_COMMAND_BUFFER_USAGE', 'VK_DESCRIPTOR_BINDING', 'VK_DESCRIPTOR_POOL_CREATE', 'VK_DESCRIPTOR_SET_LAYOUT_CREATE', 'VK_BUILD_ACCELERATION_STRUCTURE', 'VK_GEOMETRY_INSTANCE', 'VK_GEOMETRY', 'VK_CULL_MODE', 'VK_FENCE_CREATE', 'VK_DEPENDENCY', 'VK_FORMAT_FEATURE_2', 'VK_FORMAT_FEATURE', 'VK_DEBUG_UTILS_MESSAGE_SEVERITY', 'VK_DEBUG_UTILS_MESSAGE_TYPE', 'XR_SWAPCHAIN_USAGE', 'XR_SWAPCHAIN_CREATE', 'XR_SPACE_LOCATION', 'XR_VIEW_STATE', 'XR_COMPOSITION_LAYER', 'XR_DEBUG_UTILS_MESSAGE_SEVERITY', 'XR_DEBUG_UTILS_MESSAGE_TYPE', 'VMA_ALLOCATION_CREATE', 'VMA_ALLOCATOR_CREATE'):
        if token.startswith(prefix + '_'):
            return prefix
    return re.sub(r'_(?:BIT|NONE|ALL)(?:_[A-Z0-9]+)?$', '', token)

def main():
    OUT.mkdir(exist_ok=True)
    rows, dynamic, files = [], [], collections.Counter()
    for project in PROJECTS:
        root = BASE / project
        names = subprocess.check_output(['rg', '--files', '-g', '*.cpp', '-g', '*.hpp', '-g', '*.h', '-g', '*.c', '-g', '*.cc', '-g', '*.cxx', '-g', '*.inl'], cwd=root, text=True).splitlines()
        for name in sorted(names):
            if any(p.lower() in {'vendor', 'third_party', 'external', 'dependencies', 'build', 'cmake-build-debug', 'cmake-build-release'} for p in pathlib.Path(name).parts):
                continue
            files[project] += 1
            source = (root / name).read_text(encoding='utf-8', errors='replace')
            clean = STRIP.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m.group()), source)
            for match in EXPR.finditer(clean):
                tokens = re.findall(ATOM, match.group())
                groups = collections.defaultdict(set)
                for token in tokens:
                    groups[family(token)].add(token)
                for group, values in groups.items():
                    rows.append([project, name.replace('\\', '/'), clean.count('\n', 0, match.start()) + 1, group, ' | '.join(sorted(values)), ' '.join(match.group().split())])
            for line, text in enumerate(clean.splitlines(), 1):
                if re.search(r'\busage\s*\(|\busage\s*\|=|\bresult\s*\|=|Vk(?:Buffer|Image)UsageFlags|\busage_flags_\s*\|=', text):
                    dynamic.append([project, name.replace('\\', '/'), line, text.strip()])
    counts = collections.defaultdict(collections.Counter)
    for project, _, _, group, combo, _ in rows:
        counts[group, combo][project] += 1
    with (OUT / 'occurrences.csv').open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f); writer.writerow(['project', 'file', 'line', 'family', 'combination', 'source_expression']); writer.writerows(rows)
    with (OUT / 'combinations.csv').open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f); writer.writerow(['flag_type', 'combination', 'usage_count'])
        for (group, combo), count in sorted(counts.items(), key=lambda x: (x[0][0], -sum(x[1].values()), x[0][1])):
            writer.writerow([group, combo, sum(count.values())])
    with (OUT / 'usage-context.csv').open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f); writer.writerow(['project', 'file', 'line', 'context']); writer.writerows(dynamic)
    report = ['Static flag inventory — 2026-10-12', '', f'Scanned files: {sum(files.values())}', f'Flag/enum expressions: {len(rows)}; distinct family/combinations: {len(counts)}.', '',
        'Method: rg-visible C/C++ source and headers, including examples, tests and tools.',
        'Excluded: ignored files, build and dependency directories; comments and string literals.',
        'Frequency is lexical source-expression count, including declarations, checks, defaults and helper arguments.',
        'Contiguous literal OR chains are order-normalized and counted once; ternary branches count separately.',
        'NONE/ALL sentinels are included. VMA memory usage is an enum and is labeled separately.',
        'This is not a runtime resource count or an interprocedural effective-mask analysis.',
        'Named masks, helper-added bits, conditional accumulation, zero masks and implicit library defaults need separate interpretation.',
        'occurrences.csv contains all locations; usage-context.csv contains usage builders and dynamic-mask context.', '',
        'Known effective-mask cases:',
        'vista/src/cbt/mesh.cpp:41-44: StorageBufferUsage = STORAGE_BUFFER | TRANSFER_SRC | TRANSFER_DST; derived masks add INDIRECT_BUFFER, VERTEX_BUFFER or INDEX_BUFFER.',
        'vista/src/terrain_editor/tree_population.cpp:115-119: buffer helper adds TRANSFER_DST and TRANSFER_SRC to its usage argument.',
        'vista/src/atmosphere.cpp:428-431: create_buffer helper adds TRANSFER_DST.',
        'VulkanXR/src/desktop_application.cpp:17-31: image_usage starts with SAMPLED and conditionally adds COLOR_ATTACHMENT, DEPTH_STENCIL_ATTACHMENT, TRANSFER_SRC and TRANSFER_DST from XR usage.',
        'VulkanXR/src/vxr/swapchain.hpp: usage methods accumulate XR swapchain bits, so chained methods are not literal OR expressions.', '']
    for group in sorted({g for g, _ in counts}, key=lambda g: (g not in ('VK_BUFFER_USAGE', 'VK_IMAGE_USAGE'), g)):
        report += [group, '  Usage count : combination']
        for (g, combo), count in sorted(counts.items(), key=lambda x: (-sum(x[1].values()), x[0][1])):
            if g == group:
                report.append(f"  {sum(count.values())} : {combo}")
        report.append('')
    (OUT / 'inventory.txt').write_text('\n'.join(report) + '\n', encoding='utf-8')
    print('\n'.join(report[:7]))

if __name__ == '__main__':
    main()
