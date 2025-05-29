#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Set simulation time and parameters
    double simTime = 10.0;
    uint32_t packetSize = 1024;
    uint32_t maxPackets = 10;
    double packetInterval = 1.0; // seconds between packets

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point channel
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPv4 addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP Server on Node 0
    uint16_t udpPort = 4000;
    UdpServerHelper serverHelper(udpPort);
    ApplicationContainer serverApps = serverHelper.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Install UDP Client on Node 1
    UdpClientHelper clientHelper(interfaces.GetAddress(0), udpPort);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Enable logging if needed
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}