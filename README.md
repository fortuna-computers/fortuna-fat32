# fortuna-fat32

[![Automated tests](https://github.com/fortuna-computers/fortuna-fat32/actions/workflows/automated-tests.yml/badge.svg?branch=master)](https://github.com/fortuna-computers/fortuna-fat32/actions/workflows/automated-tests.yml)
[![Code size](https://github.com/fortuna-computers/fortuna-fat32/actions/workflows/code-size.yml/badge.svg?branch=master)](https://github.com/fortuna-computers/fortuna-fat32/actions/workflows/code-size.yml)

A very small (&lt; 8 kB) and memory conscious (&lt; 40 bytes + a shared 512 byte buffer) C ANSI code for accessing FAT32 images. Compilable to both AVR and x64, for use in Fortuna computers and emulator.

### Special registers

* `F_RSLT`: result of the last operation (all operations set 
* `F_SZ`: buffer size (used when reading/writing files)
* `F_FLN`: file number (used when reading/writing files)

### Current limitations

* Will only work in the first partition of a single disk.

## Supported operations

All operations return 0 on success or various errors.

Disk operations:

| Operation | Description | Input | Output |
|-----------|-------------|-------|--------|
| `F_FREE`  | Free disk space (from FSInfo) | - | `000 - 003`: Space, in clusters |
| `F_BOOT` | Load boot sector | - | The 512-byte boot sector |
| `F_FSINFO_RECALC`  | Recalculate values in FSINFO | - | - |

Directory operations:

*(directory could be a path relative to current dir, or an absolute path starting with '/')*

| Operation | Description | Input | Output |
|-----------|-------------|-------|--------|
| `F_DIR`   | List contents of current directory | `0`: start over; `1`: continue | Directory listing ([same structure as FAT32](https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#Directory_entry))
| `F_CD`    | Change directory | Directory path | - |
| `F_MKDIR` | Create a directory | Directory path | - |
| `F_RMDIR` | Remove a directory | Directory path | - |

File editing operations:

| Operation | Description | Input | Output | `F_FLN` | `F_SZ` |
|-----------|-------------|-------|--------|---------|--------|
| `F_OPEN` | Open an existing file | File name | - | Sets file number | - |
| `F_CREATE` | Create a file | File name | - | Set file number |
| `F_SEEK`  | Seek a sector (relative forward) | Buffer: 32-bit sector, or `0xFFFFFFFF` for end | - | File number | Set size of sector |
| `F_READ` | Read block and seek next sector | - | Buffer read | File number | Set size of sector
| `F_OVERWRITE` | Rewrite an existing block | Buffer to write | - | File number | Size of sector |
| `F_APPEND` | Append a new block at the end of file | Buffer to write | - | File number | Size of sector |
| `F_CLOSE` | Close file | - | - | File number| - |

File/directory operations:

| Operation | Description | Input | Output |
|-----------|-------------|-------|--------|
| `F_STAT` | Read file/dir information | File/dir name | File in directory listing ([same structure as FAT32](https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#Directory_entry) |
| `F_MV` | Rename/move directory | Old file/directory name, new file/directory name | - |
| `F_RM` | Remove file | File/Directory name | - |

## Structures

`F_OPEN` request values (TODO)

Return values:

| Value | Meaning |
|-------|---------|
| `0`   | Success - end of operation |
| `1`   | Success - more data available |
| `>1`  | Failure |
