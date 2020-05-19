# mamtool
Medium Auxiliary Memory access tool

Primarily targetting NetBSD, but should work with minimal modifications on other BSDs and Linux.

```
$ sudo ./mamtool
Opening device /dev/enrst0

Device identifies itself as : SCSI   busnum = 0, target = 3, lun = 0

REMAINING CAPACITY IN PARTITION (binary, 8 bytes, read-only): 792305
MAXIMUM CAPACITY IN PARTITION (binary, 8 bytes, read-only): 800226
TAPEALERT FLAGS (binary, 8 bytes, read-only): 0
LOAD COUNT (binary, 8 bytes, read-only): 7
MAM SPACE REMAINING (binary, 8 bytes, read-only): 1014
ASSIGNING ORGANIZATION (ascii, 8 bytes, read-only): LTO-CVE 
FORMATTED DENSITY CODE (binary, 1 bytes, read-only): 70
INITIALIZATION COUNT (binary, 2 bytes, read-only): 1
VOLUME IDENTIFIER (ascii, 0 bytes, read-only): 
DEVICE VENDOR/SERIAL NUMBER AT LAST LOAD (ascii, 40 bytes, read-only): HP      (...)
DEVICE VENDOR/SERIAL NUMBER AT LOAD-1 (ascii, 40 bytes, read-only): HP      (...)
DEVICE VENDOR/SERIAL NUMBER AT LOAD-2 (ascii, 40 bytes, read-only): HP      (...)
DEVICE VENDOR/SERIAL NUMBER AT LOAD-3 (ascii, 40 bytes, read-only): HP      (...)
TOTAL MBYTES WRITTEN IN MEDIUM LIFE (binary, 8 bytes, read-only): 2091706
TOTAL MBYTES READ IN MEDIUM LIFE (binary, 8 bytes, read-only): 0
TOTAL MBYTES WRITTEN IN CURRENT/LAST LOAD (binary, 8 bytes, read-only): 0
TOTAL MBYTES READ IN CURRENT/LAST LOAD (binary, 8 bytes, read-only): 0
LOGICAL POSITION OF FIRST ENCRYPTED BLOCK (binary, 8 bytes, read-only): 281474976710655
MEDIUM MANUFACTURER (ascii, 8 bytes, read-only): FUJIFILM
MEDIUM SERIAL NUMBER (ascii, 32 bytes, read-only): (...)
MEDIUM LENGTH (binary, 4 bytes, read-only): 820
MEDIUM WIDTH (binary, 4 bytes, read-only): 127
ASSIGNING ORGANIZATION (ascii, 8 bytes, read-only): LTO-CVE 
MEDIUM DENSITY CODE (binary, 1 bytes, read-only): 70
MEDIUM MANUFACTURE DATE (ascii, 8 bytes, read-only): 20110727
MAM CAPACITY (binary, 8 bytes, read-only): 4096
MEDIUM TYPE (binary, 1 bytes, read-only): 0
MEDIUM TYPE INFORMATION (binary, 2 bytes, read-only): 0
UNIQUE CARTRIDGE IDENTITY (binary, 28 bytes, read-only): (...)
ALTERNATIVE UNIQUE CARTRIDGE IDENTITY (binary, 24 bytes, read-only): (...)
```

