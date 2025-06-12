#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer starCenter;
    starCenter.Create(1); // Node 0

    NodeContainer starPeripherals;
    starPeripherals.Create(3); // Nodes 1,2,3

    NodeContainer busNodes;
    busNodes.Create(3); // Nodes 4,5,6

    // Point-to-Point links
    PointToPointHelper p2pStar;
    p2pStar.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pStar.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer starDevices[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        NodeContainer pair(starCenter.Get(0), starPeripherals.Get(i));
        starDevices[i] = p2pStar.Install(pair);
    }

    PointToPointHelper p2pBus;
    p2pBus.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    p2pBus.SetChannelAttribute("Delay", StringValue("5ms"));

    // Connect bus nodes: 4-5-6 => (4-5), (5-6)
    NetDeviceContainer busDevices[2];
    NodeContainer busLink1(busNodes.Get(0), busNodes.Get(1)); // 4,5
    NodeContainer busLink2(busNodes.Get(1), busNodes.Get(2)); // 5,6
    busDevices[0] = p2pBus.Install(busLink1);
    busDevices[1] = p2pBus.Install(busLink2);

    // Install internet stack on all nodes
    NodeContainer allNodes;
    allNodes.Add(starCenter);
    allNodes.Add(starPeripherals);
    allNodes.Add(busNodes);
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Star links
    Ipv4InterfaceContainer starInterfaces[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << (i+1) << ".0";
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        starInterfaces[i] = address.Assign(starDevices[i]);
    }

    // Bus links
    Ipv4InterfaceContainer busInterfaces[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << (i+1) << ".0";
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        busInterfaces[i] = address.Assign(busDevices[i]);
    }

    // Manually set up routing for correct delivery between bus nodes and central node
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP echo server on star center node (Node 0)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApp = echoServer.Install(starCenter.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // UDP echo clients on bus nodes 4 and 5
    // Node 4
    UdpEchoClientHelper echoClient1(starInterfaces[0].GetAddress(0), echoPort);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApp1 = echoClient1.Install(busNodes.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(19.0));

    // Node 5
    UdpEchoClientHelper echoClient2(starInterfaces[0].GetAddress(0), echoPort);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApp2 = echoClient2.Install(busNodes.Get(1));
    clientApp2.Start(Seconds(2.5));
    clientApp2.Stop(Seconds(19.5));

    // FlowMonitor setup
    Ptr<FlowMonitor> flowmon;
    FlowMonitorHelper flowmonHelper;
    flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    flowmon->SerializeToXmlFile("hybrid-star-bus-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}