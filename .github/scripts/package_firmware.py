# Package the firmware builds for the browser flasher: copy every flash part of each build into a versioned folder and write the manifest that lists them
import argparse
import json
import shutil
import subprocess
from pathlib import Path


# Read the flash parts of a PlatformIO environment (bootloader, partition table, boot_app0 and the app, with their addresses)
def read_flash_parts(project_dir, environment):
    output = subprocess.run(['pio', 'project', 'metadata', '-e', environment, '--json-output'], cwd=project_dir, check=True, capture_output=True, text=True).stdout
    metadata = json.loads(output)[environment]

    # The extra images come with their addresses, the app goes at the application offset
    parts = [{'path': Path(image['path']), 'offset': int(image['offset'], 16)} for image in metadata['extra']['flash_images']]
    application_path = Path(metadata['prog_path']).with_suffix('.bin')
    parts.append({'path': application_path, 'offset': int(metadata['extra']['application_offset'], 16)})

    # Check every part was built
    for part in parts:
        if not part['path'].is_file():
            raise SystemExit(f'Missing flash part: {part["path"]} (build the firmware first)')

    # Return the parts in flash order
    return sorted(parts, key=lambda part: part['offset'])


# Copy the parts of each build into the versioned folder (named after their environment, since the file names repeat), the debug symbols beside it, and write one manifest that lists every build, in the ESP Web Tools format: the flasher picks the build of the chip it finds
def package_firmware(project_dir, builds, name, version, output_dir):
    manifest_builds = []
    version_dir, debug_dir = output_dir / version, output_dir / 'debug'
    version_dir.mkdir(parents=True, exist_ok=True)
    debug_dir.mkdir(parents=True, exist_ok=True)
    for environment, chip_family in builds:
        # Copy each part as <environment>-<part>.bin
        parts = read_flash_parts(project_dir, environment)
        manifest_parts = []
        for part in parts:
            file_name = f'{environment}-{part["path"].name}'
            shutil.copy2(part['path'], version_dir / file_name)
            manifest_parts.append({'path': f'{version}/{file_name}', 'offset': part['offset']})
        manifest_builds.append({'chipFamily': chip_family, 'parts': manifest_parts})

        # The debug symbols of the app, for the release (not for the flasher)
        shutil.copy2(parts[-1]['path'].with_suffix('.elf'), debug_dir / f'{environment}-firmware.elf')

        # Show what was packaged
        print(f'{environment} ({chip_family})')
        for part in manifest_parts:
            print(f'  0x{part["offset"]:06x}  {part["path"]}')

    # Write the manifest of this version (updates keep the saved settings, so nothing is erased)
    manifest = {
        'name': name,
        'version': version,
        'new_install_prompt_erase': False,
        'builds': manifest_builds
    }
    (output_dir / 'manifest.json').write_text(json.dumps(manifest, indent='\t') + '\n')


# Read the arguments and package the builds
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Package firmware builds for the OpenGizmo browser flasher')
    parser.add_argument('--project-dir', type=Path, required=True, help='Folder of the PlatformIO project')
    parser.add_argument('--build', action='append', required=True, metavar='ENV:CHIP', help='A PlatformIO environment that was built and its chip family, e.g. release:ESP32-S3 (repeat for each build)')
    parser.add_argument('--name', required=True, help='Project name shown by the flasher')
    parser.add_argument('--version', required=True, help='Release version, also the name of the parts folder')
    parser.add_argument('--output-dir', type=Path, required=True, help='Folder to write the manifest and the parts folder to')
    arguments = parser.parse_args()
    builds = [tuple(build.split(':', 1)) for build in arguments.build]
    package_firmware(arguments.project_dir, builds, arguments.name, arguments.version, arguments.output_dir)
