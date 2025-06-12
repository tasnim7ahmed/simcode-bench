#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LanWanLanSimulation");

int main(int argc, char *argv[])
{
    std::string csmaDataRate = "100Mbps";
    CommandLine cmd;
    cmd.AddValue("csmaRate", "CSMA data rate (default: 100Mbps)", csmaDataRate);
    cmd.Parse(argc, argv);

    // Top LAN: switches ts1-ts4, endpoints t2 and t3, router tr
    Ptr<Node> ts1 = CreateObject<Node>();
    Ptr<Node> ts2 = CreateObject<Node>();
    Ptr<Node> ts3 = CreateObject<Node>();
    Ptr<Node> ts4 = CreateObject<Node>();
    Ptr<Node> t2 = CreateObject<Node>();
    Ptr<Node> t3 = CreateObject<Node>();
    Ptr<Node> tr = CreateObject<Node>();

    // Bottom LAN: switches bs1-bs5, endpoints b2 and b3, router br
    Ptr<Node> bs1 = CreateObject<Node>();
    Ptr<Node> bs2 = CreateObject<Node>();
    Ptr<Node> bs3 = CreateObject<Node>();
    Ptr<Node> bs4 = CreateObject<Node>();
    Ptr<Node> bs5 = CreateObject<Node>();
    Ptr<Node> b2 = CreateObject<Node>();
    Ptr<Node> b3 = CreateObject<Node>();
    Ptr<Node> br = CreateObject<Node>();

    // Aggregating nodes to install InternetStack easily
    NodeContainer allNodes;
    allNodes.Add(ts1); allNodes.Add(ts2); allNodes.Add(ts3); allNodes.Add(ts4); allNodes.Add(t2); allNodes.Add(t3); allNodes.Add(tr);
    allNodes.Add(bs1); allNodes.Add(bs2); allNodes.Add(bs3); allNodes.Add(bs4); allNodes.Add(bs5); allNodes.Add(b2); allNodes.Add(b3); allNodes.Add(br);

    InternetStackHelper stack;
    stack.Install(allNodes);

    // ---- TOP LAN CONSTRUCTION ----
    // Top LAN: Use four switches (ts1-ts4), two endpoints (t2,t3), router tr
    // We'll model switches as nodes, and use CsmaNetDevice for L2 switching behavior across shared media segments

    // A: Build inter-switch mesh for top LAN and connect endpoints and router as specified
    // For t2<->b2, they must traverse multiple switches. Connect t2 to ts1, ts1 to ts2 to ts3, finally ts3 to tr.
    // For t3<->b3, via a single switch only. Connect t3 to ts4, ts4 to tr.

    // Subnets
    Ipv4AddressHelper address;
    NetDeviceContainer topLanDevices;
    NetDeviceContainer bottomLanDevices;

    // -- Top path: t2 <-> ts1 <-> ts2 <-> ts3 <-> tr

    // Connect t2 to ts1
    CsmaHelper csmaTopT2;
    csmaTopT2.SetChannelAttribute("DataRate", StringValue(csmaDataRate));
    csmaTopT2.SetChannelAttribute("Delay", StringValue("2us"));
    NodeContainer top1;
    top1.Add(t2); top1.Add(ts1);
    NetDeviceContainer ndevs_t2_ts1 = csmaTopT2.Install(top1);

    // Connect ts1 to ts2
    NodeContainer ts1_ts2;
    ts1_ts2.Add(ts1); ts1_ts2.Add(ts2);
    NetDeviceContainer ndevs_ts1_ts2 = csmaTopT2.Install(ts1_ts2);

    // Connect ts2 to ts3
    NodeContainer ts2_ts3;
    ts2_ts3.Add(ts2); ts2_ts3.Add(ts3);
    NetDeviceContainer ndevs_ts2_ts3 = csmaTopT2.Install(ts2_ts3);

    // Connect ts3 to tr
    NodeContainer ts3_tr;
    ts3_tr.Add(ts3); ts3_tr.Add(tr);
    NetDeviceContainer ndevs_ts3_tr = csmaTopT2.Install(ts3_tr);

    // Add to full top LAN device container for IP assignment later
    topLanDevices.Add(ndevs_t2_ts1.Get(0)); // t2
    topLanDevices.Add(ndevs_t2_ts1.Get(1)); // ts1
    topLanDevices.Add(ndevs_ts1_ts2.Get(0)); // ts1
    topLanDevices.Add(ndevs_ts1_ts2.Get(1)); // ts2
    topLanDevices.Add(ndevs_ts2_ts3.Get(0)); // ts2
    topLanDevices.Add(ndevs_ts2_ts3.Get(1)); // ts3
    topLanDevices.Add(ndevs_ts3_tr.Get(0)); // ts3
    topLanDevices.Add(ndevs_ts3_tr.Get(1)); // tr

    // -- Single-switch alternate LAN path: t3 <-> ts4 <-> tr

    CsmaHelper csmaTopT3;
    csmaTopT3.SetChannelAttribute("DataRate", StringValue(csmaDataRate));
    csmaTopT3.SetChannelAttribute("Delay", StringValue("2us"));
    NodeContainer t3_ts4_tr;
    t3_ts4_tr.Add(t3); t3_ts4_tr.Add(ts4); t3_ts4_tr.Add(tr);
    NetDeviceContainer ndevs_t3_ts4_tr = csmaTopT3.Install(t3_ts4_tr);

    topLanDevices.Add(ndevs_t3_ts4_tr.Get(0)); // t3
    topLanDevices.Add(ndevs_t3_ts4_tr.Get(1)); // ts4
    topLanDevices.Add(ndevs_t3_ts4_tr.Get(2)); // tr

    // ---- BOTTOM LAN CONSTRUCTION ----
    // Bottom LAN: five switches (bs1-bs5), two endpoints (b2, b3), and router br

    // For b2<->traverses multiple switches: b2 <-> bs1 <-> bs2 <-> bs3 <-> br
    CsmaHelper csmaBotB2;
    csmaBotB2.SetChannelAttribute("DataRate", StringValue(csmaDataRate));
    csmaBotB2.SetChannelAttribute("Delay", StringValue("2us"));

    NodeContainer bot1; bot1.Add(b2); bot1.Add(bs1);
    NetDeviceContainer ndevs_b2_bs1 = csmaBotB2.Install(bot1);

    NodeContainer bs1_bs2; bs1_bs2.Add(bs1); bs1_bs2.Add(bs2);
    NetDeviceContainer ndevs_bs1_bs2 = csmaBotB2.Install(bs1_bs2);

    NodeContainer bs2_bs3; bs2_bs3.Add(bs2); bs2_bs3.Add(bs3);
    NetDeviceContainer ndevs_bs2_bs3 = csmaBotB2.Install(bs2_bs3);

    NodeContainer bs3_br; bs3_br.Add(bs3); bs3_br.Add(br);
    NetDeviceContainer ndevs_bs3_br = csmaBotB2.Install(bs3_br);

    bottomLanDevices.Add(ndevs_b2_bs1.Get(0)); // b2
    bottomLanDevices.Add(ndevs_b2_bs1.Get(1)); // bs1
    bottomLanDevices.Add(ndevs_bs1_bs2.Get(0)); // bs1
    bottomLanDevices.Add(ndevs_bs1_bs2.Get(1)); // bs2
    bottomLanDevices.Add(ndevs_bs2_bs3.Get(0)); // bs2
    bottomLanDevices.Add(ndevs_bs2_bs3.Get(1)); // bs3
    bottomLanDevices.Add(ndevs_bs3_br.Get(0)); // bs3
    bottomLanDevices.Add(ndevs_bs3_br.Get(1)); // br

    // For b3<->br via a single switch: b3 <-> bs4 <-> br
    CsmaHelper csmaBotB3;
    csmaBotB3.SetChannelAttribute("DataRate", StringValue(csmaDataRate));
    csmaBotB3.SetChannelAttribute("Delay", StringValue("2us"));
    NodeContainer b3_bs4_br; b3_bs4_br.Add(b3); b3_bs4_br.Add(bs4); b3_bs4_br.Add(br);
    NetDeviceContainer ndevs_b3_bs4_br= csmaBotB3.Install(b3_bs4_br);

    bottomLanDevices.Add(ndevs_b3_bs4_br.Get(0)); // b3
    bottomLanDevices.Add(ndevs_b3_bs4_br.Get(1)); // bs4
    bottomLanDevices.Add(ndevs_b3_bs4_br.Get(2)); // br

    // Add another switch (bs5) not connected to endpoints per description, for completeness
    NodeContainer bs5_br; bs5_br.Add(bs5); bs5_br.Add(br);
    NetDeviceContainer ndevs_bs5_br = csmaBotB3.Install(bs5_br);
    bottomLanDevices.Add(ndevs_bs5_br.Get(0)); // bs5
    bottomLanDevices.Add(ndevs_bs5_br.Get(1)); // br

    // ---- WAN LINK BETWEEN ROUTERS ----
    PointToPointHelper p2pWan;
    p2pWan.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pWan.SetChannelAttribute("Delay", StringValue("50ms"));

    NodeContainer wanPair;
    wanPair.Add(tr);
    wanPair.Add(br);

    NetDeviceContainer wanDevices = p2pWan.Install(wanPair);

    // ---- IP Address Assignment ----

    // Top LAN: 192.168.1.0/24
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topLanInterfaces = address.Assign(topLanDevices);

    // Bottom LAN: 192.168.2.0/24
    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomLanInterfaces = address.Assign(bottomLanDevices);

    // WAN: 76.1.1.0/30
    address.SetBase("76.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer wanInterfaces = address.Assign(wanDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ---- APPLICATION SETUP (UDP ECHO) ----
    // Servers on b2 (for t2 to b2) and b3 (for t3 to b3)

    uint16_t port1 = 9001;
    UdpEchoServerHelper echoServer1(port1);
    ApplicationContainer serverApp1 = echoServer1.Install(b2);
    serverApp1.Start(Seconds(1.0));
    serverApp1.Stop(Seconds(18.0));

    uint16_t port2 = 9002;
    UdpEchoServerHelper echoServer2(port2);
    ApplicationContainer serverApp2 = echoServer2.Install(b3);
    serverApp2.Start(Seconds(1.0));
    serverApp2.Stop(Seconds(18.0));

    // Clients on t2 and t3 targeting b2 and b3, respectively

    // Get IP addresses: b2 is on 192.168.2.0/24 -- find first b2 device IP
    Ipv4Address b2Addr;
    for (uint32_t i=0; i < bottomLanInterfaces.GetN(); ++i) {
        if (bottomLanInterfaces.GetAddress(i) != Ipv4Address("0.0.0.0") && b2->GetObject<Ipv4>()->GetObject<Ipv4>()->GetNInterfaces() > 0) {
            if (bottomLanDevices.Get(i)->GetNode() == b2) {
                b2Addr = bottomLanInterfaces.GetAddress(i);
                break;
            }
        }
    }
    UdpEchoClientHelper echoClient1(b2Addr, port1);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(320));

    ApplicationContainer clientApp1 = echoClient1.Install(t2);
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(18.0));

    // Get IP address for b3
    Ipv4Address b3Addr;
    for (uint32_t i=0; i < bottomLanInterfaces.GetN(); ++i) {
        if (bottomLanDevices.Get(i)->GetNode() == b3) {
            b3Addr = bottomLanInterfaces.GetAddress(i);
            break;
        }
    }
    UdpEchoClientHelper echoClient2(b3Addr, port2);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(320));
    ApplicationContainer clientApp2 = echoClient2.Install(t3);
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(18.0));

    // ---- ENABLE PCAP TRACING AT KEY POINTS ----

    // WAN link (tr<->br)
    p2pWan.EnablePcapAll("lan-wan-lan-wan", false);

    // Top LAN, at tr, t2, t3
    csmaTopT2.EnablePcap("lan-wan-lan-top-t2", t2->GetId(), 0, false);
    csmaTopT2.EnablePcap("lan-wan-lan-top-tr", tr->GetId(), 0, false);
    csmaTopT3.EnablePcap("lan-wan-lan-top-t3", t3->GetId(), 0, false);

    // Bottom LAN, at br, b2, b3
    csmaBotB2.EnablePcap("lan-wan-lan-bot-b2", b2->GetId(), 0, false);
    csmaBotB2.EnablePcap("lan-wan-lan-bot-br", br->GetId(), 0, false);
    csmaBotB3.EnablePcap("lan-wan-lan-bot-b3", b3->GetId(), 0, false);

    // ---- RUN SIMULATION ----
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}