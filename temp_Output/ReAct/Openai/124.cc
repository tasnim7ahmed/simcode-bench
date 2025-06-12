#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes, hub;
    nodes.Create(3);      // End-nodes: n0, n1, n2 (different subnets)
    hub.Create(1);        // Hub node: n3 (acts as router)

    // Create separate point-to-hub CSMA segments, like VLAN trunk ports
    NodeContainer n0_hub, n1_hub, n2_hub;
    n0_hub.Add(nodes.Get(0));
    n0_hub.Add(hub.Get(0));
    n1_hub.Add(nodes.Get(1));
    n1_hub.Add(hub.Get(0));
    n2_hub.Add(nodes.Get(2));
    n2_hub.Add(hub.Get(0));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install on each separate point-to-hub link ("trunk port" representation)
    NetDeviceContainer d0 = csma.Install(n0_hub);
    NetDeviceContainer d1 = csma.Install(n1_hub);
    NetDeviceContainer d2 = csma.Install(n2_hub);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(nodes);
    internet.Install(hub);

    // Assign IP addresses to simulate VLAN/subnet isolation
    Ipv4AddressHelper addr0, addr1, addr2;
    addr0.SetBase("10.0.1.0", "255.255.255.0");
    addr1.SetBase("10.0.2.0", "255.255.255.0");
    addr2.SetBase("10.0.3.0", "255.255.255.0");

    Ipv4InterfaceContainer i0 = addr0.Assign(d0); // n0 - .1, hub - .2
    Ipv4InterfaceContainer i1 = addr1.Assign(d1); // n1 - .1, hub - .2
    Ipv4InterfaceContainer i2 = addr2.Assign(d2); // n2 - .1, hub - .2

    // Set up routing: hub node as router (enable IP forwarding)
    Ptr<Ipv4> hubIpv4 = hub.Get(0)->GetObject<Ipv4>();
    hubIpv4->SetAttribute("IpForward", BooleanValue(true));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install simple UDP Echo applications to verify communication via router/hub
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i2.GetAddress(0), 9); // n2's .1 address
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet capture (pcap tracing) on all CSMA devices
    csma.EnablePcapAll("vlan-hub-trunk-pcap", false);

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}