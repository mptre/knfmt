# Ensure main include detection works with relative include(s).

set -e

_wrkdir="$(mktemp -dt knfmt.XXXXXX)"
trap 'rm -r $_wrkdir' EXIT
cd "${_wrkdir}"

cat <<'EOF' >.clang-format
---
IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^"libks/'
    Priority: 1
...
EOF

mkdir libks
cat <<'EOF' >libks/arena.c
#include "libks/arena.h"

#include "libks/arithmetic.h"
EOF

${EXEC:-} "${KNFMT}" -ds libks/arena.c
