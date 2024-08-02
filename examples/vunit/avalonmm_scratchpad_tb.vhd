-------------------------------------------------------------------------------
-- Title      : Template Testbench
-- Project    : Square Kilometre Array
-------------------------------------------------------------------------------
-- File       : avalonmm_scratchpad_tb.vhd
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

entity avalonmm_scratchpad_tb is
    generic (
        RUNNER_CFG : string
    );
end entity;

architecture sim of avalonmm_scratchpad_tb is

    constant c_DATA_WIDTH  : natural      := 32;
    constant c_BURST_WIDTH : natural      := 4;

    signal i_clk     : std_logic := '1';
    signal i_clk_rst : std_logic;

    signal address      : std_logic_vector(11 downto 0);
    signal read_enable  : std_logic;
    signal write_enable : std_logic;
    signal write_data   : std_logic_vector(c_DATA_WIDTH - 1 downto 0);
    signal byte_enable  : std_logic_vector(c_DATA_WIDTH / 8 - 1 downto 0);
    signal burst_count  : std_logic_vector(c_BURST_WIDTH - 1 downto 0);

    signal read_data       : std_logic_vector(c_DATA_WIDTH - 1 downto 0);
    signal read_data_valid : std_logic;
    signal response        : std_logic_vector(1 downto 0);
    signal wait_request    : std_logic;

    constant c_BUS_MASTER  : bus_master_t := new_bus(
        data_length    => write_data'length,
        address_length => address'length);

    constant c_MEMORY_HANDLE : memory_t       := new_memory(endian => big_endian);
    constant c_BUS_SLAVE_0   : avalon_slave_t := new_avalon_slave(
        memory                         => c_MEMORY_HANDLE,
        readdatavalid_high_probability => 0.8,
        waitrequest_high_probability   => 0.2,
        name                           => "Some Memory");

    
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
            NODE      => 0,
            TICK      => 1 us,
            VUNIT_BUS => c_BUS_MASTER -- this connects the Processor to the vunit register bus functional model.
        );

    -- The Register Bus. Could be AvalonMM, AXI, Wishbone.
    E_BUS_MASTER : entity vunit_lib.avalon_master
        generic map (
            BUS_HANDLE             => c_BUS_MASTER,
            USE_READDATAVALID      => True,
            FIXED_READ_LATENCY     => 1,
            WRITE_HIGH_PROBABILITY => 0.8,
            READ_HIGH_PROBABILITY  => 0.8
        )
        port map (
            clk           => i_clk,
            address       => address,
            byteenable    => byte_enable,
            burstcount    => burst_count,
            waitrequest   => wait_request,
            write         => write_enable,
            writedata     => write_data,
            read          => read_enable,
            readdata      => read_data,
            readdatavalid => read_data_valid
        );

    E_BUS_SLAVE : entity vunit_lib.avalon_slave
        generic map (
            AVALON_SLAVE => c_BUS_SLAVE_0
        )
        port map (
            clk           => i_clk,
            address       => address,
            byteenable    => byte_enable,
            burstcount    => burst_count,
            waitrequest   => wait_request,
            write         => write_enable,
            writedata     => write_data,
            read          => read_enable,
            readdata      => read_data,
            readdatavalid => read_data_valid
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
            num_bytes   => 2 ** address'length * 4,
            name        => "Slave Memory",
            alignment   => 4,
            permissions => read_and_write);
        wait until not i_clk_rst;
        while test_suite loop
            if run("VProc") then
                -- Put some stuff in memory to back the slave component.
                for addr in 0 to 2 ** address'length / 4 - 1 loop
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
