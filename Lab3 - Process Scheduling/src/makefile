run-simulator: scheduler.out
	@echo "Enter algorithm:";
	@echo "0) FIFO\n1) Non-Preemptive SJF\n2) Preemptive SJF\n3) Round Robin\n";
	@read -p "Choice: " c; \
	read -p "Enter path to .dat file: " f; \
	echo; \
	./scheduler.out $$c ./$$f

scheduler.out: scheduler.cpp np_sjf.cpp np_sjf_2.cpp rr.cpp rr_2.cpp fifo.cpp fifo_2.cpp algos.h
	g++ scheduler.cpp np_sjf.cpp np_sjf_2.cpp rr.cpp rr_2.cpp fifo.cpp fifo_2.cpp -o scheduler.out

clean:
	rm *.out