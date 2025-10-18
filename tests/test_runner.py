import subprocess
import os
from typing import List

EXE_PATH = "../build/crux"
directories: list[str] = ["type_methods", "builtins", "features", "modules"]
files = []

def get_files() -> None:
	for directory in directories:
		path = f"./{directory}/"
		items = os.listdir(path)
		files.extend([f"{path}{item}" for item in items])

def run_scripts() -> List[int]:
	return_codes = []
	for file in files:
		print(f"=== RUNNING {file} ===")
		result = subprocess.run([EXE_PATH, file])
		print(f"=== END OF {file} ===\n\n")
		return_codes.append(result.returncode)
	return return_codes

if __name__ == "__main__":
	get_files()
	codes = run_scripts()
	for code in codes:
		if code != 0:
			exit(-1)
	else:
		print("All Tests Ran Successfully")