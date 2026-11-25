# Read the notes of a release from CHANGELOG.md, and stop the release when they are missing: no version goes out without notes.
# It can also write every released version of the changelog to a JSON file, which the website reads when it is built (its "What's new" page)
import argparse
import json
import re
from pathlib import Path


# Split the changelog into its sections: the title of each "## " heading (its first word, and the whole heading), and the lines under it
def read_sections(changelog):
    sections, title = {}, None
    for line in changelog.read_text(encoding='utf-8').splitlines():
        heading = re.match(r'^## (\S+)(.*)$', line)
        if heading:
            title = heading.group(1)
            sections[title] = {'heading': line[3:].strip(), 'lines': []}
        elif title:
            sections[title]['lines'].append(line)

    # Return each section without the blank lines around it
    return {title: {'heading': section['heading'], 'notes': '\n'.join(section['lines']).strip()} for title, section in sections.items()}


# Return the notes of a version (a "## vX.Y.Z" section, the date may follow), and refuse a release whose notes are missing or still under Unreleased
def read_release_notes(changelog, version):
    if not changelog.is_file():
        raise SystemExit(f'{changelog} is missing: write the notes of {version} there before releasing it.')
    sections = read_sections(changelog)

    # The notes of the version must be there, and not empty
    notes = sections.get(version, {}).get('notes', '')
    if not notes:
        raise SystemExit(f'{changelog} has no notes for {version}: move the Unreleased notes under a "## {version}" heading before releasing it.')

    # Anything left under Unreleased would be released without being listed
    if sections.get('Unreleased', {}).get('notes', ''):
        raise SystemExit(f'{changelog} still has notes under Unreleased: move them under "## {version}" before releasing it.')
    return notes


# Return every version of the changelog that has notes, newest first as the changelog lists them: { version, date, url, notes } (the date is the one after the version, or None)
def read_releases(changelog, repository):
    releases = []
    for title, section in read_sections(changelog).items():
        # Only the "## vX.Y.Z" sections: Unreleased and the other headings are not releases
        if not re.fullmatch(r'v\d+\.\d+\.\d+', title) or not section['notes']:
            continue
        date = re.search(r'\d{4}-\d{2}-\d{2}', section['heading'])
        releases.append({
            'version': title,
            'date': date.group(0) if date else None,
            'url': f'https://github.com/{repository}/releases/tag/{title}',
            'notes': section['notes']
        })
    return releases


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Read the notes of an OpenGizmo release from CHANGELOG.md')
    parser.add_argument('--changelog', type=Path, default=Path('CHANGELOG.md'), help='The changelog to read')
    parser.add_argument('--version', required=True, help='The version to release, for example v1.2.0')
    parser.add_argument('--output', type=Path, required=True, help='The file to write the notes to')
    parser.add_argument('--releases-json', type=Path, help='Also write every released version to this JSON file, for the website')
    parser.add_argument('--repository', help='The GitHub repository of the releases (owner/name), for their links in the JSON file')
    arguments = parser.parse_args()
    if arguments.releases_json and not arguments.repository:
        parser.error('--releases-json needs --repository, for the links of the releases')

    # Write the notes, ready to be the body of the GitHub release
    notes = read_release_notes(arguments.changelog, arguments.version)
    arguments.output.write_text(notes + '\n', encoding='utf-8')
    print(f'Release notes of {arguments.version}:\n\n{notes}')

    # Write every released version for the website, the one being released first
    if arguments.releases_json:
        releases = read_releases(arguments.changelog, arguments.repository)
        if releases[0]['version'] != arguments.version:
            raise SystemExit(f'{arguments.changelog} lists {releases[0]["version"]} above {arguments.version}: the newest version goes first, right under Unreleased.')
        arguments.releases_json.write_text(json.dumps({'releases': releases}, ensure_ascii=False, indent='\t') + '\n', encoding='utf-8')
        print(f'\n{len(releases)} releases written to {arguments.releases_json}: {", ".join(release["version"] for release in releases)}')
