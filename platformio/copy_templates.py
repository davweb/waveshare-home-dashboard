"""Copy LVGL template files to use as a dependency"""

import os.path
import shutil
import sys
from SCons.Script import DefaultEnvironment

ENV = DefaultEnvironment()
DEPS_ENV_DIR = os.path.join(ENV.get("PROJECT_LIBDEPS_DIR"), ENV.get("PIOENV"))
SOURCE_DIR = os.path.join(DEPS_ENV_DIR, "ESP32_Display_Panel", "template_files")
TARGET_DIR = os.path.join(DEPS_ENV_DIR, "LVGL_Template_Files")
FILES = ["lvgl_v8_port.cpp", "lvgl_v8_port.h"]

if not os.path.exists(TARGET_DIR):
    os.makedirs(TARGET_DIR)

for file_name in FILES:
    source_file = os.path.join(SOURCE_DIR, file_name)
    target_file = os.path.join(TARGET_DIR, file_name)

    if os.path.exists(source_file):
        print(f"Copying {file_name}")
        try:
            shutil.copy(source_file, target_file)
        except Exception as e:
            print(f"Error copying {file_name}: {e}")
            sys.exit(1)
    else:
        print(f"Error: Source file {source_file} not found.")
        sys.exit(1)
