#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedP2PCSMAExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer p2pNodes0;
    p2pNodes0.Create(2);

    NodeContainer p2pNodes1;
    p2pNodes1.Add(p2pNodes0.Get(1)); // n2
    p2pNodes1.Create(1);             // n6

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices0;
    p2pDevices0 = pointToPoint.Install(p2pNodes0);

    NetDeviceContainer p2pDevices1;
    p2pDevices1 = pointToPoint.Install(p2pNodes1);

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes0.Get(1)); // n2
    csmaNodes.Create(3);             // n3, n4, n5

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    NodeContainer allNodes;
    allNodes.Add(p2pNodes0.Get(0)); // n0
    allNodes.Add(p2pNodes0.Get(1)); // n2
    allNodes.Add(p2pNodes1.Get(1)); // n6
    allNodes.Add(csmaNodes.Get(0)); // already added n2
    allNodes.Add(csmaNodes.Get(1)); // n3
    allNodes.Add(csmaNodes.Get(2)); // n4
    allNodes.Add(csmaNodes.Get(3)); // n5

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces0;
    p2pInterfaces0 = address.Assign(p2pDevices0);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces1;
    p2pInterfaces1 = address.Assign(p2pDevices1);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign(csmaDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint32_t packetSize = 50;
    Time interPacketInterval = Seconds(0.1); // 50 bytes at 300 bps -> 0.1 seconds per packet (8*50 / 300 = 0.00133... sec?)

    UdpClientHelper udpClient(csmaInterfaces.GetAddress(3), 9);
    udpClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    clientApps.Add(udpClient.Install(p2pNodes0.Get(0))); // n0 as sender

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    UdpServerHelper udpServer(9);
    ApplicationContainer serverApp = udpServer.Install(csmaNodes.Get(3)); // n5
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed-p2p-csma.tr");
    pointToPoint.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);

    pointToPoint.EnablePcapAll("mixed-p2p-csma");
    csma.EnablePcapAll("mixed-p2p-csma");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}