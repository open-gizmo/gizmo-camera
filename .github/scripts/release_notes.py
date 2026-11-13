# Read the notes of a release from CHANGELOG.md, and stop the release when they are missing: no version goes out without notes
import argparse
import re
from pathlib import Path


# Split the changelog into its sections: the title of each "## " heading, and the lines under it
def read_sections(changelog):
    sections, title = {}, None
    for line in changelog.read_text(encoding='utf-8').splitlines():
        heading = re.match(r'^## (\S+)', line)
        if heading:
            title = heading.group(1)
            sections[title] = []
        elif title:
            sections[title].append(line)

    # Return each section without the blank lines around it
    return {title: '\n'.join(lines).strip() for title, lines in sections.items()}


# Return the notes of a version (a "## vX.Y.Z" section, the date may follow), and refuse a release whose notes are missing or still under Unreleased
def read_release_notes(changelog, version):
    if not changelog.is_file():
        raise SystemExit(f'{changelog} is missing: write the notes of {version} there before releasing it.')
    sections = read_sections(changelog)

    # The notes of the version must be there, and not empty
    notes = sections.get(version, '')
    if not notes:
        raise SystemExit(f'{changelog} has no notes for {version}: move the Unreleased notes under a "## {version}" heading before releasing it.')

    # Anything left under Unreleased would be released without being listed
    if sections.get('Unreleased', ''):
        raise SystemExit(f'{changelog} still has notes under Unreleased: move them under "## {version}" before releasing it.')
    return notes


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Read the notes of an OpenGizmo release from CHANGELOG.md')
    parser.add_argument('--changelog', type=Path, default=Path('CHANGELOG.md'), help='The changelog to read')
    parser.add_argument('--version', required=True, help='The version to release, for example v1.2.0')
    parser.add_argument('--output', type=Path, required=True, help='The file to write the notes to')
    arguments = parser.parse_args()

    # Write the notes, ready to be the body of the GitHub release
    notes = read_release_notes(arguments.changelog, arguments.version)
    arguments.output.write_text(notes + '\n', encoding='utf-8')
    print(f'Release notes of {arguments.version}:\n\n{notes}')
