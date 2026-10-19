package require ::quartus::project
foreach name {JTFRAME C96_JT51_SRC C96_JT51_BUILD} {
    if {![info exists ::env($name)]} { error "Missing environment $name; run board/build.sh" }
}
set src $::env(C96_JT51_SRC)
set out $::env(C96_JT51_BUILD)
project_new c96_jt51 -overwrite
set_global_assignment -name FAMILY "Cyclone V"
set_global_assignment -name DEVICE 5CSEBA6U19C8
set_global_assignment -name TOP_LEVEL_ENTITY c96_jt51_top
set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files
set_global_assignment -name SEARCH_PATH $out
set fp [open [file join $out hdl-files.txt] r]
foreach path [split [read $fp] "\n"] {
    if {$path ne ""} { set_global_assignment -name VERILOG_FILE $path }
}
close $fp
set_global_assignment -name VERILOG_FILE [file join $out build.vh]
set_global_assignment -name SDC_FILE [file join $src board c96.sdc]
set_location_assignment PIN_E5 -to {pcm_pins[0]}
set_location_assignment PIN_F5 -to {pcm_pins[1]}
set_location_assignment PIN_A6 -to {pcm_pins[2]}
set_location_assignment PIN_A5 -to {pcm_pins[3]}
set_instance_assignment -name IO_STANDARD "1.8 V" -to {pcm_pins[*]}
set_instance_assignment -name WEAK_PULL_UP_RESISTOR OFF -to {pcm_pins[*]}
set_global_assignment -name RESERVE_ALL_UNUSED_PINS "AS INPUT TRI-STATED"
set_instance_assignment -name GLOBAL_SIGNAL "GLOBAL CLOCK" -to replay_div
set_global_assignment -name NUM_PARALLEL_PROCESSORS 4
set_global_assignment -name MIN_CORE_JUNCTION_TEMP 0
set_global_assignment -name MAX_CORE_JUNCTION_TEMP 85
set_global_assignment -name STRATIXV_CONFIGURATION_SCHEME "PASSIVE PARALLEL X16"
set_global_assignment -name ENABLE_CONFIGURATION_PINS OFF
set_global_assignment -name CRC_ERROR_OPEN_DRAIN OFF
set_global_assignment -name ENABLE_OCT_DONE OFF
export_assignments
project_close
