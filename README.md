# fortuna-fat32
A very small (&lt; 8 kB) and memory conscious (&lt; 40 bytes + a shared 512 byte buffer) C ANSI code for accessing FAT32 images. Compilable to both AVR and x64, for use in Fortuna computers and emulator.

## Supported operations

Disk operations:

|-----------|-------------|-------|--------|
| Operation | Description | Input | Output |
|-----------|-------------|-------|--------|
| `F_FREE`  | Free disk space | - | `000 - 003`: Space, in bytes |
| `F_LABEL` | Get volume label | - | `000 - 007`: Volume label |

