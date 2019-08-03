#pragma once
/*#
    # z80.h

    Header-only Z80 CPU emulator written in C.

    DON'T EDIT: This file is machine-generated from codegen/z80.template.h

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C or C++ file to create the 
    implementation.

    Optionally provide the following macros with your own implementation
    
    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    ## Emulated Pins
    ***********************************
    *           +-----------+         *
    * M1    <---|           |---> A0  *
    * MREQ  <---|           |---> A1  *
    * IORQ  <---|           |---> A2  *
    * RD    <---|           |---> ..  *
    * WR    <---|    Z80    |---> A15 *
    * HALT  <---|           |         *
    * WAIT  --->|           |<--> D0  *
    * INT   --->|           |<--> D1  *
    *           |           |<--> ... *
    *           |           |<--> D7  *
    *           +-----------+         *
    ***********************************

    ## Not Emulated
    - refresh cycles (RFSH pin)
    - interrupt mode 0
    - bus request/acknowledge (BUSRQ and BUSAK pins)
    - the RESET pin is currently not checked, call the z80_reset() 
      function instead

    ## Functions
    ~~~C
    void z80_init(z80_t* cpu, const z80_desc_t* desc)
    ~~~
        Initializes a new Z80 CPU instance. The z80_desc_t struct
        provides initialization attributes:
            ~~~C
            typedef struct {
                z80_tick_t tick_cb; // the CPU tick callback
                void* user_data;    // user data arg handed to callbacks
            } z80_desc_t;
            ~~~
        The tick_cb function will be called from inside z80_exec().

    ~~~C
    void z80_reset(z80_t* cpu)
    ~~~
        Resets the Z80 CPU instance. 

    ~~~C
    uint32_t z80_exec(z80_t* cpu, uint32_t num_ticks)
    ~~~
        Starts executing instructions until num_ticks is reached or the PC
        hits a trap, returns the number of executed ticks. The number of
        executed ticks will be greater or equal to num_ticks (unless a trap
        has been hit), because complete instructions will be executed. During
        execution the tick callback will be invoked one or multiple times
        (usually once per machine cycle, but also for 'filler ticks' or wait
        state ticks). To check if a trap has been hit, test whether 
        z80_t.trap_id != 0
        NOTE: the z80_exec() function may return in the 'middle' of an
        DD/FD extended instruction (right after the prefix byte). If this
        is the case, z80_opdone() will return false.

    ~~~C
    bool z80_opdone(z80_t* cpu)
    ~~~
        Return true if z80_exec() has returned at the end of an instruction,
        and false if the CPU is in the middle of a DD/FD prefix instruction.

    ~~~C
    void z80_set_x(z80_t* cpu, uint8_t val)
    void z80_set_xx(z80_t* cpu, uint16_t val)
    uint8_t z80_x(z80_t* cpu)
    uint16_t z80_xx(z80_t* cpu)
    ~~~
        Set and get Z80 registers and flags.

    ~~~C
    void z80_trap_cb(z80_t* cpu, z80_trap_t trap_cb)
    ~~~
        Set an optional trap callback. If this is set it will be invoked
        at the end of an instruction with the current PC (which points
        to the start of the next instruction). The trap callback should
        return a non-zero value if the execution loop should exit. The
        returned value will also be written to z80_t.trap_id.
        Set a null ptr as trap callback disables the trap checking.
        To get the current trap callback, simply access z80_t.trap_cb directly.

    ## Macros
    ~~~C
    Z80_SET_ADDR(pins, addr)
    ~~~
        set 16-bit address bus pins in 64-bit pin mask

    ~~~C
    Z80_GET_ADDR(pins)
    ~~~
        extract 16-bit address bus value from 64-bit pin mask

    ~~~C
    Z80_SET_DATA(pins, data)
    ~~~
        set 8-bit data bus pins in 64-bit pin mask

    ~~~C
    Z80_GET_DATA(pins)
    ~~~
        extract 8-bit data bus value from 64-bit pin mask

    ~~~C
    Z80_MAKE_PINS(ctrl, addr, data)
    ~~~
        build 64-bit pin mask from control-, address- and data-pins

    ~~~C
    Z80_DAISYCHAIN_BEGIN(pins)
    ~~~
        used in tick function at start of interrupt daisy-chain block

    ~~~C
    Z80_DAISYCHAIN_END(pins)
    ~~~
        used in tick function at end of interrupt daisy-chain block

    ## The Tick Callback 

    The tick function is called for one or multiple time cycles
    and connects the Z80 to the outside world. Usually one call
    of the tick function corresponds to one machine cycle, 
    but this is not always the case. The tick functions takes
    2 arguments:

    - num_ticks: the number of time cycles for this callback invocation
    - pins: a 64-bit integer with CPU pins (address- and data-bus pins,
        and control pins)

    A simplest-possible tick callback which just performs memory read/write
    operations on a 64kByte byte array looks like this:

    ~~~C
    uint8_t mem[1<<16] = { 0 };
    uint64_t tick(int num_ticks, uint64_t pins, void* user_data) {
        if (pins & Z80_MREQ) {
            if (pins & Z80_RD) {
                Z80_SET_DATA(pins, mem[Z80_GET_ADDR(pins)]);
            }
            else if (pins & Z80_WR) {
                mem[Z80_GET_ADDR(pins)] = Z80_GET_DATA(pins);
            }
        }
        else if (pins & Z80_IORQ) {
            // FIXME: perform device I/O
        }
        return pins;
    }
    ~~~

    The tick callback inspects the pins, performs the requested actions
    (memory read/write and input/output), modifies the pin bitmask
    with requests for the CPU (inject wait states, or request an
    interrupt), and finally returns the pin bitmask back to the 
    CPU emulation.

    The following pin bitmasks are relevant for the tick callback:

    - **MREQ|RD**: This is a memory read cycle, the tick callback must 
      put the byte at the memory address indicated by the address
      bus pins A0..A15 (bits 0..15) into the data bus 
      pins (D0..D7). If the M1 pin is also set, then this
      is an opcode fetch machine cycle (4 clock ticks), otherwise
      this is a normal memory read machine cycle (3 clock ticks)
    - **MREQ|WR**: This is a memory write machine cycle, the tick
      callback must write the byte in the data bus pins (D0..D7)
      to the memory location in the address bus pins (A0..A15). 
      A memory write machine cycle is 3 clock-ticks.
    - **IORQ|RD**: This is a device-input machine cycle, the 16-bit
      port number is in the address bus pins (A0..A15), and the
      tick callback must write the input-byte to the data bus
      pins (D0..D7). An input machine cycle is 4 clock-ticks.
    - **IORQ|WR**: This is a device-output machine cycle, the data
      bus pins (D0..D7) contains the byte to be output
      at the port in the address-bus pins (A0..A15). An output
      machine cycle is 4 cycles.

    Interrupt handling requires to inspect and set additional
    pins, more on that below.

    To inject wait states, execute the additional cycles in the
    CPU tick callback, and set the number of wait states
    with the Z80_SET_WAIT() macro on the returned CPU pins.
    Up to 7 wait states can be injected per machine cycle.

    Note that not all calls to the tick callback have one
    of the above pin bit patterns set. The CPU may need
    to execute filler- or processing ticks which are
    not memory-, IO- or interrupt-handling operations.

    This may happen in the following situations:
    - opcode fetch machine cycles are always a single callback
      invocation of 4 cycles with the M1|MREQ|RD pins set, however
      in a real Z80, some opcode fetch machine cycles are 5 or 6
      cycles long, in this case, the tick callback will be called
      again without control pins set and a tick count of 1 or 2
    - some instructions require additional processing ticks which
      are not memory- or IO-operations, in this case the tick
      callback may be called for with any number of ticks, but
      without activated control pins

    ## Interrupt Handling

    The interrupt 'daisy chain protocol' is entirely implemented
    in the tick callback (usually the actual interrupt daisy chain
    handling happens in the Z80 chip-family emulators, but the
    tick callback needs to invoke their interrupt handling functions).

    An interrupt request/acknowledge cycle for (most common)
    interrupt mode 2 looks like this:

    - an interrupt is requested from inside the tick callback by
      setting the INT pin in any tick callback invocation (the 
      INT pins remains active until the end of the current instruction)
    - the CPU emulator checks the INT pin at the end of the current
      instruction, if the INT pin is active, an interrupt-request/acknowledge
      machine cycle is executed which results in additional tick
      callback invocations to perform the interrupt-acknowledge protocol
    - the interrupt-controller device with pending interrupt scans the
      pin bits for M1|IORQ during the tick callback, and if active,
      places the interrupt vector low-byte on the data bus pins
    - back in the CPU emulation, the interrupt request is completed by
      constructing the complete 16-bit interrupt vector, reading the
      address of the interrupt service routine from there, and setting
      the PC register to that address (the next executed instruction
      is then the first instruction of the interrupt service routine)

    There are 2 virtual pins for the interrupt daisy chain protocol:

    - **IEIO** (Interrupt Enable In/Out): This combines the IEI and IEO 
      pins found on Z80 chip-family members and is used to disable
      interrupts for lower-priority interrupt controllers in the
      daisy chain if a higher priority device is currently negotiating
      interrupt handling with the CPU. The IEIO pin always starts
      in active state at the start of the daisy chain, and is handed
      from one interrupt controller to the next in order of 
      daisy-chain priority. The first interrupt controller with
      active interrupt handling clears the IEIO bit, which prevent
      the 'downstream' lower-priority interrupt controllers from
      issuing interrupt requests.
    - **RETI** (Return From Interrupt): The virtual RETI pin is set
      by the CPU when it decodes a RETI instruction. This is scanned
      by the interrupt controller which is currently 'under service'
      to enable interrupt handling for lower-priority devices
      in the daisy chain. In a real Z80 system this the interrupt
      controller chips perform their own simple instruction decoding
      to detect RETI instructions.

    The CPU tick callback is the heart of emulation, for complete
    tick callback examples check the emulators and tests here:
    
    http://github.com/floooh/chips-test

    http://github.com/floooh/yakc

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution. 
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*--- callback function typedefs ---*/
typedef uint64_t (*z80_tick_t)(int num_ticks, uint64_t pins, void* user_data);
typedef int (*z80_trap_t)(uint16_t pc, int ticks, uint64_t pins, void* trap_user_data);

