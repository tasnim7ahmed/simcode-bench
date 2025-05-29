#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Step 1: Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Step 2: Install point-to-point devices and channels
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Step 3: Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Step 4: Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Step 5: Set up UDP server on node 1
    uint16_t port = 9; // UDP port
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Step 6: Set up UDP client on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Step 7: Enable packet tracing
    pointToPoint.EnablePcapAll("point-to-point");

    // Step 8: NetAnim configuration
    AnimationInterface anim("point-to-point.xml");

    // Manually assign positions for better visualization
    anim.SetConstantPosition(nodes.Get(0), 50.0, 100.0);
    anim.SetConstantPosition(nodes.Get(1), 150.0, 100.0);

    anim.EnablePacketMetadata(true); // For detailed packet animations

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}