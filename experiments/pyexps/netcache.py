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
    I40eLinuxNode, NetCacheServer, NetCacheClient, PegasusServer, PegasusClient
)
from simbricks.orchestration.simulators import QemuHost, I40eNIC, NS3BridgeNet, NS3Pegasus, NS3NetCache

e = Experiment(name='netcache')
e.checkpoint = False  # use checkpoint and restore to speed up simulation

# create client
client_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
client_config.ip = '10.0.0.5'
client_config.app = NetCacheClient()
client_config.app.node_id = 0
client0 = QemuHost(client_config)
client0.sync = True
client0.name = 'client0'
client0.wait = True
e.add_host(client0)
client0_nic = I40eNIC()
e.add_nic(client0_nic)
client0.add_nic(client0_nic)

client_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
client_config.ip = '10.0.0.6'
client_config.app = NetCacheClient()
client_config.app.node_id = 1
client1 = QemuHost(client_config)
client1.sync = True
client1.name = 'client1'
client1.wait = True
e.add_host(client1)
client1_nic = I40eNIC()
e.add_nic(client1_nic)
client1.add_nic(client1_nic)

client_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
client_config.ip = '10.0.0.7'
client_config.app = NetCacheClient()
client_config.app.node_id = 2
client2 = QemuHost(client_config)
client2.sync = True
client2.name = 'client2'
client2.wait = True
e.add_host(client2)
client2_nic = I40eNIC()
e.add_nic(client2_nic)
client2.add_nic(client2_nic)

# create server
server_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
server_config.ip = '10.0.0.3'
server_config.app = NetCacheServer()
server_config.app.node_id = 0
server0 = QemuHost(server_config)
server0.sync = True
server0.name = 'server0'
server0.wait = True
e.add_host(server0)
server0_nic = I40eNIC()
e.add_nic(server0_nic)
server0.add_nic(server0_nic)

server_config = I40eLinuxNode()  # boot Linux with i40e NIC driver
server_config.ip = '10.0.0.4'
server_config.app = NetCacheServer()
server_config.app.node_id = 1
server1 = QemuHost(server_config)
server1.sync = True
server1.name = 'server1'
server1.wait = True
e.add_host(server1)
server1_nic = I40eNIC()
e.add_nic(server1_nic)
server1.add_nic(server1_nic)

# connect NICs over network
network = NS3NetCache()
e.add_network(network)
server0_nic.set_network(network)
server1_nic.set_network(network)
client0_nic.set_network(network)
client1_nic.set_network(network)
client2_nic.set_network(network)

# set more interesting link latencies than default
eth_latency = 2 * 10**3  # 500 us
network.eth_latency = eth_latency
server0_nic.eth_latency = eth_latency
server1_nic.eth_latency = eth_latency
client0_nic.eth_latency = eth_latency
client1_nic.eth_latency = eth_latency
client2_nic.eth_latency = eth_latency

experiments = [e]
