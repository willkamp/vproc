-------------------------------------------------------------------------------
-- Title      : Template Testbench
-- Project    : Square Kilometre Array
-------------------------------------------------------------------------------
-- File       : wishbone_scratchpad_tb.vhd
-- Author     : William Kamp <will@kamputed.com>
-- Company    : Kamputed Limited
-- Standard   : VHDL-2008
-------------------------------------------------------------------------------
-- Copyright (c) 2024 Kamputed Limited
-------------------------------------------------------------------------------
-- Description:
--------------------------------------------------------------------------------
-- Revisions:  Revisions and documentation are controlled by
-- the revision control system (RCS).  The RCS should be consulted
-- on revision history.
-------------------------------------------------------------------------------

library vunit_lib;
    context vunit_lib.vunit_context;
    context vunit_lib.vc_context;

library ieee;
    use ieee.std_logic_1164.all;
    use ieee.numeric_std.all;

entity wishbone_scratchpad_tb is
    generic (
        RUNNER_CFG : string
    );
end entity;

architecture sim of wishbone_scratchpad_tb is

    constant c_DATA_WIDTH  : natural := 32;
    constant c_BURST_WIDTH : natural := 4;

    signal i_clk     : std_logic := '1';
    signal i_clk_rst : std_logic;

    signal clk   : std_logic;
    signal adr   : std_logic_vector(11 downto 0);
    signal dat_i : std_logic_vector(c_DATA_WIDTH - 1 downto 0);
    signal dat_o : std_logic_vector(c_DATA_WIDTH - 1 downto 0);
    signal sel   : std_logic_vector(c_DATA_WIDTH / 8 - 1 downto 0);
    signal cyc   : std_logic;
    signal stb   : std_logic;
    signal we    : std_logic;
    signal stall : std_logic;
    signal ack   : std_logic;

    constant c_BUS_MASTER : bus_master_t := new_bus(
            data_length    => dat_o'length,
            address_length => adr'length);

    constant c_MEMORY_HANDLE : memory_t         := new_memory(endian => big_endian);
    constant c_BUS_SLAVE_0   : wishbone_slave_t := new_wishbone_slave(
            memory                 => c_MEMORY_HANDLE,
            ack_high_probability   => 0.8,
            stall_high_probability => 0.2);

    function bit_swap (
        slv                  : std_logic_vector;
        constant slice_width : natural := 1   -- size of the groups to swap. e.g swap order of bytes = 8.
    )
    return std_logic_vector is
        variable out_slv : std_logic_vector(slv'range);
        variable lo, hi  : natural;
    begin
        assert slv'length mod slice_width = 0
            report "std_logic_vector input length (" & natural'image(slv'length) & ") is not a multiple of the slice_width (" & natural'image(slice_width) & ")."
            severity failure;
        for idx in 0 to slv'length / slice_width - 1 loop
            lo                                      := out_slv'low + idx * slice_width;
            hi                                      := slv'high - idx * slice_width;
            out_slv(lo + slice_width - 1 downto lo) := slv(hi downto hi - slice_width + 1);
        end loop;
        return out_slv;
    end function bit_swap;

begin

    -- Clock Generation
    i_clk     <= not i_clk after 2 ns;
    i_clk_rst <= '1', '0' after 100 ns;

    E_VIRT_PROC : entity work.vproc_vunit
        generic map (
            NODE      => 1,
            TICK      => 1 us,
            VUNIT_BUS => c_BUS_MASTER -- this connects the Processor to the vunit register bus functional model.
        );

    -- The Register Bus. Could be AvalonMM, AXI, Wishbone.
    E_BUS_MASTER : entity vunit_lib.wishbone_master
        generic map (
            BUS_HANDLE              => c_BUS_MASTER,
            STROBE_HIGH_PROBABILITY => 0.8
        )
        port map (
            clk   => i_clk,
            adr   => adr,
            dat_i => dat_i,
            dat_o => dat_o,
            sel   => sel,
            cyc   => cyc,
            stb   => stb,
            we    => we,
            stall => stall,
            ack   => ack
        );

    E_BUS_SLAVE : entity vunit_lib.wishbone_slave
        generic map (
            WISHBONE_SLAVE => c_BUS_SLAVE_0
        )
        port map (
            clk   => i_clk,
            adr   => adr,
            dat_i => dat_o,
            dat_o => dat_i,
            sel   => sel,
            cyc   => cyc,
            stb   => stb,
            we    => we,
            stall => stall,
            ack   => ack
        );

    ---------------------------------------------------------------------------
    -- Main testbench
    P_MAIN : process
        variable mem_buffer : buffer_t;
        variable data       : std_logic_vector(c_DATA_WIDTH - 1 downto 0);
    begin
        test_runner_setup(runner, runner_cfg);

        -- allocate a memory buffer to back the slave component.
        mem_buffer := allocate(
            memory => c_MEMORY_HANDLE,
            num_bytes   => 2 ** adr'length * 4,
            name        => "Slave Memory",
            alignment   => 4,
            permissions => read_and_write);
        wait until not i_clk_rst;
        while test_suite loop
            if run("VProc") then
                -- Put some stuff in memory to back the slave component.
                for addr in 0 to 2 ** adr'length / 4 - 1 loop
                    data := std_logic_vector(to_unsigned(addr, data'length));
                    write_word(
                        memory  => c_MEMORY_HANDLE,
                        address => addr * 4,
                        word    => data
                    );
                    -- We expect the virtual processor to read, bit-reverse and write back the data.
                    set_expected_word(
                        memory   => c_MEMORY_HANDLE,
                        address  => addr * 4,
                        expected => bit_swap(data)
                    );
                end loop;
                -- let the virtual processor do its thing.
                wait for 100 us;           -- FIXME : communicate that the VProc has finished somehow. Maybe it sets VTicks to some large value.
                check_expected_was_written(memory => c_MEMORY_HANDLE);
            end if;
        end loop;
        test_runner_cleanup(runner);
        wait;
    end process;

end architecture;
