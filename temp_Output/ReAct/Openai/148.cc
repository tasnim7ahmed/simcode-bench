#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer centralNode;
    centralNode.Create(1);
    NodeContainer leafNodes;
    leafNodes.Create(3);

    // Define pairs: (0,1), (0,2), (0,3)
    NodeContainer n0n1(centralNode.Get(0), leafNodes.Get(0));
    NodeContainer n0n2(centralNode.Get(0), leafNodes.Get(1));
    NodeContainer n0n3(centralNode.Get(0), leafNodes.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d0d3 = p2p.Install(n0n3);

    InternetStackHelper internet;
    internet.Install(centralNode);
    internet.Install(leafNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i3 = ipv4.Assign(d0d3);

    uint16_t udpPort = 4000;

    // Install UDP server on node 0 for each connection
    UdpServerHelper server1(udpPort);
    UdpServerHelper server2(udpPort + 1);
    UdpServerHelper server3(udpPort + 2);

    ApplicationContainer serverApps;
    serverApps.Add(server1.Install(centralNode.Get(0)));
    serverApps.Add(server2.Install(centralNode.Get(0)));
    serverApps.Add(server3.Install(centralNode.Get(0)));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // UDP client: n1 -> n0
    UdpClientHelper client1(i0i1.GetAddress(0), udpPort);
    client1.SetAttribute("MaxPackets", UintegerValue(320));
    client1.SetAttribute("Interval", TimeValue(MilliSeconds(30))); // ~33 packets/sec
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = client1.Install(leafNodes.Get(0));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(10.0));

    // UDP client: n2 -> n0
    UdpClientHelper client2(i0i2.GetAddress(0), udpPort + 1);
    client2.SetAttribute("MaxPackets", UintegerValue(320));
    client2.SetAttribute("Interval", TimeValue(MilliSeconds(30)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp2 = client2.Install(leafNodes.Get(1));
    clientApp2.Start(Seconds(1.0));
    clientApp2.Stop(Seconds(10.0));

    // UDP client: n3 -> n0
    UdpClientHelper client3(i0i3.GetAddress(0), udpPort + 2);
    client3.SetAttribute("MaxPackets", UintegerValue(320));
    client3.SetAttribute("Interval", TimeValue(MilliSeconds(30)));
    client3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp3 = client3.Install(leafNodes.Get(2));
    clientApp3.Start(Seconds(1.0));
    clientApp3.Stop(Seconds(10.0));

    p2p.EnablePcapAll("branch-topology");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}