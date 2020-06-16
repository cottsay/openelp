Todo Items
==========

Features
--------
* Public proxy registration
* More args for Windows service
* Publish to statsd
* Provide fail2ban filter

Optimizations
-------------
* Re-use same slot on reconnect
* Stop allocating new memory after proxy\_start

Additional Settings
-------------------
* Timeout (ConnectionTimeout in config)
* Inactivity timeout
* TCP connection whitelisting
* TCP Retry Count
* Keepalive
* Max data packet size
* Buffer sizes
* Configurable SO\_REUSEADDR
* Allow/reject duplicate callsigns
