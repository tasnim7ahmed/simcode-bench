#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HubAndTrunkSimulation");

int main(int argc, char *argv[]) {
    bool tracing = true;

    // Create nodes
    NodeContainer hub;
    hub.Create(1);

    NodeContainer vlanNodes;
    vlanNodes.Create(3);

    NodeContainer allNodes = NodeContainer(hub, vlanNodes);

    // Create CSMA channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer hubDevices;
    NetDeviceContainer vlanDevices[3];

    // Connect each VLAN node to the hub via CSMA
    for (uint32_t i = 0; i < 3; ++i) {
        NetDeviceContainer devices = csma.Install(NodeContainer(hub.Get(0), vlanNodes.Get(i)));
        hubDevices.Add(devices.Get(0));
        vlanDevices[i].Add(devices.Get(1));
    }

    // Install Internet stacks
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer interfaces[4]; // 3 VLANs + hub interfaces

    // VLAN 1: 192.168.1.0/24
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    interfaces[0] = ipv4.Assign(vlanDevices[0]);

    // VLAN 2: 192.168.2.0/24
    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    interfaces[1] = ipv4.Assign(vlanDevices[1]);

    // VLAN 3: 192.168.3.0/24
    ipv4.SetBase("192.168.3.0", "255.255.255.0");
    interfaces[2] = ipv4.Assign(vlanDevices[2]);

    // Assign IP addresses to hub's interfaces
    ipv4.SetBase("192.168.1.1", "255.255.255.0");
    interfaces[3].Add(hub.Get(0)->GetObject<Ipv4>()->Assign(hubDevices));

    // Manually set up routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up applications
    uint16_t port = 9; // Discard port

    // Ping from node 0 to node 1 and node 2
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(vlanNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[1].GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(vlanNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    if (tracing) {
        csma.EnablePcapAll("hub_trunk_simulation", false);
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}