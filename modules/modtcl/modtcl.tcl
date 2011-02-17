# Copyright (C) 2004-2011  See the AUTHORS file for details.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation.
#
# TCL master file for ZNC modtcl
#
#  Usage: LoadModule = modtcl /path/to/this/file
#

# rehash simply reloads this file and shows info for any errors
set ::MasterFile [info script]
proc rehash {} {
	if {[catch {source $::MasterFile}]} {
		PutModule "Error rehashing tcl master file: $::errorInfo"
	}
}
proc bgerror {message} {
	PutModule "Background error: $::errorInfo"
}

# set basic eggdrop style variables
set ::botnet-nick ZNC_[GetUsername]
set ::botnick [GetCurNick]
set ::server [GetServer]
set ::server-online [expr [GetServerOnline] / 1000]

# add some eggdrop style procs
proc putlog message {PutModule $message}
proc puthelp {text {option ""}} {
	if {[regexp -nocase {^(?:privmsg|notice) (\S+) :(.*)} $text . target line]} {
		if {$target == "*modtcl"} {PutModule $line; return}
		if {$target == "*status"} {PutStatus $line; return}
		if {[botonchan $target]} {PutUser ":$::botnick![getchanhost $::botnick] $text"}
	}
	PutIRC $text
}
proc putserv {text {option ""}} {puthelp $text}
proc putquick {text {option ""}} {puthelp $text}
proc stripcodes {flags args} {return [regsub -all {\xF|\x2|\x16|\x1F|\x7|\x3([0-9]{1,2})?(,[0-9]{1,2})?} [join $args] {}]}
proc unixtime {} {return [clock seconds]}
proc duration {s} {
	set ret ""
	foreach {n m} "year 31536000 week 604800 day 86400 hour 3600 minute 60 second 1" {
		if {$s >= $m} {
			set tmp [expr $s / $m]
			if {$tmp == 1} {set i ""} else {set i "s"}
			set ret $ret[format "%lu %s%s " $tmp $n $i]
			incr s -[expr $tmp * $m]
		}
	}
	return $ret
}
proc encrypt {key string} {return [bencrypt $key $string]}
proc decrypt {key string} {return [bdecrypt $key $string]}
proc channels {} {return [GetChans]}
proc chanlist {channel {flags ""}} {
	set ret ""
	foreach u [GetChannelUsers [string trim $channel "{}"]] {lappend ret [lindex $u 0]}
	return $ret
}
proc getchanmode channel {return [GetChannelModes [string trim $channel "{}"]]}
proc getchanhost {nick {channel ""}} {
	# TODO: to have full info here we need to use /who #chan when we join
	if {$channel == ""} {set channel [join [channels]]}
	foreach c $channel {
		foreach u [GetChannelUsers $c] {
			if {[string match $nick [lindex $u 0]]} {
				return "[lindex $u 1]@[lindex $u 2]"
			}
		}
	}
	return ""
}
proc onchan {nick chan} {return [expr [lsearch -exact [string tolower [chanlist $chan]] [string tolower $nick]] != -1]}
proc botonchan channel {return [onchan $::botnick $channel]}

proc isop {nick {channel ""}} {return [PermCheck $nick "@" $channel]}
proc isvoice {nick {channel ""}} {return [PermCheck $nick "+" $channel]}
proc botisop {{channel ""}} {return [isop $::botnick $channel]}
proc botisvoice {{channel ""}} {return [isvoice $::botnick $channel]}

proc PermCheck {nick perm channel} {
	if {$channel == ""} {set channel [channels]}
	if {[ModuleLoaded crypt]} {regsub {^¤} $nick {} nick}
	foreach c $channel {
		foreach u [GetChannelUsers $c] {
			if {[string match -nocase $nick [lindex $u 0]] && [string match *$perm* [lindex $u 3]]} {
				return 1
			}
		}
	}
	return 0
}
proc ModuleLoaded modname {
	foreach {mod args glob} [join [GetModules]] {
		if {[string match -nocase $modname $mod]} {return 1}
	}
	return 0
}

# rewrite all timers to use the after command
proc utimer {seconds tcl-command} {after [expr $seconds * 1000] ${tcl-command}}
proc timer {minutes tcl-command} {after [expr $minutes * 60 * 1000] ${tcl-command}}
proc utimers {} {set t {}; foreach a [after info] {lappend t "0 [lindex [after info $a] 0] $a"}; return $t}
proc timers {} {set t {}; foreach a [after info] {lappend t "0 [lindex [after info $a] 0] $a"}; return $t}
proc killtimer id {return [after cancel $id]}
proc killutimer id {return [after cancel $id]}

# pseudo procs to satisfy script loading, no functionality
proc setudef {type name} {return}
proc modules {} {return "encryption"}
proc channel {command {options ""}} {return ""}
proc queuesize {{queue ""}} {return 0}
proc matchattr {handle flags {channel ""}} {return 0}

# load other script files
source [file dirname $::MasterFile]/binds.tcl


PutModule "Succesfully loaded modtcl with master file: [info script]"
