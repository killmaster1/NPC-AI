#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
npcai_java_tools="${NPC_AI_JAVA_TOOLS:-build/java-tools}"
mkdir -p "$npcai_java_tools/classes" "$npcai_java_tools/dex"
for npcai_item in ecj android r8; do
  case "$npcai_item" in
    ecj) npcai_url=https://repo.maven.apache.org/maven2/org/eclipse/jdt/ecj/3.37.0/ecj-3.37.0.jar;;
    android) npcai_url=https://repo.maven.apache.org/maven2/com/google/android/android/4.1.1.4/android-4.1.1.4.jar;;
    r8) npcai_url=https://storage.googleapis.com/r8-releases/raw/8.3.37/r8.jar;;
  esac
  if [[ ! -f "$npcai_java_tools/$npcai_item.jar" ]]; then curl -fsSL "$npcai_url" -o "$npcai_java_tools/$npcai_item.jar"; fi
 done
mkdir -p "$npcai_java_tools/classes/br/com/npcai"
find "$npcai_java_tools/classes/br/com/npcai" -maxdepth 1 -type f -name '*.class' -delete
java -jar "$npcai_java_tools/ecj.jar" -1.8 -proc:none -classpath "$npcai_java_tools/android.jar" -d "$npcai_java_tools/classes" android/br/com/npcai/*.java
java -cp "$npcai_java_tools/r8.jar" com.android.tools.r8.D8 --min-api 26 --lib "$npcai_java_tools/android.jar" --output "$npcai_java_tools/dex" "$npcai_java_tools"/classes/br/com/npcai/*.class
python3 - "$npcai_java_tools/dex/classes.dex" <<'PY'
from pathlib import Path
import sys
b=Path(sys.argv[1]).read_bytes()
Path('native/generated/editor_dex.h').write_text('// Generated from android/br/com/npcai/*.java; D8 8.3.37, API26\nstatic const unsigned char editorDex[] = {\n'+','.join(str(x) for x in b)+'\n};\n')
PY
