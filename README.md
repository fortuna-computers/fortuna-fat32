# fortuna-fat32
A very small (&lt; 8 kB) and memory conscious (&lt; 40 bytes + a shared 512 byte buffer) C ANSI code for accessing FAT32 images. Compilable to both AVR and x64, for use in Fortuna computers and emulator.

## Special registers

* `F_RSLT`: result of the last operation

## Supported operations

All operations return 0 on success or various errors.

Disk operations:

| Operation | Description | Input | Output |
|-----------|-------------|-------|--------|
| `F_LABEL` | Get volume label | - | `000 - 010`: Volume label |
| `F_FREE`  | Free disk space (from FSInfo) | - | `000 - 003`: Space, in bytes |
| `F_FREE_R`  | Free disk space (recalculate from FAT) | - | `000 - 003`: Space, in bytes |

Directory operations (all operations are relative to the current directory):

| Operation | Description | Input | Output |
|-----------|-------------|-------|--------|
| `F_DIR`   | List contents of current directory | `0`: start over; `1`: continue | Directory listing (see below)
| `F_CD`    | Change directory | Directory name | - |
| `F_MKDIR` | Create a directory | Directory name | - |
| `F_RMDIR` | Remove a directory | Directory name | - |

File operations:

| Operation | Description | Input | Output |
|-----------|-------------|-------|--------|
| `F_OPEN` | Open or create a file | File name + attributes (see below) | File number |
| `F_CLOSE` | Close file | - | File number |
| `F_READ` | Read block | File number, block number | Number of bytes left |
| `F_WRITE` | Write block | File number, block number | Number of bytes to write |

Operations that work both in files and directories:

| Operation | Description | Input | Output |
|-----------|-------------|-------|--------|
| `F_STAT` | Read file/dir information | File/dir name | Stat structure |
| `F_RM` | Remove file/directory | File/Directory name | - |
| `F_MV` | Rename/move directory | Old file/directory name, new file/directory name | - |

## Structures

`F_DIR` result structure:

* same structure as directory entry in FAT32

`F_OPEN` request values (TODO)

Stat structure (TODO)

Return values

| Value | Meaning |
| `0`   | Success - end of operation |
| `1`   | Success - more data available |
| `>1`  | Failure |
