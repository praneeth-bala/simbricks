# Copyright 2022 Max Planck Institute for Software Systems, and
# National University of Singapore
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""
Simple example experiment, which sets up a client and a server host connected
through a switch.

The client pings the server.
"""

from simbricks.orchestration.experiments import Experiment
from simbricks.orchestration.nodeconfig import (
    I40eLinuxNode, HttpServer, HttpClient
)
from simbricks.orchestration.simulators import QemuHost, I40eNIC, NS3HttpNet, NS3DumbbellNet

e = Experiment(name='simple_ping')
e.checkpoint = True  # use checkpoint and restore to speed up simulation

# create client
client_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
client_config.ip = '10.0.0.5'
client_config.app = HttpClient()
client1 = QemuHost(client_config)
client1.sync = True
client1.name = 'client1'
client1.wait = True
e.add_host(client1)
client1_nic = I40eNIC()
e.add_nic(client1_nic)
client1.add_nic(client1_nic)

# client_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
# client_config.ip = '10.0.0.6'
# client_config.app = HttpClient()
# client2 = QemuHost(client_config)
# client2.sync = True
# client2.name = 'client2'
# client2.wait = True
# e.add_host(client2)
# client2_nic = I40eNIC()
# e.add_nic(client2_nic)
# client2.add_nic(client2_nic)

# client_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
# client_config.ip = '10.0.0.7'
# client_config.app = HttpClient()
# client3 = QemuHost(client_config)
# client3.sync = True
# client3.name = 'client3'
# client3.wait = True
# e.add_host(client3)
# client3_nic = I40eNIC()
# e.add_nic(client3_nic)
# client3.add_nic(client3_nic)

# create server
server_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
server_config.ip = '10.0.0.2'
server_config.app = HttpServer()
server = QemuHost(server_config)
server.sync = True
server.name = 'server'
server.wait = True
e.add_host(server)

# attach server's NIC
server_nic = I40eNIC()
e.add_nic(server_nic)
server.add_nic(server_nic)

# connect NICs over network
network = NS3DumbbellNet()
e.add_network(network)
server_nic.set_network(network)
client1_nic.set_network(network)
# client3_nic.set_network(network)
# client2_nic.set_network(network)

# set more interesting link latencies than default
eth_latency = 2 * 10**6  # 500 us
network.eth_latency = eth_latency
server_nic.eth_latency = 500
client1_nic.eth_latency = 500
# client3_nic.eth_latency = eth_latency
# client2_nic.eth_latency = eth_latency

experiments = [e]
