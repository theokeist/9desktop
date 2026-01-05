all:V:
	cd lib; mk
	cd cmd/ui-demo; mk
	cd cmd/9de-dash; mk
	cd cmd/9de-panel; mk
	cd cmd/9de-shell; mk
	cd cmd/9de-session; mk
	cd cmd/9de-control; mk

clean:V:
	cd cmd/9de-session; mk clean
	cd cmd/9de-shell; mk clean
	cd cmd/9de-panel; mk clean
	cd cmd/9de-dash; mk clean
	cd cmd/ui-demo; mk clean
	cd cmd/9de-control; mk clean
	cd lib; mk clean
