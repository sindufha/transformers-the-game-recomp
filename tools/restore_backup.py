"""Restore this private backup without executing the original game or using ADB."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def git(*args):
    return subprocess.check_output(['git', *map(str, args)], text=True, stderr=subprocess.STDOUT)

def portable_spirv(summary):
    summary = json.loads(json.dumps(summary))
    summary.pop('source', None); summary.pop('ndk', None)
    for archive in summary['archives']: archive.pop('path', None)
    return summary

def verify_backup():
    manifest = json.loads((ROOT / 'backup/files.sha256.json').read_text(encoding='utf-8'))
    for name, expected in manifest['files'].items():
        path = ROOT / name
        if not path.is_file() or digest(path) != expected:
            raise ValueError('Missing or changed backup file: ' + name + '. Run git lfs pull if binaries are pointer files.')
    for name, expected in manifest.get('relocatable_spirv_metadata', {}).items():
        actual = json.loads((ROOT / name).read_text(encoding='utf-8'))
        if portable_spirv(actual) != expected:
            raise ValueError('Changed SPIRV-Tools provenance: ' + name)
    print('Verified', len(manifest['files']), 'backup files and relocatable SPIRV metadata.', flush=True)

def restore_dependencies():
    records = json.loads((ROOT / 'backup/dependencies.json').read_text(encoding='utf-8'))
    for record in records:
        target = ROOT / record['path']
        if target.is_dir() and not any(target.iterdir()):
            target.rmdir()  # Empty submodule placeholder only; never recursive.
        if not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            print(git('-c', 'core.autocrlf=false', 'clone', '--no-checkout', ROOT / record['bundle'], target).strip())
            print(git('-C', target, '-c', 'core.autocrlf=false', 'checkout', '--detach', record['revision']).strip())
        actual = git('-C', target, 'rev-parse', 'HEAD').strip()
        if actual != record['revision']:
            raise ValueError('Existing dependency has a different revision: ' + str(target))
        if git('-C', target, 'status', '--porcelain', '--untracked-files=no').strip():
            raise ValueError('Existing dependency is modified; refusing to overwrite: ' + str(target))
    print('Pinned dependency source checkouts restored from repository bundles.', flush=True)

def restore_input(path):
    lift = ROOT / 'work/lift-5aa3bd490f5b-target'
    expected = json.loads((lift / 'lift-summary.json').read_text(encoding='utf-8'))['input_sha256']
    if digest(path) != expected:
        raise ValueError('Unsupported Transformers.exe. Required SHA-256: ' + expected)
    target = lift / 'input.exe'
    if target.exists():
        if digest(target) != expected:
            raise ValueError('Existing frozen input differs; refusing to overwrite it.')
    else:
        shutil.copy2(path, target)
    print('Verified original EXE restored as local build input; it remains Git-ignored.', flush=True)

def rebase_spirv(ndk):
    if ndk is not None:
        source = ndk / 'sources/third_party/shaderc/third_party/spirv-tools'
        if not (source / 'include').is_dir():
            raise ValueError('Required SPIRV-Tools source headers are missing from this NDK: ' + str(ndk))
    for abi in ('arm64-v8a', 'armeabi-v7a', 'x86', 'x86_64'):
        folder = ROOT / ('work/spirv-' + abi)
        path = folder / 'build-summary.json'
        summary = json.loads(path.read_text(encoding='utf-8'))
        for archive in summary['archives']:
            archive['path'] = str(folder / Path(archive['path'].replace('\\', '/')).name)
            if digest(Path(archive['path'])) != archive['sha256']:
                raise ValueError('SPIRV-Tools archive checksum mismatch')
        if ndk is not None:
            summary['source'] = str(source)
            summary['ndk'] = str(ndk)
        path.write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    print('SPIRV-Tools archive locations rebased; binary hashes retained.', flush=True)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game-exe', type=Path, help='Your supported original Transformers.exe (not included in the repository).')
    ap.add_argument('--ndk', type=Path, help='NDK 27.1.12297006 root; needed for portable full native/backend rebuilding.')
    ap.add_argument('--verify-only', action='store_true', help='Verify tracked backup contents; do not restore dependencies or EXE.')
    args = ap.parse_args()
    verify_backup()
    if args.verify_only:
        return
    if args.game_exe is None:
        ap.error('--game-exe is required when restoring the build workspace')
    restore_input(args.game_exe.resolve())
    restore_dependencies()
    ndk = args.ndk.resolve() if args.ndk else None
    if ndk is None:
        sdk = os.environ.get('ANDROID_HOME') or os.environ.get('ANDROID_SDK_ROOT')
        candidate = Path(sdk) / 'ndk/27.1.12297006' if sdk else None
        if candidate and candidate.is_dir():
            ndk = candidate.resolve()
    rebase_spirv(ndk)
    recipe_path = ROOT / 'ports/dxvk_android/prepare.py'
    spec = importlib.util.spec_from_file_location('backup_dxvk_prepare', recipe_path)
    recipe = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(recipe)
    recipe.prepare(ROOT / 'external/dxvk-native', ROOT / 'work/dxvk-source', ROOT / 'work')
    print('Restoration complete. Follow README.md to package the APK; no app/game was launched.', flush=True)

if __name__ == '__main__':
    main()