/*--- address lines ---*/
#define Z80_A0  (1ULL<<0)
#define Z80_A1  (1ULL<<1)
#define Z80_A2  (1ULL<<2)
#define Z80_A3  (1ULL<<3)
#define Z80_A4  (1ULL<<4)
#define Z80_A5  (1ULL<<5)
#define Z80_A6  (1ULL<<6)
#define Z80_A7  (1ULL<<7)
#define Z80_A8  (1ULL<<8)
#define Z80_A9  (1ULL<<9)
#define Z80_A10 (1ULL<<10)
#define Z80_A11 (1ULL<<11)
#define Z80_A12 (1ULL<<12)
#define Z80_A13 (1ULL<<13)
#define Z80_A14 (1ULL<<14)
#define Z80_A15 (1ULL<<15)

/*--- data lines ------*/
#define Z80_D0  (1ULL<<16)
#define Z80_D1  (1ULL<<17)
#define Z80_D2  (1ULL<<18)
#define Z80_D3  (1ULL<<19)
#define Z80_D4  (1ULL<<20)
#define Z80_D5  (1ULL<<21)
#define Z80_D6  (1ULL<<22)
#define Z80_D7  (1ULL<<23)

/*--- control pins ---*/

/* system control pins */
#define  Z80_M1    (1ULL<<24)       /* machine cycle 1 */
#define  Z80_MREQ  (1ULL<<25)       /* memory request */
#define  Z80_IORQ  (1ULL<<26)       /* input/output request */
#define  Z80_RD    (1ULL<<27)       /* read */
#define  Z80_WR    (1ULL<<28)       /* write */
#define  Z80_CTRL_MASK (Z80_M1|Z80_MREQ|Z80_IORQ|Z80_RD|Z80_WR)

