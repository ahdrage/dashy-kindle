"""Predecode the pinned upstream archive for PW1 firmware without base64."""
import base64
import hashlib
import io
import json
import lzma
from pathlib import Path
import re
import tarfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA256 = "65a63528fbe9515950cc3aa0d931749548680f37898a3819a4ebc0a740588942"


def main():
    source = (ROOT / ".deps/winterbreak2-jb.sh").read_bytes()
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError("Unexpected upstream bootstrap; review it before adapting")
    lines = source.decode().splitlines(keepends=True)
    encoded = re.search(r'echo "([A-Za-z0-9+/=]+)"', lines[4]).group(1)
    payload = lzma.decompress(base64.b64decode(encoded, validate=True))
    with tarfile.open(fileobj=io.BytesIO(payload)) as archive:
        for member in archive.getmembers():
            path = Path(member.name)
            if path.is_absolute() or ".." in path.parts or not (member.isfile() or member.isdir()):
                raise ValueError(f"Unexpected archive entry: {member.name}")
    out = ROOT / "output/unlock/winterbreak2"
    out.mkdir(parents=True, exist_ok=True)
    (out / "kmc.tar").write_bytes(payload)
    prefix = '''#!/bin/sh
# The original archive, decoded on the Mac because PW1 has no base64 tool.
rm -rf /tmp/kmc || exit 1
mkdir /tmp/kmc || exit 1
echo "Unpacking JB from local archive..."
if ! tar xf /mnt/us/winterbreak2/kmc.tar -C /tmp/kmc; then
    echo "STOP: unlock archive could not be unpacked; system install not started"
    exit 1
fi
for required in kindlepw2/bin/sh_integration_launcher kindlepw2/bin/kmc_system_patcher kindlepw2/lib/sh_integration_extractor.so system_patches/patch_system.sh sql/appreg_register_sh_integration_common.sql; do
    if [ ! -s "/tmp/kmc/$required" ]; then
        echo "STOP: incomplete unlock archive; missing $required"
        exit 1
    fi
done
echo "Done"
'''
    # Only replace the original six-line unpack stage. Keep the installer body intact.
    adapted = prefix + "".join(lines[6:])
    (out / "jb.sh").write_text(adapted)
    metadata = {
        "original_sha256": SOURCE_SHA256,
        "adaptation": "Decode the original payload on Mac; use plain tar on PW1 and abort if unpacking fails",
        "installer_body_unchanged": adapted.split("# Packed from 00_helpers.sh", 1)[1] == source.decode().split("# Packed from 00_helpers.sh", 1)[1],
        "files": {name: {"bytes": (out / name).stat().st_size,
                         "sha256": hashlib.sha256((out / name).read_bytes()).hexdigest()}
                  for name in ("jb.sh", "kmc.tar")},
    }
    (out.parent / "pw1-retry.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
