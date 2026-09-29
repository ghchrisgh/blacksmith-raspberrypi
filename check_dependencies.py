import sys
import subprocess
import importlib.util
from pathlib import Path
REQUIREMENTS_FILE = "requirements.txt"



def check_python_version():
    version = sys.version_info
    if version.major < 3 or (version.major == 3 and version.minor < 7):
        print(f"Python {version.major}.{version.minor} detected")
        print("   Required: Python 3.7+")
        return False
    else:
        print(f"Python {version.major}.{version.minor}.{version.micro}")
        return True

def check_package(package_name):
    try:
        spec = importlib.util.find_spec(package_name)
        if spec is not None:
            module = importlib.import_module(package_name)
            version = getattr(module, "__version__", "unknown")
            print(f"{package_name}: {version}")
            return True
        else:
            print(f"{package_name}: Not found")
            return False
    except ImportError:
        print(f"{package_name}: Import error")
        return False

def parse_requirements(file_path):
    packages = []
    with open(file_path, "r") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                # z.B. "numpy==1.23" -> "numpy"
                pkg = line.split("==")[0].split(">=")[0].split("<=")[0]
                packages.append(pkg)
    return packages

def main():
    print("Checking Knock-Knock Python Analysis Dependencies\n")
    python_ok = check_python_version()
    print()

    if not Path(REQUIREMENTS_FILE).exists():
        print(f"Error: {REQUIREMENTS_FILE} not found!")
        sys.exit(1)

    packages = parse_requirements(REQUIREMENTS_FILE)
    missing_packages = []

    print("Checking required packages:")
    for pkg in packages:
        if not check_package(pkg):
            missing_packages.append(pkg)

    print("\n" + "="*50)

    if not python_ok or missing_packages:
        if missing_packages:
            print("Missing dependencies detected:")
            for pkg in missing_packages:
                print(f" - {pkg}")
            choice = input("\nInstall missing packages with pip? (y/n): ").strip().lower()
            if choice == "y":
                subprocess.check_call([sys.executable, "-m", "pip", "install", *missing_packages])
                print("Missing packages installed successfully.")
            else:
                print("Aborted.")
                sys.exit(1)
        else:
            print("Python version does not meet requirements.")
            sys.exit(1)
    else:
        print("All dependencies satisfied!")



if __name__ == "__main__":
    main()
