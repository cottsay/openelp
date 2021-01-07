Todo Items
==========

Features
--------
* More args for Windows service
* Publish to statsd
* Provide fail2ban filter
* Support pause/continue in Windows service
* Support socket activation in systemd
* Support CIDR notation for AdditionalExternalBindAddresses

Optimizations
-------------
* Re-use same slot on reconnect
* Skip unnecessary registration updates
* Stop allocating new memory after proxy\_start

Additional Settings
-------------------
* Timeout (ConnectionTimeout in config)
* Inactivity timeout
* TCP connection whitelisting
* TCP Retry Count
* Max data packet size
* Buffer sizes
* Configurable SO\_KEEPALIVE
* Configurable SO\_REUSEADDR
* Allow/reject duplicate callsigns
