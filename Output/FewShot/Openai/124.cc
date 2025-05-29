#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable packet capture tracing
    CsmaHelper::EnablePcapAll("hub-trunk");

    // Create nodes: 3 end hosts and 1 central hub/router
    NodeContainer hosts;
    hosts.Create(3);
    Ptr<Node> hub = CreateObject<Node>();

    // Each host connects via its own CSMA segment to the hub
    NodeContainer pair1;
    NodeContainer pair2;
    NodeContainer pair3;
    pair1.Add(hosts.Get(0));
    pair1.Add(hub);
    pair2.Add(hosts.Get(1));
    pair2.Add(hub);
    pair3.Add(hosts.Get(2));
    pair3.Add(hub);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devA = csma.Install(pair1);
    NetDeviceContainer devB = csma.Install(pair2);
    NetDeviceContainer devC = csma.Install(pair3);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(hosts);
    stack.Install(hub);

    // Assign different subnets to each host-hub connection (simulating VLANs)
    Ipv4AddressHelper addressA;
    addressA.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifA = addressA.Assign(devA);

    Ipv4AddressHelper addressB;
    addressB.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifB = addressB.Assign(devB);

    Ipv4AddressHelper addressC;
    addressC.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifC = addressC.Assign(devC);

    // Set up static routing on the hub for inter-subnet routing
    Ptr<Ipv4> hubIpv4 = hub->GetObject<Ipv4>();
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> hubStaticRouting = ipv4RoutingHelper.GetStaticRouting(hubIpv4);

    // Each host adds default route via the hub
    for (uint32_t i = 0; i < 3; ++i)
    {
        Ptr<Node> host = hosts.Get(i);
        Ptr<Ipv4> hostIpv4 = host->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> hostStaticRouting = ipv4RoutingHelper.GetStaticRouting(hostIpv4);

        if (i == 0)
            hostStaticRouting->SetDefaultRoute(ifA.GetAddress(1), 1);
        else if (i == 1)
            hostStaticRouting->SetDefaultRoute(ifB.GetAddress(1), 1);
        else
            hostStaticRouting->SetDefaultRoute(ifC.GetAddress(1), 1);
    }

    // Hub sets subnets as directly connected (done automatically, but can populate for clarity)
    // No additional static routes required on the hub (each interface is directly connected subnet)

    // Application setup: Ping between hosts on different VLANs
    // E.g., Host 0 pings Host 1 and Host 2

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);

    // Install echo server on Host 1 and Host 2
    ApplicationContainer serverApps1 = echoServer.Install(hosts.Get(1));
    ApplicationContainer serverApps2 = echoServer.Install(hosts.Get(2));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(10.0));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient1(ifB.GetAddress(0), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApp1 = echoClient1.Install(hosts.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(9.0));

    UdpEchoClientHelper echoClient2(ifC.GetAddress(0), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApp2 = echoClient2.Install(hosts.Get(0));
    clientApp2.Start(Seconds(3.0));
    clientApp2.Stop(Seconds(9.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}