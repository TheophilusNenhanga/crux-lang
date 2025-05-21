import subprocess
import os

EXE_PATH = "../build/crux.exe"
directories: list[str] = ["type_methods", "builtins", "features"]
files = []

for directory in directories:
	path = f"./{directory}/"
	items = os.listdir(path)
	files.extend([f"{path}{item}" for item in items])

for file in files:
	result = subprocess.run([EXE_PATH, file])
	print(result)
