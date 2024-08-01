-- =============================================================
--
--  Copyright (c) 2024 William Kamp. All rights reserved.
--
--  Date: 1 August 2024
--
--  This file is part of the VProc package.
--
--  VProc is free software: you can redistribute it and/or modify
--  it under the terms of the GNU General Public License as published by
--  the Free Software Foundation, either version 3 of the License, or
--  (at your option) any later version.
--
--  VProc is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
--  You should have received a copy of the GNU General Public License
--  along with VProc. If not, see <http://www.gnu.org/licenses/>.
--
-- =============================================================
-- altera vhdl_input_version vhdl_2008

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

library vunit_lib;
    context vunit_lib.vunit_context;
    context vunit_lib.vc_context;
    use work.vproc_pkg.all;

entity vproc_vunit is
    generic (
        NODE      : integer range 0 to 63 := 0;
        TICK      : time                  := 1 us;
        VUNIT_BUS : bus_master_t
    );
    port (
        interrupt : in    std_logic_vector := ""
    );
end entity;

architecture model of vproc_vunit is

    constant DELTACYCLE : integer := -1;

    constant RD_QUEUE : queue_t := new_queue;
    constant WR_QUEUE : queue_t := new_queue;

begin

    P_MAIN : process
        variable VPDataOut  : integer;
        variable VPAddr     : integer;
        variable VPRW       : integer;
        variable v_RESERVED : std_logic_vector(9 downto 0);
        variable v_LBE      : std_logic_vector(3 downto 0);
        variable v_BE       : std_logic_vector( 3 downto 0);
        variable v_Burst    : std_logic_vector(11 downto 0);
        variable v_WE       : std_logic;
        variable v_RD       : std_logic;
        variable v_DataOut  : std_logic_vector(31 downto 0);
        variable v_DataIn   : std_logic_vector(31 downto 0);

        variable VPTicks    : integer;
        variable TickVal    : integer;
        variable v_BlkCount : integer;

        variable v_DataInSamp : integer;
        variable IntSamp      : integer;
        variable IntSampLast  : integer;
    begin
        TickVal      := 1;
        v_BlkCount   := 0;
        v_DataInSamp := 0;

        report "Initialising Virtual Processor";
        VInit(NODE);
        loop
            -- report "TICK!";

            -- Cleanly sample and convert inputs
            IntSampLast := 0;
            IntSamp     := to_integer('0' & unsigned(interrupt));
            VPTicks     := DELTACYCLE;

            if IntSamp > 0 then
                -- If an interrupt active, call $vsched with interrupt value
                VSched(NODE,
                    IntSamp,
                    v_DataInSamp,
                    VPDataOut,
                    VPAddr,
                    VPRW,
                    VPTicks);

                -- If interrupt routine returns non-zero tick, then override
                -- current tick value. Otherwise, leave at present value.
                if VPTicks > 0 then
                    TickVal := VPTicks;
                end if;
            end if;
            -- Call $virq when interrupt value changes, passing in new value
            if IntSamp /= IntSampLast then
                VIrq(NODE, IntSamp);
                IntSampLast := IntSamp;
            end if;
            -- Loop accessing new commands until VPTicks is not a delta cycle update
            while VPTicks < 0 loop
                -- Clear any interrupt (already dealt with)
                IntSamp := 0;

                -- Call the Host process message scheduler
                VSched(
                    NODE,
                    IntSamp,
                    v_DataInSamp,
                    VPDataOut,
                    VPAddr,
                    VPRW,
                    VPTicks
                );
                debug("Called VSched(" &
                       to_string(NODE) & ", " &
                       to_hstring(to_unsigned(IntSamp, 32)) & ", " &
                       to_hstring(to_unsigned(v_DataInSamp, 32)) & ", " &
                       to_hstring(to_unsigned(VPDataOut, 32)) & ", " &
                       to_hstring(to_unsigned(VPAddr, 32)) & ", " &
                       to_hstring(to_unsigned(VPRW, 32)) & ", " &
                       to_string(VPTicks) &
                       ")");

                -- Decode VPRW register.
                (v_RESERVED, v_LBE, v_BE, v_Burst, v_RD, v_WE) := std_logic_vector(to_unsigned(VPRW, 32));

                v_BlkCount := to_integer(unsigned(v_Burst));
                if v_WE then
                    if v_BlkCount <= 0 then
                        -- Write word, with Byte Enable.
                        -- VAccess(NODE, 0, 0, VPDataOut);
                        v_DataOut := std_logic_vector(to_signed(VPDataOut, 32));
                        write_bus(net, VUNIT_BUS, VPAddr, v_DataOut, v_BE);
                    else
                        -- burst Write.
                        assert and v_BE
                            report "Burst write doesn't support unaligned writes. byte_enable = 0x" & to_hstring(v_BE)
                            severity error;
                        assert and v_LBE
                            report "Burst write doesn't support unaligned writes. last byte_enable = 0x" & to_hstring(v_LBE)
                            severity error;
                        for beat in 0 to v_BlkCount - 1 loop
                            -- copy data from the Virtual Processor to the write queue.
                            VAccess(NODE, beat, 0, VPDataOut);
                            v_DataOut := std_logic_vector(to_signed(VPDataOut, 32));
                            push(wr_queue, v_DataOut);
                        end loop;
                        burst_write_bus(net, VUNIT_BUS, VPAddr, v_BlkCount, wr_queue);
                    end if;
                end if;
                if v_RD then
                    if v_BlkCount <= 0 then
                        read_bus(net, VUNIT_BUS, VPAddr, v_DataIn);
                        v_DataInSamp := to_integer(signed(v_DataIn));
                    else
                        -- Burst Read.
                        burst_read_bus(net, VUNIT_BUS, VPAddr, v_BlkCount, rd_queue);
                        for beat in 0 to v_BlkCount - 1 loop
                            v_DataIn     := pop(RD_QUEUE);
                            v_DataInSamp := to_integer(signed(v_DataIn));
                            VAccess(NODE, beat, v_DataInSamp, VPDataOut);
                        end loop;
                    end if;
                end if;
                -- Update current tick value with returned number (if not zero)
                if VPTicks > 0 then
                    TickVal := VPTicks - 1;
                end if;
            end loop;
            -- Count down to zero and stop
            while TickVal > 0 loop
                wait for TICK;
                TickVal := TickVal - 1;
            end loop;
        end loop;
    end process;

end architecture;

