import os
import shutil
from pathlib import Path

# Paths
real_project_path = Path("realProject")
src_path = real_project_path / "src"
source_cpp_dir = Path.home() / "Documents" / "platformioProjects" / "espTicker32" / "src"
source_h_dir = Path.home() / "Documents" / "platformioProjects" / "espTicker32" / "include"

def log(msg):
    print(f"[INFO] {msg}")

def main():
    if real_project_path.exists():
        log(f"'{real_project_path}' already exists. Exiting.")
        return

    # Create realProject/src
    src_path.mkdir(parents=True, exist_ok=False)
    log(f"Created folder: {src_path}")

    # Copy .cpp files
    cpp_files = list(source_cpp_dir.glob("*.cpp"))
    for file in cpp_files:
        shutil.copy(file, src_path)
        log(f"Copied .cpp file: {file.name}")

    # Copy .h files
    h_files = list(source_h_dir.glob("*.h"))
    for file in h_files:
        shutil.copy(file, src_path)
        log(f"Copied .h file: {file.name}")

    log("All files copied successfully.")

if __name__ == "__main__":
    main()