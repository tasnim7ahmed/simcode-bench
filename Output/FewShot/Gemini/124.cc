#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5); // 4 client nodes + 1 hub

    NodeContainer clientNodes = NodeContainer(nodes.Get(0), nodes.Get(1), nodes.Get(2), nodes.Get(3));
    NodeContainer hubNode = NodeContainer(nodes.Get(4));

    // Create CSMA helper
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect clients to the hub
    NetDeviceContainer clientDevices;
    for (int i = 0; i < 4; ++i) {
        NetDeviceContainer link = csma.Install(NodeContainer(clientNodes.Get(i), hubNode.Get(0)));
        clientDevices.Add(link.Get(0));
    }
    NetDeviceContainer hubDevices = csma.GetNetDevices(hubNode.Get(0));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer clientInterfaces;
    std::string baseAddress[3] = {"10.1.1.0", "10.1.2.0", "10.1.3.0"};
    for (int i = 0; i < 3; ++i) {
        address.SetBase(baseAddress[i].c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(clientDevices.Get(i)));
        clientInterfaces.Add(interfaces.Get(0));
    }
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(clientDevices.Get(3)));
    clientInterfaces.Add(interfaces.Get(0));


    Ipv4InterfaceContainer hubInterfaces;
    address.SetBase("10.1.1.0", "255.255.255.0");
    hubInterfaces.Add(address.Assign(NetDeviceContainer(hubDevices.Get(0))).Get(0));
    address.SetBase("10.1.2.0", "255.255.255.0");
    hubInterfaces.Add(address.Assign(NetDeviceContainer(hubDevices.Get(1))).Get(0));
    address.SetBase("10.1.3.0", "255.255.255.0");
    hubInterfaces.Add(address.Assign(NetDeviceContainer(hubDevices.Get(2))).Get(0));
    address.SetBase("10.1.4.0", "255.255.255.0");
    hubInterfaces.Add(address.Assign(NetDeviceContainer(hubDevices.Get(3))).Get(0));


    // Enable forwarding on the hub
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    for (uint32_t i = 0; i < hubNode.GetN(); ++i) {
        Ptr<Node> node = hubNode.Get(i);
        Ptr<Ipv4L3Protocol> ipv4 = node->GetObject<Ipv4L3Protocol>();
        ipv4->SetForwarding(true);
        ipv4->SetDefaultTtl(64);
    }

    // Setup UDP Echo Server on node 1
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(clientNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP Echo Client on node 0
    UdpEchoClientHelper echoClient(clientInterfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(clientNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    csma.EnablePcapAll("hub_network");

    // Run simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}