#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PointToPointAndLanExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create 3 routers and 2 nodes in a LAN
    NodeContainer routers;
    routers.Create(3);

    NodeContainer lanNodes;
    lanNodes.Add(routers.Get(1)); // Connect R2 to the LAN
    lanNodes.Create(2);           // Add 2 LAN nodes

    // Create point-to-point links between routers
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesR1R2, devicesR2R3;
    devicesR1R2 = pointToPoint.Install(routers.Get(0), routers.Get(1)); // R1-R2 link
    devicesR2R3 = pointToPoint.Install(routers.Get(1), routers.Get(2)); // R2-R3 link

    // Create CSMA (LAN) link
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer lanDevices;
    lanDevices = csma.Install(lanNodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(routers);
    stack.Install(lanNodes.Get(1)); // Install on LAN nodes

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR1R2 = address.Assign(devicesR1R2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR2R3 = address.Assign(devicesR2R3);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install UDP server on R3 (node 2)
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(routers.Get(2)); // R3
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install UDP client on one of the LAN nodes
    UdpClientHelper udpClient(interfacesR2R3.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(lanNodes.Get(1)); // LAN Node
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up FlowMonitor to track traffic
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
        NS_LOG_UNCOND("Tx Packets = " << i->second.txPackets);
        NS_LOG_UNCOND("Rx Packets = " << i->second.rxPackets);
        NS_LOG_UNCOND("Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps");
    }

    // Clean up and exit
    Simulator::Destroy();
    return 0;
}

