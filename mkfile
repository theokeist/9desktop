all:V:
	# Build the UI library first, then all command binaries.
	cd lib; mk
	cd cmd/ui-demo; mk
	cd cmd/9de-dash; mk
	cd cmd/9de-panel; mk
	cd cmd/9de-shell; mk
	cd cmd/9de-session; mk
	cd cmd/9de-control; mk

demo:V:
	# Build only the library and the UI demo.
	cd lib; mk
	cd cmd/ui-demo; mk

examples:V:
	# Examples are script/config driven; ensure demo and library are built.
	mk demo

clean:V:
	cd cmd/9de-session; mk clean
	cd cmd/9de-shell; mk clean
	cd cmd/9de-panel; mk clean
	cd cmd/9de-dash; mk clean
	cd cmd/ui-demo; mk clean
	cd cmd/9de-control; mk clean
	cd lib; mk clean
