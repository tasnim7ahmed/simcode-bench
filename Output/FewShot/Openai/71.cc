#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging and tracing
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: A, B, C
    Ptr<Node> nA = CreateObject<Node>();
    Ptr<Node> nB = CreateObject<Node>();
    Ptr<Node> nC = CreateObject<Node>();

    NodeContainer nodes;
    nodes.Add(nA);
    nodes.Add(nB);
    nodes.Add(nC);

    // Point-to-point link between A and B
    NodeContainer ab(nA, nB);
    PointToPointHelper p2pAB;
    p2pAB.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pAB.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d_ab = p2pAB.Install(ab);

    // Point-to-point link between B and C
    NodeContainer bc(nB, nC);
    PointToPointHelper p2pBC;
    p2pBC.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBC.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d_bc = p2pBC.Install(bc);

    // CsmaNetDevices for A and C (/32 host-only)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("1us"));

    NetDeviceContainer d_a_csma;
    Ptr<NetDevice> aCsmaDev = csma.Install(NodeContainer(nA)).Get(0);
    d_a_csma.Add(aCsmaDev);

    NetDeviceContainer d_c_csma;
    Ptr<NetDevice> cCsmaDev = csma.Install(NodeContainer(nC)).Get(0);
    d_c_csma.Add(cCsmaDev);

    // Install Internet stack with global routing
    InternetStackHelper internet;
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    internet.InstallAll();

    // Assign addresses for point-to-point links
    Ipv4AddressHelper addr_ab;
    addr_ab.SetBase("10.0.0.0", "255.255.255.252"); // /30
    Ipv4InterfaceContainer if_ab = addr_ab.Assign(d_ab);

    Ipv4AddressHelper addr_bc;
    addr_bc.SetBase("10.0.0.4", "255.255.255.252"); // /30
    Ipv4InterfaceContainer if_bc = addr_bc.Assign(d_bc);

    // Assign /32 IP addresses to Csma devices
    Ipv4AddressHelper addr_a_csma;
    addr_a_csma.SetBase("172.16.1.1", "255.255.255.255");
    Ipv4InterfaceContainer if_a_csma = addr_a_csma.Assign(d_a_csma);

    Ipv4AddressHelper addr_c_csma;
    addr_c_csma.SetBase("192.168.1.1", "255.255.255.255");
    Ipv4InterfaceContainer if_c_csma = addr_c_csma.Assign(d_c_csma);

    // Assign interfaces to correct node interface indexes
    Ptr<Ipv4> ipv4A = nA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nB->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nC->GetObject<Ipv4>();

    // Add host routes to A and C for /32 hosts via their p2p addresses
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4StaticRouting> staticRoutingA = staticRoutingHelper.GetStaticRouting(ipv4A);
    Ptr<Ipv4StaticRouting> staticRoutingC = staticRoutingHelper.GetStaticRouting(ipv4C);

    // A: route to C's /32 via B
    staticRoutingA->AddHostRouteTo(
        Ipv4Address("192.168.1.1"),
        if_ab.GetAddress(1),
        1
    );
    // C: route to A's /32 via B
    staticRoutingC->AddHostRouteTo(
        Ipv4Address("172.16.1.1"),
        if_bc.GetAddress(0),
        1
    );

    // Applications: OnOff (UDP traffic) from A (172.16.1.1) to C (192.168.1.1)
    uint16_t port = 5000;
    OnOffHelper onoff ("ns3::UdpSocketFactory",
        Address (InetSocketAddress (Ipv4Address("192.168.1.1"), port)));
    onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("500kbps")));
    onoff.SetAttribute ("PacketSize", UintegerValue (1024));
    onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
    onoff.SetAttribute ("StopTime", TimeValue (Seconds (9.0)));
    onoff.SetAttribute ("Remote", AddressValue(InetSocketAddress(Ipv4Address("192.168.1.1"), port)));
    onoff.SetAttribute ("MaxBytes", UintegerValue (0));
    ApplicationContainer cA = onoff.Install(nA);

    // Packet Sink on C (listening on 192.168.1.1:5000)
    PacketSinkHelper sink("ns3::UdpSocketFactory",
        Address(InetSocketAddress(Ipv4Address("192.168.1.1"), port)));
    ApplicationContainer cC = sink.Install(nC);
    cC.Start(Seconds(0.5));
    cC.Stop(Seconds(10.0));

    // Enable tracing and pcap
    p2pAB.EnablePcapAll("ab-link");
    p2pBC.EnablePcapAll("bc-link");
    csma.EnablePcap("a-csma", aCsmaDev, true, true);
    csma.EnablePcap("c-csma", cCsmaDev, true, true);

    AsciiTraceHelper ascii;
    p2pAB.EnableAsciiAll(ascii.CreateFileStream("ab-link.tr"));
    p2pBC.EnableAsciiAll(ascii.CreateFileStream("bc-link.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}