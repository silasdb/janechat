#!/bin/sh
#
# Possible commands:
#
# janechat-attachment-handler.sh exists mimetype filename
# janechat-attachment-handler.sh save mimetype filename
# janechat-attachment-handler.sh open mimetype filename

set -eu

mimetype2extension () {
	case "$1" in
	application/pdf) echo pdf ;;
	audio/ogg|audio/ogg\;*) echo ogg ;;
	image/jpeg) echo jpeg ;;
	image/png) echo png ;;
	video/mp4) echo mp4 ;;
	audio/mp4) echo mp4 ;;
	audio/mpeg) echo mpg ;;
	*) return 1 ;;
	esac
}

cmd="$1"
mimetype="$2"
filename="$3"

tmpdir=/tmp
extension="$(mimetype2extension "$mimetype")"
filepath="$tmpdir/$filename.$extension"

case "$cmd" in
exists)
	if [ -f "$filepath" ]; then
		echo yes
	else
		echo no
	fi
	;;
save)
	cat > "$filepath"
	;;
open)
	# Prefer run-mailcap or xdg-open
	if run-mailcap "$filepath"; then exit; fi
	if xdg-open "$filepath"; then exit; fi

        # If run-mailcap and xdg-open fails (or if it is not found), try
        # alternatives.
	case "$mimetype" in
	image/png|image/jpeg)
		feh -. "$filepath" &
		;;
	application/pdf)
		evince "$filepath" &
		;;
	video/mp4)
		mpv "$filepath" &
		;;
	audio/ogg|audio/ogg\;*|audio/mp4|audio/mpeg)
		${TERM} -e "mpv $filepath" &
		;;
	esac
	;;
*)
	exit 1
	;;
esac