/* CPU control pins */
#define  Z80_HALT  (1ULL<<29)       /* halt state */
#define  Z80_INT   (1ULL<<30)       /* interrupt request */
#define  Z80_NMI   (1ULL<<31)       /* non-maskable interrupt */
#define  Z80_BUSREQ (1ULL<<32)      /* bus request */
#define  Z80_BUSACK (1ULL<<33)      /* bus acknowledge */

/* up to 7 wait states can be injected per machine cycle */
#define Z80_WAIT0   (1ULL<<34)
#define Z80_WAIT1   (1ULL<<35)
#define Z80_WAIT2   (1ULL<<36)
#define Z80_WAIT_SHIFT (34)
#define Z80_WAIT_MASK (Z80_WAIT0|Z80_WAIT1|Z80_WAIT2)

/* interrupt-related 'virtual pins', these don't exist on the Z80 */
#define Z80_IEIO    (1ULL<<37)      /* unified daisy chain 'Interrupt Enable In+Out' */
#define Z80_RETI    (1ULL<<38)      /* cpu has decoded a RETI instruction */

/* bit mask for all CPU bus pins */
#define Z80_PIN_MASK ((1ULL<<40)-1)

/*--- status indicator flags ---*/
#define Z80_CF (1<<0)           /* carry */
#define Z80_NF (1<<1)           /* add/subtract */
#define Z80_VF (1<<2)           /* parity/overflow */
#define Z80_PF Z80_VF
#define Z80_XF (1<<3)           /* undocumented bit 3 */
#define Z80_HF (1<<4)           /* half carry */
#define Z80_YF (1<<5)           /* undocumented bit 5 */
#define Z80_ZF (1<<6)           /* zero */
#define Z80_SF (1<<7)           /* sign */

#define Z80_MAX_NUM_TRAPS (4)

/* initialization attributes */
typedef struct {
    z80_tick_t tick_cb;
    void* user_data;
} z80_desc_t;

/* state bits (z80_reg_t.bits) */
#define Z80_BIT_IX      (1<<0)      /* IX prefix active */
#define Z80_BIT_IY      (1<<1)      /* IY prefix active */
#define Z80_BIT_IFF1    (1<<2)      /* interrupt-enabled bit 1 */
#define Z80_BIT_IFF2    (1<<3)      /* interrupt-enabled bit 2 */
#define Z80_BIT_EI      (1<<4)      /* enable interrupts in next instruction */

/* Z80 register bank */
typedef struct {
    uint16_t pc;
    uint8_t a, f;
    uint8_t l, h;
    uint8_t e, d;
    uint8_t c, b;
    uint8_t il, ih;     /* during decoding, contains HL, IX or IY depending on current prefix */
    uint16_t wz;        /* internal memptr register */
    uint16_t sp;
    uint16_t ix;
    uint16_t iy;
    uint8_t i, r;
    uint16_t fa_;       /* shadow registers (AF', HL', ...) */
    uint16_t hl_;
    uint16_t de_;
    uint16_t bc_;
    uint8_t im;         /* interrupt mode (0, 1, 2) */
    uint8_t bits;       /* state bits (Z80_BIT_*) */
} z80_reg_t;

/* Z80 CPU state */
typedef struct {
    z80_tick_t tick_cb;     /* tick callback */
    z80_reg_t reg;          /* register bank */
    uint64_t pins;          /* last pin state (for debug inspection) */
    void* user_data;
    z80_trap_t trap_cb;
    void* trap_user_data;
    int trap_id;
} z80_t;

/* initialize a new z80 instance */
void z80_init(z80_t* cpu, const z80_desc_t* desc);
/* reset an existing z80 instance */
void z80_reset(z80_t* cpu);
/* set optional trap callback function */
void z80_trap_cb(z80_t* cpu, z80_trap_t trap_cb, void* trap_user_data);
/* execute instructions for at least 'ticks', but at least one, return executed ticks */
uint32_t z80_exec(z80_t* cpu, uint32_t ticks);
/* return false if z80_exec() returned in the middle of an extended intruction */
bool z80_opdone(z80_t* cpu);

