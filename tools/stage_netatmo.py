"""Copy private Netatmo settings to a mounted PW1 for import at the next launch."""
import argparse
import json
import os
from pathlib import Path
import re
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def stage(credentials, kindle):
    if credentials.stat().st_size>16384:
        raise ValueError("Credential file is too large")
    data=json.loads(credentials.read_text())
    if not isinstance(data,dict):
        raise ValueError("Credentials must be a JSON object")
    for key in ("client_id","client_secret","refresh_token"):
        value=data.get(key)
        if not isinstance(value,str) or not 1<=len(value)<=4096 or any(ord(c)<32 or ord(c)>126 for c in value):
            raise ValueError("Fill in client_id, client_secret and refresh_token in the private file")
    if any(ord(c)==32 for c in data["refresh_token"]):
        raise ValueError("The refresh token cannot contain spaces")
    station=data.get("station_id","")
    if not isinstance(station,str) or len(station)>128 or any(ord(c)<33 or ord(c)>126 for c in station):
        raise ValueError("station_id must be empty or a valid station identifier")
    version=(kindle/"system/version.txt").read_text()
    if not re.search(r"Kindle 5\.6\.1\.1 \(",version):
        raise ValueError("Expected Kindle firmware 5.6.1.1; no credentials copied")
    directory=kindle/"dashy"
    if directory.is_symlink():
        raise ValueError("Unexpected Kindle settings directory")
    directory.mkdir(exist_ok=True)
    target=directory/"netatmo.pending.json"
    if target.is_symlink():
        raise ValueError("Unexpected pending-settings path")
    # Copy only known fields; extra source metadata never reaches the device.
    payload=json.dumps({key:data.get(key,"") for key in ("client_id","client_secret","refresh_token","station_id")},indent=2)+"\n"
    temporary=None
    try:
        with tempfile.NamedTemporaryFile(mode="w",dir=directory,prefix=".netatmo-",delete=False) as stream:
            temporary=Path(stream.name)
            stream.write(payload); stream.flush(); os.fsync(stream.fileno())
        if temporary.read_text()!=payload:
            raise ValueError("Credential copy did not verify")
        os.replace(temporary,target)
        temporary=None
    finally:
        if temporary is not None: temporary.unlink(missing_ok=True)
    print("Netatmo settings staged privately. They will be imported and removed from USB staging when Dashy opens.")


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--credentials",type=Path,default=ROOT/"secrets/netatmo.json")
    parser.add_argument("--kindle",type=Path,required=True)
    args=parser.parse_args()
    try:
        stage(args.credentials,args.kindle)
    except (OSError,ValueError) as error:
        # Never print JSON input, request bodies or credentials.
        if isinstance(error,json.JSONDecodeError): parser.exit(2,"Invalid JSON in the private credentials file.\n")
        parser.exit(2,str(error)+"\n")


if __name__=="__main__": main()
