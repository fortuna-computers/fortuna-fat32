# fortuna-fat32
A very small (&lt; 8 kB) and memory conscious (&lt; 40 bytes + a shared 512 byte buffer) C ANSI code for accessing FAT32 images. Compilable to both AVR and x64, for use in Fortuna computers and emulator.

## Supported operations

All operations return 0 on success or various errors.

Disk operations:

| Operation | Description | Input | Output |
|-----------|-------------|-------|--------|
| `F_FREE`  | Free disk space | - | `000 - 003`: Space, in bytes |
| `F_LABEL` | Get volume label | - | `000 - 007`: Volume label |

Directory operations (all operations are relative to the current directory):

| Operation | Description | Input | Output |
|-----------|-------------|-------|--------|
| `F_CD`    | Change directory | Directory name | - |
| `F_DIR`   | List contents of directory | - | Directory listing (see below)
| `F_MKDIR` | Create a directory | Directory name | - |

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

## Formats

`F_DIR` return value (TODO)

`F_OPEN` request values (TODO)

Stat structure (TODO)

Return values

| Value | Meaning |
| `0`   | Success - end of operation |
| `1`   | Success - more data available |
| `>1`  | Failure |
