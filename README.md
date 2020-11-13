NSL Pulse
=========

Drops useful summary data concerning an active remote server. May be
utilised by an autonomous client to sweep across a cluster of servers to
pinpoint those approaching load capacities and maintain an eye on running
database processes and critical infrastructure.

Clients may thereafter be configured to dispatch alerts to admin personnel
to address these key infrastructural concerns.

Download, Compile and Usage
===========================

Run these commands to quickly deploy NSL pulse onto your server:

wget https://raw.githubusercontent.com/nullox/nslpulse/master/nslserva.c .  
gcc nslserva.c -o nslserva  
./nslserva  

Use CTRL+C to terminate the running process.

Reference
=========
1. (refer to https://www.nullox.com/docs/pulse/ for documentation)

Support
=======

Support our works through donations, we accept bitcoin, ethereum and verus coin.

BTC Donation: 3NQBVhxMrJpVeCViZNiHLouLgrCUXbL18C  
ETH Donation: 0x4C11E15Df5483Fd94Ae474311C9741041eB451ed  
VRSC Donation: RMm4wJ74eHBzuXhJ9MLsAvUQP925YmDUnp

