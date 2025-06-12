#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LanWanLanSimulation");

int main(int argc, char *argv[]) {
    std::string csmaRate = "100Mbps";
    bool enablePcap = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("csmaRate", "CSMA link data rate (e.g., 100Mbps or 10Mbps)", csmaRate);
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // Create nodes for top LAN: 4 switches + 2 end hosts
    NodeContainer ts1, ts2, ts3, ts4;
    ts1.Create(1);
    ts2.Create(1);
    ts3.Create(1);
    ts4.Create(1);

    NodeContainer t2, t3;
    t2.Create(1);
    t3.Create(1);

    // Connect top switches in a mesh: ts1 <-> ts2 <-> ts3 <-> ts4
    CsmaHelper csmaTop;
    csmaTop.SetChannelAttribute("DataRate", DataRateValue(DataRate(csmaRate)));
    csmaTop.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer d_ts1_ts2 = csmaTop.Install(NodeContainer(ts1, ts2));
    NetDeviceContainer d_ts2_ts3 = csmaTop.Install(NodeContainer(ts2, ts3));
    NetDeviceContainer d_ts3_ts4 = csmaTop.Install(NodeContainer(ts3, ts4));

    // Connect end hosts to top switches
    NetDeviceContainer d_t2_ts1 = csmaTop.Install(NodeContainer(t2, ts1));
    NetDeviceContainer d_t3_ts4 = csmaTop.Install(NodeContainer(t3, ts4));

    // Bottom LAN: 5 switches + 2 end hosts
    NodeContainer bs1, bs2, bs3, bs4, bs5;
    bs1.Create(1);
    bs2.Create(1);
    bs3.Create(1);
    bs4.Create(1);
    bs5.Create(1);

    NodeContainer b2, b3;
    b2.Create(1);
    b3.Create(1);

    // Connect bottom switches in line: bs1 <-> bs2 <-> bs3 <-> bs4 <-> bs5
    CsmaHelper csmaBottom;
    csmaBottom.SetChannelAttribute("DataRate", DataRateValue(DataRate(csmaRate)));
    csmaBottom.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer d_bs1_bs2 = csmaBottom.Install(NodeContainer(bs1, bs2));
    NetDeviceContainer d_bs2_bs3 = csmaBottom.Install(NodeContainer(bs2, bs3));
    NetDeviceContainer d_bs3_bs4 = csmaBottom.Install(NodeContainer(bs3, bs4));
    NetDeviceContainer d_bs4_bs5 = csmaBottom.Install(NodeContainer(bs4, bs5));

    // Connect end hosts to bottom switches
    NetDeviceContainer d_b2_bs1 = csmaBottom.Install(NodeContainer(b2, bs1));
    NetDeviceContainer d_b3_bs5 = csmaBottom.Install(NodeContainer(b3, bs5));

    // Create routers
    Ptr<Node> tr = CreateObject<Node>();
    Ptr<Node> br = CreateObject<Node>();

    // Connect routers via WAN point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("50ms"));

    NetDeviceContainer d_tr_br = p2p.Install(NodeContainer(tr, br));

    // Install internet stack on all nodes except switches
    InternetStackHelper stack;
    stack.Install(t2);
    stack.Install(t3);
    stack.Install(b2);
    stack.Install(b3);
    stack.Install(tr);
    stack.Install(br);

    // Bridges for switches
    for (auto switchNode : NodeContainer(ts1, ts2, ts3, ts4)) {
        stack.Install(switchNode);
    }
    for (auto switchNode : NodeContainer(bs1, bs2, bs3, bs4, bs5)) {
        stack.Install(switchNode);
    }

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Top LAN subnet
    address.SetBase("192.168.1.0", "255.255.255.0");
    address.Assign(d_t2_ts1);
    address.Assign(d_ts1_ts2);
    address.Assign(d_ts2_ts3);
    address.Assign(d_ts3_ts4);
    address.Assign(d_t3_ts4);

    // Bottom LAN subnet
    address.SetBase("192.168.2.0", "255.255.255.0");
    address.Assign(d_b2_bs1);
    address.Assign(d_bs1_bs2);
    address.Assign(d_bs2_bs3);
    address.Assign(d_bs3_bs4);
    address.Assign(d_bs4_bs5);
    address.Assign(d_b3_bs5);

    // WAN subnet
    address.SetBase("76.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_tr_br = address.Assign(d_tr_br);

    // Bridge interfaces on switches
    // Top switches
    for (auto switchNode : NodeContainer(ts1, ts2, ts3, ts4)) {
        Ptr<Ipv4> ipv4 = switchNode->GetObject<Ipv4>();
        ipv4->SetForwarding("Ipv4GlobalRouting", true);
    }

    // Bottom switches
    for (auto switchNode : NodeContainer(bs1, bs2, bs3, bs4, bs5)) {
        Ptr<Ipv4> ipv4 = switchNode->GetObject<Ipv4>();
        ipv4->SetForwarding("Ipv4GlobalRouting", true);
    }

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Server on b2 and b3
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps_b2 = echoServer.Install(b2);
    serverApps_b2.Start(Seconds(1.0));
    serverApps_b2.Stop(Seconds(10.0));

    ApplicationContainer serverApps_b3 = echoServer.Install(b3);
    serverApps_b3.Start(Seconds(1.0));
    serverApps_b3.Stop(Seconds(10.0));

    // Clients on t2 and t3
    UdpEchoClientHelper echoClient_b2(b2->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    echoClient_b2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient_b2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient_b2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps_b2 = echoClient_b2.Install(t2);
    clientApps_b2.Start(Seconds(2.0));
    clientApps_b2.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient_b3(b3->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    echoClient_b3.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient_b3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient_b3.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps_b3 = echoClient_b3.Install(t3);
    clientApps_b3.Start(Seconds(2.0));
    clientApps_b3.Stop(Seconds(10.0));

    // Enable PCAPs at key points if enabled
    if (enablePcap) {
        csmaTop.EnablePcapAll("lan_wan_lan_top");
        csmaBottom.EnablePcapAll("lan_wan_lan_bottom");
        p2p.EnablePcapAll("lan_wan_lan_wan");
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}