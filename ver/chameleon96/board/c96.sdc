create_clock -name hps_user0 -period 10.000 [get_pins {u_clocks|h2f_user0_clk}]
create_generated_clock -name replay50 -source [get_pins {u_clocks|h2f_user0_clk}] -divide_by 2 [get_pins {replay_div|q}]
derive_clock_uncertainty
set_false_path -to [get_registers {*|request_meta[*]}]
set_false_path -from [get_registers {*|gp_in[*]}] -to [get_pins {u_gp|gp_in[*]}]
set_false_path -to [get_registers {reset_pipe[0]}]