/* register access functions */
void z80_set_a(z80_t* cpu, uint8_t v);
void z80_set_f(z80_t* cpu, uint8_t v);
void z80_set_l(z80_t* cpu, uint8_t v);
void z80_set_h(z80_t* cpu, uint8_t v);
void z80_set_e(z80_t* cpu, uint8_t v);
void z80_set_d(z80_t* cpu, uint8_t v);
void z80_set_c(z80_t* cpu, uint8_t v);
void z80_set_b(z80_t* cpu, uint8_t v);
void z80_set_fa(z80_t* cpu, uint16_t v);
void z80_set_af(z80_t* cpi, uint16_t v);
void z80_set_hl(z80_t* cpu, uint16_t v);
void z80_set_de(z80_t* cpu, uint16_t v);
void z80_set_bc(z80_t* cpu, uint16_t v);
void z80_set_fa_(z80_t* cpu, uint16_t v);
void z80_set_af_(z80_t* cpi, uint16_t v);
void z80_set_hl_(z80_t* cpu, uint16_t v);
void z80_set_de_(z80_t* cpu, uint16_t v);
void z80_set_bc_(z80_t* cpu, uint16_t v);
void z80_set_pc(z80_t* cpu, uint16_t v);
void z80_set_wz(z80_t* cpu, uint16_t v);
void z80_set_sp(z80_t* cpu, uint16_t v);
void z80_set_i(z80_t* cpu, uint8_t v);
void z80_set_r(z80_t* cpu, uint8_t v);
void z80_set_ix(z80_t* cpu, uint16_t v);
void z80_set_iy(z80_t* cpu, uint16_t v);
void z80_set_im(z80_t* cpu, uint8_t v);
void z80_set_iff1(z80_t* cpu, bool b);
void z80_set_iff2(z80_t* cpu, bool b);
void z80_set_ei_pending(z80_t* cpu, bool b);

uint8_t z80_a(z80_t* cpu);
uint8_t z80_f(z80_t* cpu);
uint8_t z80_l(z80_t* cpu);
uint8_t z80_h(z80_t* cpu);
uint8_t z80_e(z80_t* cpu);
uint8_t z80_d(z80_t* cpu);
uint8_t z80_c(z80_t* cpu);
uint8_t z80_b(z80_t* cpu);
uint16_t z80_fa(z80_t* cpu);
uint16_t z80_af(z80_t* cpu);
uint16_t z80_hl(z80_t* cpu);
uint16_t z80_de(z80_t* cpu);
uint16_t z80_bc(z80_t* cpu);
uint16_t z80_fa_(z80_t* cpu);
uint16_t z80_af_(z80_t* cpu);
uint16_t z80_hl_(z80_t* cpu);
uint16_t z80_de_(z80_t* cpu);
uint16_t z80_bc_(z80_t* cpu);
uint16_t z80_pc(z80_t* cpu);
uint16_t z80_wz(z80_t* cpu);
uint16_t z80_sp(z80_t* cpu);
uint16_t z80_ir(z80_t* cpu);
uint8_t z80_i(z80_t* cpu);
uint8_t z80_r(z80_t* cpu);
uint16_t z80_ix(z80_t* cpu);
uint16_t z80_iy(z80_t* cpu);
uint8_t z80_im(z80_t* cpu);
bool z80_iff1(z80_t* cpu);
bool z80_iff2(z80_t* cpu);
bool z80_ei_pending(z80_t* cpu);

/* helper macro to start interrupt handling in tick callback */
#define Z80_DAISYCHAIN_BEGIN(pins) if (pins&Z80_M1) { pins|=Z80_IEIO;
/* helper macro to end interrupt handling in tick callback */
#define Z80_DAISYCHAIN_END(pins) pins&=~Z80_RETI; }
/* return a pin mask with control-pins, address and data bus */
#define Z80_MAKE_PINS(ctrl, addr, data) ((ctrl)|(((data)<<16)&0xFF0000ULL)|((addr)&0xFFFFULL))
/* extract 16-bit address bus from 64-bit pins */
#define Z80_GET_ADDR(p) ((uint16_t)(p&0xFFFFULL))
/* merge 16-bit address bus value into 64-bit pins */
#define Z80_SET_ADDR(p,a) {p=((p&~0xFFFFULL)|((a)&0xFFFFULL));}
/* extract 8-bit data bus from 64-bit pins */
#define Z80_GET_DATA(p) ((uint8_t)((p&0xFF0000ULL)>>16))
/* merge 8-bit data bus value into 64-bit pins */
#define Z80_SET_DATA(p,d) {p=((p&~0xFF0000ULL)|(((d)<<16)&0xFF0000ULL));}
/* extract number of wait states from pin mask */
#define Z80_GET_WAIT(p) ((p&Z80_WAIT_MASK)>>Z80_WAIT_SHIFT)
/* set up to 7 wait states in pin mask */
#define Z80_SET_WAIT(p,w) {p=((p&~Z80_WAIT_MASK)|((((uint64_t)w)<<Z80_WAIT_SHIFT)&Z80_WAIT_MASK));}

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

