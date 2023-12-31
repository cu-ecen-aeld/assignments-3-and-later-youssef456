#!/bin/sh
### BEGIN INIT INFO
# Provides:          aesdsocket
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start aesdsocket application as a daemon
### END INIT INFO

DAEMON=./aesdsocket
DAEMON_ARGS="-d"  # Add any other necessary arguments here
NAME=aesdsocket
DESC="AESD Socket Application"
PIDFILE=/var/run/aesdsocket.pid

case "$1" in
  start)
    echo "Starting $DESC: $NAME"
    start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
    ;;
  stop)
    echo "Stopping $DESC: $NAME"
    start-stop-daemon -K -n aesdsocket
    ;;
  restart)
    $0 stop
    sleep 1
    $0 start
    ;;
  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
    ;;
esac

exit 0
