initial begin
`ifdef DUMPSIGNALS
	`ifdef NCVERILOG
		$shm_open("jt51_test.shm");
		$shm_probe(jt51_test,"AS");
	`else
		$dumpfile("jt51_test.lxt");
		$dumpvars();
		$dumpon;
	`endif
	//#(280*10000) $finish;
`endif
end