/* set 16-bit address bus pins */
#define _SA(addr) pins=(pins&~0xFFFFULL)|((addr)&0xFFFFULL)
/* set 16-bit address bus and 8-bit data bus pins */
#define _SAD(addr,data) pins=(pins&~0xFFFFFFULL)|((((data)&0xFFULL)<<16)&0xFF0000ULL)|((addr)&0xFFFFULL)
/* get 8-bit data bus value from pins */
#define _GD() ((uint8_t)((pins&0xFF0000ULL)>>16))
/* invoke 'filler tick' without control pins set */
#define _T(num) pins=tick(num,(pins&~Z80_CTRL_MASK),ud);ticks+=num
/* invoke tick callback with pins mask */
#define _TM(num,mask) pins=tick(num,(pins&~(Z80_CTRL_MASK))|(mask),ud);ticks+=num
/* invoke tick callback (with wait state detecion) */
#define _TWM(num,mask) pins=tick(num,(pins&~(Z80_WAIT_MASK|Z80_CTRL_MASK))|(mask),ud);ticks+=num+Z80_GET_WAIT(pins)
/* memory read machine cycle */
#define _MR(addr,data) _SA(addr);_TWM(3,Z80_MREQ|Z80_RD);data=_GD()
/* memory write machine cycle */
#define _MW(addr,data) _SAD(addr,data);_TWM(3,Z80_MREQ|Z80_WR)
/* input machine cycle */
#define _IN(addr,data) _SA(addr);_TWM(4,Z80_IORQ|Z80_RD);data=_GD()
/* output machine cycle */
#define _OUT(addr,data) _SAD(addr,data);_TWM(4,Z80_IORQ|Z80_WR);
/* read 8-bit immediate value */
#define _IMM8(data) _MR(c.pc++,data);
/* read 16-bit immediate value (also update WZ register) */
#define _IMM16(data) {uint8_t w,z;_MR(c.pc++,z);_MR(c.pc++,w);c.wz=(w<<8)|z;} 
/* true if current op is an indexed op */
#define _IDX() FIXME // (0!=(r2&(_BIT_USE_IX|_BIT_USE_IY)))
/* generate effective address for (HL), (IX+d), (IY+d) */
#define _ADDR(addr,ext_ticks) FIXME //{addr=_G16(ws,_HL);if(_IDX()){int8_t d;_MR(pc++,d);addr+=d;_S_WZ(addr);_T(ext_ticks);}}
/* helper macro to bump R register */
#define _BUMPR() c.r=(c.r&0x80)|((c.r+1)&0x7F);
/* a normal opcode fetch, bump R */
#define _FETCH(op) {_SA(c.pc++);_TWM(4,Z80_M1|Z80_MREQ|Z80_RD);op=_GD();_BUMPR();}
/* special opcode fetch for CB prefix, only bump R if not a DD/FD+CB 'double prefix' op */
#define _FETCH_CB(op) {_SA(c.pc++);_TWM(4,Z80_M1|Z80_MREQ|Z80_RD);op=_GD();if(!_IDX()){_BUMPR();}}
/* evaluate S+Z flags */
#define _SZ(val) ((val&0xFF)?(val&Z80_SF):Z80_ZF)
/* evaluate SZYXCH flags */
#define _SZYXCH(acc,val,res) (_SZ(res)|(res&(Z80_YF|Z80_XF))|((res>>8)&Z80_CF)|((acc^val^res)&Z80_HF))
/* evaluate flags for 8-bit adds */
#define _ADD_FLAGS(acc,val,res) (_SZYXCH(acc,val,res)|((((val^acc^0x80)&(val^res))>>5)&Z80_VF))
/* evaluate flags for 8-bit subs */
#define _SUB_FLAGS(acc,val,res) (Z80_NF|_SZYXCH(acc,val,res)|((((val^acc)&(res^acc))>>5)&Z80_VF))
/* evaluate flags for 8-bit compare */
#define _CP_FLAGS(acc,val,res) (Z80_NF|(_SZ(res)|(val&(Z80_YF|Z80_XF))|((res>>8)&Z80_CF)|((acc^val^res)&Z80_HF))|((((val^acc)&(res^acc))>>5)&Z80_VF))
/* evaluate flags for LD A,I and LD A,R */
#define _SZIFF2_FLAGS(val) ((_G_F()&Z80_CF)|_SZ(val)|(val&(Z80_YF|Z80_XF))|((r2&_BIT_IFF2)?Z80_PF:0))

