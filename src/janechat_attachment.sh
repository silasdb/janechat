#!/bin/sh
set -eu

cmd="$1"
filepath="$2"
mimetype="$3"

if [ "$1" = 'exists' ]; then
	# TODO
	exit
fi

# "$1" is open

case "$mimetype" in
image/png|image/jpeg)
	feh "$filepath" &
	;;
application/pdf)
	evince "$filepath" &
	;;
video/mp4)
	mplayer "$filepath" &
	;;
'audio/ogg; codecs=opus')
	xterm -c "mplayer $filepath" &
	;;
esac
