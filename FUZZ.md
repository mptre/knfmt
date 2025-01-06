# Fuzzing

## American Fuzz Lop (AFL)

1. Configure and compile the binary.

```
./configure --pedantic
make clean
make AFL_USE_ASAN=1 AFL_USE_UBSAN=1 CC=afl-cc
```

1. Copy the binary to a safe place.

```
cp knfmt /tmp
```

1. Create directories.

```sh
mkdir -p fuzz/{i,o}
```

1. It's often of interest to exercise the latest source code changes by using
   the latest test cases as the corpus.

```sh
git log --name-status tests/*.[ch] |
sed -n 's/^A[[:space:]][[:space:]]*//p' |
head -15 |
xargs -I {} cp {} fuzz/i
```

1. Start one or many instances of afl-fuzz.

```
afl-fuzz -i fuzz/i -o fuzz/o -M f0 /tmp/knfmt -s
afl-fuzz -i fuzz/i -o fuzz/o -S f1 /tmp/knfmt -s
```