/* register access functions */
void z80_set_a(z80_t* cpu, uint8_t v)         { cpu.reg.a = v; }
void z80_set_f(z80_t* cpu, uint8_t v)         { cpu.reg.f = v; }
void z80_set_l(z80_t* cpu, uint8_t v)         { cpu.reg.l = v; }
void z80_set_h(z80_t* cpu, uint8_t v)         { cpu.reg.h = v; }
void z80_set_e(z80_t* cpu, uint8_t v)         { cpu.reg.e = v; }
void z80_set_d(z80_t* cpu, uint8_t v)         { cpu->reg.d = v; }
void z80_set_c(z80_t* cpu, uint8_t v)         { cpu->reg.c = v; }
void z80_set_b(z80_t* cpu, uint8_t v)         { cpu->reg.b = v; }
void z80_set_af(z80_t* cpu, uint16_t v)       { cpu->reg.a = v; cpu->reg.f = v>>8; }
void z80_set_fa(z80_t* cpu, uint16_t v)       { cpu->reg.a = v>>8; cpu->reg.a = v; }
void z80_set_hl(z80_t* cpu, uint16_t v)       { cpu->reg.l = v; cpu->reg.h = v>>8; }
void z80_set_de(z80_t* cpu, uint16_t v)       { cpu->reg.e = v; cpu->reg.d = v>>8; }
void z80_set_bc(z80_t* cpu, uint16_t v)       { cpu->reg.c = v; cpu->reg.b = v>>8; }
void z80_set_fa_(z80_t* cpu, uint16_t v)      { cpu->reg.fa_ = v; };
void z80_set_af_(z80_t* cpu, uint16_t v)      { cpu->reg.fa_ = (v<<8)|(v>>8); }
void z80_set_hl_(z80_t* cpu, uint16_t v)      { cpu->reg.hl_ = v; }
void z80_set_de_(z80_t* cpu, uint16_t v)      { cpu->reg.de_ = v; }
void z80_set_bc_(z80_t* cpu, uint16_t v)      { cpu->reg.bc_ = v; }
void z80_set_sp(z80_t* cpu, uint16_t v)       { cpu->reg.sp = v; }
void z80_set_iy(z80_t* cpu, uint16_t v)       { cpu->reg.iy = v; }
void z80_set_ix(z80_t* cpu, uint16_t v)       { cpu->reg.ix = v; }
void z80_set_wz(z80_t* cpu, uint16_t v)       { cpu->reg.wz = v; }
void z80_set_pc(z80_t* cpu, uint16_t v)       { cpu->reg.pc = v; }
void z80_set_ir(z80_t* cpu, uint16_t v)       { cpu->reg.i = v>>8; cpu->reg.r = v; }
void z80_set_i(z80_t* cpu, uint8_t v)         { cpu->reg.i = v; }
void z80_set_r(z80_t* cpu, uint8_t v)         { cpu->reg.r = v; }
void z80_set_im(z80_t* cpu, uint8_t v)        { cpu->reg.im = v; }
void z80_set_iff1(z80_t* cpu, bool b)         { if (b) { cpu->reg.bits |= Z80_BIT_IFF1; } else { cpu->reg.bits &= ~Z80_BIT_IFF1; } }
void z80_set_iff2(z80_t* cpu, bool b)         { if (b) { cpu->reg.bits |= Z80_BIT_IFF2; } else { cpu->reg.bits &= ~Z80_BIT_IFF2; } }
void z80_set_ei_pending(z80_t* cpu, bool b)   { if (b) { cpu->reg.bits |= Z80_BIT_EI; } else { cpu->reg.bits &= ~Z80_BIT_EI; } }
uint8_t z80_a(z80_t* cpu)         { return cpu->reg.a; }
uint8_t z80_f(z80_t* cpu)         { return cpu->reg.f; }
uint8_t z80_l(z80_t* cpu)         { return cpu->reg.l; }
uint8_t z80_h(z80_t* cpu)         { return cpu->reg.h; }
uint8_t z80_e(z80_t* cpu)         { return cpu->reg.e; }
uint8_t z80_d(z80_t* cpu)         { return cpu->reg.d; }
uint8_t z80_c(z80_t* cpu)         { return cpu->reg.c; }
uint8_t z80_b(z80_t* cpu)         { return cpu->reg.b; }
uint16_t z80_fa(z80_t* cpu)       { return (cpu->reg.f<<8)|cpu->reg.a; }
uint16_t z80_af(z80_t* cpu)       { return (cpu->reg.a<<8)|cpu->reg.f; }
uint16_t z80_hl(z80_t* cpu)       { return (cpu->reg.h<<8)|cpu->reg.l; }
uint16_t z80_de(z80_t* cpu)       { return (cpu->reg.d<<8)|cpu->reg.e; }
uint16_t z80_bc(z80_t* cpu)       { return (cpu->reg.b<<8)|cpu->reg.c; }
uint16_t z80_fa_(z80_t* cpu)      { return cpu->reg.fa_; }
uint16_t z80_af_(z80_t* cpu)      { return (cpu->reg.fa_<<8)|(cpu->reg.fa_>>8); }
uint16_t z80_hl_(z80_t* cpu)      { return cpu->reg.hl_; }
uint16_t z80_de_(z80_t* cpu)      { return cpu->reg.de_; }
uint16_t z80_bc_(z80_t* cpu)      { return cpu->reg.bc_; }
uint16_t z80_sp(z80_t* cpu)       { return cpu->reg.sp_; }
uint16_t z80_iy(z80_t* cpu)       { return cpu->reg.iy; }
uint16_t z80_ix(z80_t* cpu)       { return cpu->reg.ix; }
uint16_t z80_wz(z80_t* cpu)       { return cpu->reg.wz; }
uint16_t z80_pc(z80_t* cpu)       { return cpu->reg.pc; }
uint16_t z80_ir(z80_t* cpu)       { return (cpu->reg.i<<8)|cpu->reg.r; }
uint8_t z80_i(z80_t* cpu)         { return cpu->reg.i; }
uint8_t z80_r(z80_t* cpu)         { return cpu->reg.r; }
uint8_t z80_im(z80_t* cpu)        { return cpu->reg.im; }
bool z80_iff1(z80_t* cpu)         { return 0 != (cpu->reg.bits & Z80_BIT_IFF1); }
bool z80_iff2(z80_t* cpu)         { return 0 != (cpu->reg.bits & Z80_BIT_IFF2); }
bool z80_ei_pending(z80_t* cpu)   { return 0 != (cpu->reg.bits & Z80_BIT_EI); }

void z80_init(z80_t* cpu, const z80_desc_t* desc) {
    CHIPS_ASSERT(_FA == 0);
    CHIPS_ASSERT(cpu && desc);
    CHIPS_ASSERT(desc->tick_cb);
    memset(cpu, 0, sizeof(*cpu));
    z80_reset(cpu);
    cpu->tick_cb = desc->tick_cb;
    cpu->user_data = desc->user_data;
}

void z80_reset(z80_t* cpu) {
    CHIPS_ASSERT(cpu);
    /* set AF to 0xFFFF, all other regs are undefined, set to 0xFFFF to */
    cpu->bc_de_hl_fa = cpu->bc_de_hl_fa_ = 0xFFFFFFFFFFFFFFFFULL;
    z80_set_ix(cpu, 0xFFFF);
    z80_set_iy(cpu, 0xFFFF);
    z80_set_wz(cpu, 0xFFFF);
    /* set SP to 0xFFFF, PC to 0x0000 */
    z80_set_sp(cpu, 0xFFFF);
    z80_set_pc(cpu, 0x0000);
    /* IFF1 and IFF2 are off */
    z80_set_iff1(cpu, false);
    z80_set_iff2(cpu, false);
    /* IM is set to 0 */
    z80_set_im(cpu, 0);
    /* after power-on or reset, R is set to 0 (see z80-documented.pdf) */
    z80_set_ir(cpu, 0x0000);
    cpu->im_ir_pc_bits &= ~(_BIT_EI|_BIT_USE_IX|_BIT_USE_IY);
}

