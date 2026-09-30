default:
    just --list

_with_tools_venv +CMD:
    . tools/.pyvenv/bin/activate && {{ CMD }}

setup-tools-venv:
    python -m venv tools/.pyvenv
    just _with_tools_venv pip --require-virtualenv install -r tools/requirements.txt

lint: setup-tools-venv
    just _with_tools_venv ruff format --diff
    just _with_tools_venv ruff check
