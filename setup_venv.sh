#!/bin/bash

set -e  # Exit on any error
echo "Setting up Python virtual environment..."



# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is not installed or not in PATH"
    echo "Please install Python 3.7+ before running this script"
    exit 1
fi



# Check Python version
PYTHON_VERSION=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
REQUIRED_VERSION="3.7"

if [ "$(printf '%s\n' "$REQUIRED_VERSION" "$PYTHON_VERSION" | sort -V | head -n1)" != "$REQUIRED_VERSION" ]; then
    echo "Error: Python $PYTHON_VERSION detected, but Python $REQUIRED_VERSION+ is required"
    exit 1
fi

echo "Python $PYTHON_VERSION detected"



# Create virtual environment
if [ -d ".venv" ]; then
    echo "Virtual environment .venv already exists."
    read -p "Do you want to recreate it? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Removing existing virtual environment..."
        rm -rf .venv
    else 
        echo "Using existing virtual environment"
        echo "To activate the virtual environment: source .venv/bin/activate"
        exit 0
    fi
fi

echo "Creating virtual environment..."
python3 -m venv .venv



# Update and install requirements
echo "Upgrading pip..."
.venv/bin/pip install --upgrade pip

echo "Install requirements from requirements.txt..."
.venv/bin/pip install -r requirements.txt


echo "Setup complete!"
echo "To activate the virtual environment: source .venv/bin/activate"