void z80_trap_cb(z80_t* cpu, z80_trap_t trap_cb, void* trap_user_data) {
    CHIPS_ASSERT(cpu);
    cpu->trap_cb = trap_cb;
    cpu->trap_user_data = trap_user_data;
}

bool z80_opdone(z80_t* cpu) {
    return 0 == (cpu->im_ir_pc_bits & (_BIT_USE_IX|_BIT_USE_IY));
}

/* sign+zero+parity lookup table */
static uint8_t _z80_szp[256] = {
  0x44,0x00,0x00,0x04,0x00,0x04,0x04,0x00,0x08,0x0c,0x0c,0x08,0x0c,0x08,0x08,0x0c,
  0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,0x0c,0x08,0x08,0x0c,0x08,0x0c,0x0c,0x08,
  0x20,0x24,0x24,0x20,0x24,0x20,0x20,0x24,0x2c,0x28,0x28,0x2c,0x28,0x2c,0x2c,0x28,
  0x24,0x20,0x20,0x24,0x20,0x24,0x24,0x20,0x28,0x2c,0x2c,0x28,0x2c,0x28,0x28,0x2c,
  0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,0x0c,0x08,0x08,0x0c,0x08,0x0c,0x0c,0x08,
  0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,0x08,0x0c,0x0c,0x08,0x0c,0x08,0x08,0x0c,
  0x24,0x20,0x20,0x24,0x20,0x24,0x24,0x20,0x28,0x2c,0x2c,0x28,0x2c,0x28,0x28,0x2c,
  0x20,0x24,0x24,0x20,0x24,0x20,0x20,0x24,0x2c,0x28,0x28,0x2c,0x28,0x2c,0x2c,0x28,
  0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x8c,0x88,0x88,0x8c,0x88,0x8c,0x8c,0x88,
  0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x88,0x8c,0x8c,0x88,0x8c,0x88,0x88,0x8c,
  0xa4,0xa0,0xa0,0xa4,0xa0,0xa4,0xa4,0xa0,0xa8,0xac,0xac,0xa8,0xac,0xa8,0xa8,0xac,
  0xa0,0xa4,0xa4,0xa0,0xa4,0xa0,0xa0,0xa4,0xac,0xa8,0xa8,0xac,0xa8,0xac,0xac,0xa8,
  0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x88,0x8c,0x8c,0x88,0x8c,0x88,0x88,0x8c,
  0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x8c,0x88,0x88,0x8c,0x88,0x8c,0x8c,0x88,
  0xa0,0xa4,0xa4,0xa0,0xa4,0xa0,0xa0,0xa4,0xac,0xa8,0xa8,0xac,0xa8,0xac,0xac,0xa8,
  0xa4,0xa0,0xa0,0xa4,0xa0,0xa4,0xa4,0xa0,0xa8,0xac,0xac,0xa8,0xac,0xa8,0xa8,0xac,
};

static inline void _z80_daa(z80_reg_t* c) {
    uint8_t v = c->a;
    if (c->f & Z80_NF) {
        if (((c->a & 0xF)>0x9) || (c->f & Z80_HF)) {
            v -= 0x06;
        }
        if ((c->a > 0x99) || (c->f & Z80_CF)) {
            v -= 0x60;
        }
    }
    else {
        if (((c->a & 0xF)>0x9) || (c->f & Z80_HF)) {
            v += 0x06;
        }
        if ((c->a > 0x99) || (c->f & Z80_CF)) {
            v += 0x60;
        }
    }
    c->f &= Z80_CF|Z80_NF;
    c->f |= (c->a>0x99) ? Z80_CF : 0;
    c->f |= (c->a ^ v) & Z80_HF;
    c->f |= _z80_szp[v];
    c->a = v;
}

static inline void _z80_cpl(z80_reg_t* c) {
    c->a ^= 0xFF;
    c->f = (c->f & (Z80_SF|Z80_ZF|Z80_PF|Z80_CF)) | Z80_HF | Z80_NF | (c->a & (Z80_YF|Z80_XF));
}

static inline void _z80_scf(z80_reg_t* c) {
    c->f = (c->f & (Z80_SF|Z80_ZF|Z80_PF|Z80_CF)) | Z80_CF | (c->a & (Z80_YF|Z80_XF));
}

static inline void _z80_ccf(z80_reg_t* c) {
    c->f = ((c->f & (Z80_SF|Z80_ZF|Z80_PF|Z80_CF)) | ((c->f & Z80_CF)<<4) | (c->a & (Z80_YF|Z80_XF))) ^ Z80_CF;
}

static inline void _z80_rlca(z80_reg_t* c) {
    uint8_t r = (c->a<<1) | (c->a>>7);
    c->f = ((c->a>>7) & Z80_CF) | (c->f & (Z80_SF|Z80_ZF|Z80_PF)) | (r & (Z80_YF|Z80_XF));
    c->a = r;
}

static inline void _z80_rrca(z80_reg_t* c) {
    uint8_t r = (c->a>>1) | (c->a<<7);
    c->f = (c->a & Z80_CF) | (c->f & (Z80_SF|Z80_ZF|Z80_PF)) | (r & (Z80_YF|Z80_XF));
    c->a = r;
}

static inline void _z80_rla(z80_reg_t* c) {
    uint8_t r = (c->a<<1) | (c->f & Z80_CF);
    c->f = ((c->a>>7) & Z80_CF) | (c->f & (Z80_SF|Z80_ZF|Z80_PF)) | (r & (Z80_YF|Z80_XF));
    c->a = r;
}

static inline void _z80_rra(z80_reg_t* c) {
    uint8_t r = (c->a>>1) | ((c->f & Z80_CF)<<7);
    c->f = (c->a & Z80_CF) | (c->f & (Z80_SF|Z80_ZF|Z80_PF)) | (r & (Z80_YF|Z80_XF));
    c->a = r;
    return ws;
}

