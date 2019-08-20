To debug just replace DEFINE with DEBUG_DISPLAY(_args) $display("%s", $sformatf _args); from the header (it displays all info on clock changes).
Compile seeprom.sv with verilator into Cpp class files, then run example_build_script.sh (paths need to be modified).

(for me it didn't compile with warning on wire i2c_start, i2c_stop, so you have to explicitly ignore than warning).

It generates raw 8051 assembler instructions (example binary.raw is right here, but running i2c_emulate_flagrom_driver should generate the same one) used by upload_solution.py to send 'user code' into emulator (basically it only uses READ_BYTE, WRITE_BYTE, CLOCK (0), CLOCK (1) and PRINT(BYTE)).

WARNING: Codes are messy and shouldn't be taken as code style examples.	

Flagrom.zip contents are also here for the backup purposes.