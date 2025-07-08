import tempfile
import urllib.request
import zipfile
import os
import shutil
import subprocess
import configparser

required_files = ["livekit_rtc.proto", "livekit_models.proto", "livekit_metrics.proto"]
protobuf_location = "../protobufs"
bindings_src_dest = "../"

def main():
    version = read_version()
    with tempfile.TemporaryDirectory() as temp_dir:
        repo_archive = download_archive(version, temp_dir)
        unzip_file(repo_archive, temp_dir)
        repo_root = os.path.join(temp_dir, f"protocol--livekit-protocol-{version}")
        patch_proto_imports(repo_root)
        copy_proto_definitions(repo_root, protobuf_location)
    generate_bindings()

def read_version():
    config = configparser.ConfigParser()
    config.read("./version.ini")
    return config["download"]["version"]

def download_archive(version,dest):
    file_name = f"protocol@{version}.zip"
    url = f"https://github.com/livekit/protocol/archive/refs/tags/@livekit/{file_name}"
    download_path = os.path.join(dest, file_name)
    print("Downloading " + url)
    urllib.request.urlretrieve(url, download_path)
    return download_path

def patch_proto_imports(repo_root):
    replacements = {
        'import "google/protobuf/timestamp.proto";': 'import "timestamp.proto";'
        # Add more replacements here if needed
    }
    for fname in required_files:
        src = os.path.join(repo_root, "protobufs", fname)
        try:
            with open(src, "r") as f:
                content = f.read()
            modified = False
            new_content = content
            for old, new in replacements.items():
                if old in new_content:
                    new_content = new_content.replace(old, new)
                    modified = True
                    print(f"Patched {fname}")
            if modified:
                with open(src, "w") as f:
                    f.write(new_content)
        except IOError as e:
            print(f"Error processing {fname}: {e}")

def copy_proto_definitions(repo_root, dest):
    for fname in required_files:
        print("Copying " + fname)
        src = os.path.join(repo_root, "protobufs", fname)
        shutil.copy2(src, dest)

def unzip_file(srcfile, dest):
    with zipfile.ZipFile(srcfile, 'r') as zip_ref:
        zip_ref.extractall(dest)

def generate_bindings(verbose = False):
    protoc_path = shutil.which("protoc")
    if not protoc_path:
        raise RuntimeError("Please install the Protobuf compiler")

    input_files = ["timestamp.proto"] + required_files
    protoc_cmd = [
        protoc_path,
        *(["--nanopb_opt=-v"] if verbose else []), # Verbose output
        "--nanopb_opt=--c-style",
        "--nanopb_opt=--error-on-unmatched",
        "--nanopb_opt=-s discard_deprecated:true",
        "--nanopb_opt=-s package:'livekit_pb'", # Prefixes generated types
        f"--nanopb_out={os.path.abspath(bindings_src_dest)}",
    ] + input_files

    print(f"Generating bindings: {" ".join(protoc_cmd)}")
    result = subprocess.run(
        protoc_cmd,
        capture_output=True,
        text=True,
        cwd=os.path.abspath(protobuf_location)
    )
    if verbose:
        print(result.stderr)
    if result.returncode != 0:
        raise RuntimeError(f"protoc failed: {result.stderr}")

if __name__ == "__main__":
    main()