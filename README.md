# knfinit

Simple and Stupid Init system (PID 1) for multi services Docker containers.

## Features

* Launch services set in a **Docker** container.
* Restart services on unexpected shutdown (normal shutdown only if `TERM` signal received by the PID 1 process).
* Cleanly stops services using `TERM` signal on container shutdown.

## Install

Simply run `make` to get **knfinit** binary to use in your containers.

## Usage

Wrap your services in a shell script (test with **bash**) that must remains alive and accepts `TERM` signal.

Example for rsyslog:
```
#!/bin/bash

# Logger launcher

if [ -f /var/run/rsyslogd.pid ]; then
        echo -n "Removing stalled PID file ..."
        rm -f /var/run/rsyslogd.pid && echo " done" || echo " Failed !"
fi

exec rsyslogd -n
```

When using **sleep** call, make sure signals are handled by the script :
```
sleep 60 &
wait
```

In your conatainer (*Dockerfile*), pass the list of shell scripts as **knfinit** arguments.
