# Copyright (C) 2004-2011  See the AUTHORS file for details.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation.
#
# Binds module to process incoming messages with ZNC modtcl
#
# Supported bind types: pubm/pub, time, evnt, nick, bot, dcc, kick
#  evnt: prerehash,rehash,init-server,disconnect-server
#

namespace eval Binds {

# vars
	if {![info exists Binds::List]} {
		variable List [list]
	}

# procs
	proc Add {type flags cmd {procname ""}} {
		if {![regexp {^(pub|pubm|nick|mode|raw|bot|time|evnt|dcc|kick)$} $type]} {
			PutModule "Tcl error: Bind type: $type not supported"
			return
		}
		# ToDo: Flags check from user info (IsAdmin, etc)
		if {$procname == ""} {
			PutModule "Tcl error: Without a proc binds are useless"
			return
		}
		# ToDo: bind hit counter
		if {[lsearch $Binds::List "$type $flags [list $cmd] 0 [list $procname]"] == -1} {
			lappend Binds::List "$type $flags [list $cmd] 0 [list $procname]"
		}
		return $cmd
	}

	proc Del {type flags cmd procname} {
		if {[set match [lsearch [binds] "$type $flags $cmd 0 $procname"]] != -1 || [set match [lsearch [binds] "$type $flags {${cmd}} 0 $procname"]] != -1} {
			set Binds::List [lreplace $Binds::List $match $match]
			return $cmd
		} else {
			error "Tcl error: no such binding"
		}
	}

	proc ProcessPubm {nick user handle channel text} {
		# Loop bind list and execute
		foreach n [binds pub] {
			if {[string match [lindex $n 2] [lindex [split $text] 0]]} {
				[lindex $n 4] $nick $user $handle $channel [lrange [join $text] 1 end]
			}
		}
		foreach {type flags mask hits proc} [join [binds pubm]] {
			regsub {^%} $mask {*} mask
			if {[ModuleLoaded crypt]} {regsub {^¤} $nick {} nick}
			if {[string match -nocase $mask "$channel $text"]} {
				$proc $nick $user $handle $channel $text
			}
		}
	}

	proc ProcessTime {} {
		if {[clock format [clock seconds] -format "%S"] != 0} {return}
		set time [clock format [clock seconds] -format "%M %H %d %m %Y"]
		foreach {mi ho da mo ye} $time {}
		# Loop bind list and execute
		foreach n [binds time] {
			if {[string match [lindex $n 2] $time]} {
				[lindex $n 4] $mi $ho $da $mo $ye
			}
		}
	}

	proc ProcessEvnt {event} {
		foreach n [binds evnt] {
			if {[string match [lindex $n 2] $event]} {
				[lindex $n 4] $event
			}
		}
		switch $event {
			init-server {
				set ::botnick [GetCurNick]
				set ::server [GetServer]
				set ::server-online [expr [GetServerOnline] / 1000]
			}
			disconnect-server {
				set ::botnick ""
				set ::server ""
				set ::server-online 0
			}
		}
	}

	proc ProcessBot {from-bot cmd text} {
		foreach n [binds bot] {
			if {[string match [lindex $n 2] $cmd]} {
				[lindex $n 4] ${from-bot} $cmd $text
			}
		}
	}

	proc ProcessNick {nick user handle channel newnick} {
		if {$nick == $::botnick} {set ::botnick $newnick}
		foreach n [binds nick] {
			if {[string match [lindex $n 2] $newnick]} {
				[lindex $n 4] $nick $user $handle $channel $newnick
			}
		}
	}

	proc ProcessKick {nick user handle channel target reason} {
		foreach n [binds kick] {
			if {[string match [lindex $n 2 0] $channel] && [string match [lindex $n 2 1] $target]} {
				[lindex $n 4] $nick $user $handle $channel $target $reason
			}
		}
	}

	proc ProcessDcc {handle idx text} {
		set match 0
		foreach n [binds dcc] {
			if {[string match -nocase [lindex $n 2] [string range [lindex $text 0] 1 end]]} {
				[lindex $n 4] $handle $idx [lrange $text 1 end]
				set match 1
			}
		}
		if {!$match} {
			PutModule "Error, dcc trigger '[string range [lindex $text 0] 1 end]' doesnt exist"
		}
	}
}

# Provide aliases according to eggdrop specs
proc ::bind {type flags cmd {procname ""}} {Binds::Add $type $flags $cmd $procname}
proc ::unbind {type flags cmd procname} {Binds::Del $type $flags $cmd $procname}
proc ::binds {{type ""}} {if {$type != ""} {set type "$type "};return [lsearch -all -inline $Binds::List "$type*"]}
proc ::bindlist {{type ""}} {foreach bind $Binds::List {PutModule "$bind"}}

PutModule "modtcl script loaded: Binds v0.1"