uint32_t z80_exec(z80_t* cpu, uint32_t num_ticks) {
    cpu->trap_id = 0;
    z80_reg_t c = cpu->reg;
    uint8_t map_bits = c.bits & (Z80_BITS_IX|Z80_BITS_IY);
    uint64_t pins = cpu->pins;
    uint64_t pre_pins = pins;
    const z80_tick_t tick = cpu->tick_cb;
    const z80_trap_t trap = cpu->trap_cb;
    void* ud = cpu->user_data;
    uint32_t ticks = 0;
    uint8_t op;
    do {
        _FETCH(op);
        /* ED prefix cancels DD and FD prefix */
        if (op == 0xED) {
            map_bits &= ~(Z80_BIT_IX|Z80_BIT_IY);
        }
        /* IX/IY <=> HL mapping for DD/FD prefixed ops */
        if (map_bits != (c.bits & Z80_BITS_IX|Z80_BITS_IY)) {
            if (c.bits & Z80_BITS_IX) {
                c.ix = (c.ih<<8)|c.il;
            }
            else if (c.bits & Z80_BITS_IY) {
                c.iy = (c.ih<<8)|c.il
            }
            else {
                c.l = c.il; c.h = c.ih;
            }
            if (map_bits & Z80_BITS_IX) {
                c.il = c.ix; c.ih = c.ix >> 8;
            }
            else if (map_bits & Z80_BITS_IY) {
                c.il = c.iy; c.ih = c.iy >> 8;
            }
            else {
                c.il = c.l c.ih = c.h;
            }
            c.bits = (c.bits & ~(Z80_BITS_IX|Z80_BIT_IY)) | map_bits;
        }
        /* code-generated instruction decoder */
        switch (op) {
$decode_block
            default: break;
        }
        /* interrupt handling */
        bool nmi = (pins & (pre_pins ^ pins)) & Z80_NMI;
        bool irq = ((pins & (Z80_INT|Z80_BUSREQ)) == Z80_INT) && (c.bits & Z80_BIT_IFF1);
        if (nmi || irq) {
            c.bits &= ~Z80_BIT_IFF1;
            /* a regular interrupt also clears the IFF2 flag */
            if (!nmi) {
                c.bits &= ~Z80_BIT_IFF2;
            }
            /* leave HALT state (if currently in HALT) */
            if (pins & Z80_HALT) {
                pins &= ~Z80_HALT;
                c.pc++;
            }
            /* put PC on address bus */
            _SA(pc);
            if (nmi) {
                /* non-maskable interrupt:
                    - fetch and discard the next instruction byte
                    - store PC on stack
                    - load PC and WZ with 0x0066
                */
                _TWM(5,Z80_M1|Z80_MREQ|Z80_RD); _BUMPR();
                _MW(--c->sp,c->pc>>8);
                _MW(--c->sp,c->pc);
                c->pc = c->wz = 0x0066;
            }
            else {
                /* maskable interrupt: load interrupt vector from peripheral (even if not used) */
                _TWM(4,Z80_M1|Z80_IORQ); _BUMPR();
                const uint8_t int_vec = _GD();
                _T(2);
                uint16_t addr;
                uint8_t w, z;
                switch (c->im) {
                    /* interrupt mode 0: not supported */
                    case 0:
                        break;
                    /* interrupt mode 1: put PC on stack, load PC and WZ with 0x0038 */
                    case 1:
                        _MW(--c->sp,c->pc>>8);
                        _MW(--c->sp,c->pc);
                        c->pc = = c->wz = 0x0038;
                        break;
                    /* interrupt mode 2: put PC on stack, load PC and WZ from interrupt-vector-table */
                    case 2:
                        _MW(--c->sp,c->pc>>8);
                        _MW(--c->sp,c->pc);
                        addr = (_G_I()<<8) | (int_vec & 0xFE);
                        _MR(addr++,z);
                        _MR(addr,w);
                        c->pc = c->wz = (w<<8)|z;
                        break;
                }
            }
        }
        /* if we arrive here we are at the end of a DD/FD prefixed instruction,
           clear the prefix bits for the next instruction
        */
        map_bits &= ~(Z80_USE_IX|Z80_USE_IY);

        /* invoke optional user-trap callback */
        if (trap) {
            int trap_id = trap(c.pc, ticks, pins, cpu->trap_user_data);
            if (trap_id) {
                cpu->trap_id=trap_id;
                break;
            }
        }

        /* clear the interrupt pin state */
        pins &= ~Z80_INT;

        /* delay-enable IFF flags */
        if (c.bits & Z80_BIT_EI) {
            c.bits &= ~Z80_BIT_EI;
            c.bits |= (Z80_BIT_IFF1 | Z80_BIT_IFF2);
        }
        pre_pins = pins;
    } while (ticks < num_ticks);
    c.bits = (c.bits & ~(Z80_BIT_IX|Z80_BIT_IY)) | map_bits;
    cpu->reg = c;
    cpu->pins = pins;
    return ticks;
}

#undef _SA
#undef _SAD
#undef _GD
#undef _T
#undef _TM
#undef _TWM
#undef _MR
#undef _MW
#undef _IN
#undef _OUT
#undef _IMM8
#undef _IMM16
#undef _ADDR
#undef _BUMPR
#undef _FETCH
#undef _FETCH_CB
#undef _SZ
#undef _SZYXCH
#undef _ADD_FLAGS
#undef _SUB_FLAGS
#undef _CP_FLAGS
#undef _SZIFF2_FLAGS

#endif /* CHIPS_IMPL */
