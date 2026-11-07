# One module through the conformance gate. Usage: gate.sh <ekoc> <file>,
# run from the repository root. A module with test bindings runs as its
# own root; a library module rides the check pipeline, whose exit status
# is deliberately ignored — only its diagnostics speak. Any rejection
# exits nonzero, which xargs turns into a failed gate.

out=$("$1" test "$2" 2>&1); rc=$?

if echo "$out" | grep -q 'E-lower'; then
    echo "gate $2: a checked program was rejected in lower"
    echo "$out" | grep 'E-lower' | head -3
    exit 1
fi

if echo "$out" | grep -q 'declares no test bindings'; then
    out=$("$1" check "$2" 2>&1)
    if echo "$out" | grep -q 'E-lower'; then
        echo "gate $2: a checked program was rejected in lower"
        echo "$out" | grep 'E-lower' | head -3
        exit 1
    fi
    if echo "$out" | grep ' E-' | grep -qv 'E-entry-point'; then
        echo "fail $2"
        echo "$out" | grep ' E-' | grep -v 'E-entry-point' | head -3
        exit 1
    fi
    exit 0
fi

if [ "$rc" -ne 0 ]; then
    echo "fail $2"
    echo "$out" | tail -5
    exit 1
fi
