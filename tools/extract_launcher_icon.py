"""Extract the supported original EXE's icon for the Android launcher."""
from pathlib import Path
import hashlib
import io
import struct
import pefile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

def main():
    source = ROOT.parent / 'Transformers.exe'
    blob = source.read_bytes()
    if hashlib.sha256(blob).hexdigest() != '5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0':
        raise ValueError('Unsupported original executable')
    pe = pefile.PE(data=blob)
    resources = {entry.id: entry for entry in pe.DIRECTORY_ENTRY_RESOURCE.entries if entry.id}
    def data(entry):
        while hasattr(entry, 'directory'):
            entry = entry.directory.entries[0]
        resource = entry.data.struct
        return pe.get_data(resource.OffsetToData, resource.Size)
    group = data(resources[14])
    count = struct.unpack_from('<H', group, 4)[0]
    icons = {entry.id: data(entry) for entry in resources[3].directory.entries}
    offset, entries, payloads = 6 + count * 16, [], []
    for index in range(count):
        width, height, colors, reserved, planes, bits, size, ident = struct.unpack_from('<BBBBHHIH', group, 6 + index * 14)
        payload = icons[ident]
        entries.append(struct.pack('<BBBBHHII', width, height, colors, reserved, planes, bits, len(payload), offset))
        payloads.append(payload)
        offset += len(payload)
    ico = struct.pack('<HHH', 0, 1, count) + b''.join(entries) + b''.join(payloads)
    # Keep this historical 32px extraction separate from the current HD icon.
    destination = ROOT / 'work/original-exe-icon.png'
    destination.parent.mkdir(parents=True, exist_ok=True)
    Image.open(io.BytesIO(ico)).save(destination)
    print(destination)

if __name__ == '__main__':
    main()
