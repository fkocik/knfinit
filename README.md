# KNF Init System

[![Build Status](https://travis-ci.org/fkocik/knfinit.svg?branch=master)](https://travis-ci.org/fkocik/knfinit)

Simple and Stupid Init system (PID 1) for multi services **Docker** containers.

## Features

* Launch services set in a **Docker** container.
* Restart services on unexpected shutdown (normal shutdown only if `TERM` signal received by the PID 1 process).
* Cleanly stops services using `TERM` signal on container shutdown.

## Install

Simply run `make` to get **knfinit** binary to use in your containers.

## Usage

Wrap your services in a shell script (tested with **bash**) that must remains alive and accepts `TERM` signal.

In your conatainer (*Dockerfile*), pass the list of shell scripts as **knfinit** arguments.

### Basic service usage

In the shell, execute your favorite program in foreground like you should do it when using your shell as **Docker** `PID 1` process.

Example for **RSyslog**:
```
#!/bin/bash

# Logger launcher

if [ -f /var/run/rsyslogd.pid ]; then
        echo -n "Removing stalled PID file ..."
        rm -f /var/run/rsyslogd.pid && echo " done" || echo " Failed !"
fi

exec rsyslogd -n
```

### Monitoring service usage

With some software, you cannot execute *daemon* in foreground.
So, you'll need to write a sort of monitoring loop. In this cas, make sure signals are handled by the script.

Example for **Postfix**:
```
#!/bin/bash

# Mail Transfert Agent startup

postfix start

STATUS=1

trap "echo 'Now Stopping Postfix ...'; postfix stop; STATUS=0" INT TERM QUIT

while postfix status; do
        sleep 300 &
        wait
done

echo "Postfix down."

exit $STATUS
```

### Task scheduling

Given the fact **knfinit** is restarting services, you can schedule maintenance tasks for your container
using **sleep** call. This is not a real time schedule but may be sufficient for regular indenpotent batch needs :
```
#!/bin/bash

# Batch sample running hourly

sleep 1h &
wait
find /tmp -iname 'myapp-*.tmp' -type f -delete
```

Another sample that run at startup then daily:
```
#!/bin/bash

# Batch sample running at startup and then hourly

find /tmp -iname 'myapp-*.tmp' -type f -delete
sleep 1h &
wait
```

