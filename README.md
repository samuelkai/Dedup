# Dedup
A command line application that finds duplicate files and removes them. Duplicate files can also be replaced with symbolic links or hard links.

The application can be built using CMake. Example:
```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

The following help text for the application can be accessed with ```dedup --help```:
```
 - find and delete duplicate files
Usage:
  /home/samuel/koodi/gradu/Dedup/build/dedup [OPTION...] path1 [path2] [path3]...
By default, the user is prompted to select which duplicates to keep.
Symbolic links, extra hard links and empty files are ignored.

 Action options:
  -d, --delete     Without prompting, delete duplicate files, keeping only
                   one file in each set of duplicates. Files in paths given
                   earlier in the argument list have higher precedence to be kept.
                   If there are duplicates that were found in the same given
                   path, the file with the earliest modification time is kept.
                   Must be specified twice in order to avoid accidental use.
  -k, --hardlink   Without prompting, keep only one file in each set of
                   duplicates and replace the others with hard links to the one
                   kept. Files in paths given earlier in the argument list have
                   higher precedence to be the target of the link (whose
                   metadata is kept). If there are duplicates that were found in the
                   same given path, the file with the earliest modification
                   time is the target.
  -l, --list       List found duplicates.
  -s, --summarize  Print only a summary of found duplicates.
  -y, --symlink    Without prompting, keep only one file in each set of
                   duplicates and replace the others with symlinks to the one kept.
                   Files in paths given earlier in the argument list have
                   higher precedence to be the target of the link. If there are
                   duplicates that were found in the same given path, the file
                   with the earliest modification time is the target.

 Other options:
  -a, --hash N   Hash digest size in bytes, valid values are 1, 2, 4, 8
                 (default: 8)
  -b, --bytes N  Number of bytes from the beginning of each file that are
                 used in hash calculation. 0 means that the whole file is hashed.
                 (default: 4096)
  -h, --help     Print this help
  -n, --no-hash  In the initial comparison step, use file contents instead of
                 hash digests. Doesn't affect the result of the program.
                 Mutually exclusive with the argument 'two'. Implies the argument
                 'vector', and is mutually exclusive with it.
  -r, --recurse  Search the paths for duplicates recursively
  -t, --two      Use two layers of unordered maps to store the candidates for
                 deduplication. Doesn't affect the result of the program.
                 Mutually exclusive with the arguments 'no-hash' and 'vector'.
  -v, --vector   Use a vector instead of an unordered map to store the
                 candidates for deduplication. Doesn't affect the result of the
                 program. Mutually exclusive with the arguments 'no-hash' and
                 'two'.
```
