#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LanWanLanGlobalRouterExample");

int main (int argc, char *argv[])
{
    std::string csmaRate = "100Mbps";
    CommandLine cmd;
    cmd.AddValue ("csmaRate", "CSMA channel data rate (e.g., 100Mbps or 10Mbps)", csmaRate);
    cmd.Parse (argc, argv);

    // --- NODES ---
    // Top LAN endpoints
    NodeContainer t2, t3;
    t2.Create (1);
    t3.Create (1);

    // Top switches
    NodeContainer ts1, ts2, ts3, ts4;
    ts1.Create (1);
    ts2.Create (1);
    ts3.Create (1);
    ts4.Create (1);

    // Top router
    Ptr<Node> tr = CreateObject<Node> ();

    // Bottom LAN endpoints
    NodeContainer b2, b3;
    b2.Create (1);
    b3.Create (1);

    // Bottom switches
    NodeContainer bs1, bs2, bs3, bs4, bs5;
    bs1.Create (1);
    bs2.Create (1);
    bs3.Create (1);
    bs4.Create (1);
    bs5.Create (1);

    // Bottom router
    Ptr<Node> br = CreateObject<Node> ();

    // --- TOPOLOGY CONNECTIONS ---

    // Top LAN connections (multiple switches, bridging)
    // t2 <-> ts1 <-> ts2 <-> ts3 <-> ts4 <-> tr

    NodeContainer t2_ts1;
    t2_ts1.Add (t2.Get(0)); t2_ts1.Add (ts1.Get(0));
    NodeContainer ts1_ts2;
    ts1_ts2.Add (ts1.Get(0)); ts1_ts2.Add (ts2.Get(0));
    NodeContainer ts2_ts3;
    ts2_ts3.Add (ts2.Get(0)); ts2_ts3.Add (ts3.Get(0));
    NodeContainer ts3_ts4;
    ts3_ts4.Add (ts3.Get(0)); ts3_ts4.Add (ts4.Get(0));
    NodeContainer ts4_tr;
    ts4_tr.Add (ts4.Get(0)); ts4_tr.Add (tr);

    // t3 <-> ts2 (only one switch to router)
    NodeContainer t3_ts2;
    t3_ts2.Add (t3.Get(0)); t3_ts2.Add (ts2.Get(0));

    // Bottom LAN connections (bridged path and single switch path)
    // b2 <-> bs1 <-> bs2 <-> bs3 <-> bs4 <-> bs5 <-> br

    NodeContainer b2_bs1;
    b2_bs1.Add (b2.Get(0)); b2_bs1.Add (bs1.Get(0));
    NodeContainer bs1_bs2;
    bs1_bs2.Add (bs1.Get(0)); bs1_bs2.Add (bs2.Get(0));
    NodeContainer bs2_bs3;
    bs2_bs3.Add (bs2.Get(0)); bs2_bs3.Add (bs3.Get(0));
    NodeContainer bs3_bs4;
    bs3_bs4.Add (bs3.Get(0)); bs3_bs4.Add (bs4.Get(0));
    NodeContainer bs4_bs5;
    bs4_bs5.Add (bs4.Get(0)); bs4_bs5.Add (bs5.Get(0));
    NodeContainer bs5_br;
    bs5_br.Add (bs5.Get(0)); bs5_br.Add (br);

    // b3 <-> bs3
    NodeContainer b3_bs3;
    b3_bs3.Add (b3.Get(0)); b3_bs3.Add (bs3.Get(0));

    // WAN connection
    NodeContainer tr_br;
    tr_br.Add (tr); tr_br.Add (br);

    // --- DEVICES AND CHANNELS ---

    // CSMA helper
    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue (csmaRate));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

    // Devices: For each CSMA link, install devices
    NetDeviceContainer dev_t2_ts1 = csma.Install (t2_ts1);
    NetDeviceContainer dev_ts1_ts2 = csma.Install (ts1_ts2);
    NetDeviceContainer dev_ts2_ts3 = csma.Install (ts2_ts3);
    NetDeviceContainer dev_ts3_ts4 = csma.Install (ts3_ts4);
    NetDeviceContainer dev_ts4_tr = csma.Install (ts4_tr);
    NetDeviceContainer dev_t3_ts2 = csma.Install (t3_ts2);

    NetDeviceContainer dev_b2_bs1 = csma.Install (b2_bs1);
    NetDeviceContainer dev_bs1_bs2 = csma.Install (bs1_bs2);
    NetDeviceContainer dev_bs2_bs3 = csma.Install (bs2_bs3);
    NetDeviceContainer dev_bs3_bs4 = csma.Install (bs3_bs4);
    NetDeviceContainer dev_bs4_bs5 = csma.Install (bs4_bs5);
    NetDeviceContainer dev_bs5_br = csma.Install (bs5_br);
    NetDeviceContainer dev_b3_bs3 = csma.Install (b3_bs3);

    // Point-to-Point WAN link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("50ms"));
    NetDeviceContainer dev_tr_br = p2p.Install (tr_br);

    // --- BRIDGING (SWITCHES) ---

    // Top switches bridge their CSMA devices
    BridgeHelper bridge;
    // ts1 bridges: t2, ts2
    NetDeviceContainer ts1Ports;
    ts1Ports.Add (dev_t2_ts1.Get (1)); // port for t2
    ts1Ports.Add (dev_ts1_ts2.Get (0)); // port towards ts2
    bridge.Install (ts1.Get(0), ts1Ports);

    // ts2 bridges: ts1, ts3, t3
    NetDeviceContainer ts2Ports;
    ts2Ports.Add (dev_ts1_ts2.Get (1)); // port from ts1
    ts2Ports.Add (dev_ts2_ts3.Get (0)); // to ts3
    ts2Ports.Add (dev_t3_ts2.Get (1)); // from t3
    bridge.Install (ts2.Get(0), ts2Ports);

    // ts3 bridges: ts2, ts4
    NetDeviceContainer ts3Ports;
    ts3Ports.Add (dev_ts2_ts3.Get (1));
    ts3Ports.Add (dev_ts3_ts4.Get (0));
    bridge.Install (ts3.Get(0), ts3Ports);

    // ts4 bridges: ts3, tr
    NetDeviceContainer ts4Ports;
    ts4Ports.Add (dev_ts3_ts4.Get (1));
    ts4Ports.Add (dev_ts4_tr.Get (0));
    bridge.Install (ts4.Get(0), ts4Ports);

    // Bottom switches bridge their CSMA devices
    // bs1 bridges: b2, bs2
    NetDeviceContainer bs1Ports;
    bs1Ports.Add (dev_b2_bs1.Get (1));
    bs1Ports.Add (dev_bs1_bs2.Get (0));
    bridge.Install (bs1.Get(0), bs1Ports);

    // bs2 bridges: bs1, bs3
    NetDeviceContainer bs2Ports;
    bs2Ports.Add (dev_bs1_bs2.Get (1));
    bs2Ports.Add (dev_bs2_bs3.Get (0));
    bridge.Install (bs2.Get(0), bs2Ports);

    // bs3 bridges: bs2, bs4, b3
    NetDeviceContainer bs3Ports;
    bs3Ports.Add (dev_bs2_bs3.Get (1));
    bs3Ports.Add (dev_bs3_bs4.Get (0));
    bs3Ports.Add (dev_b3_bs3.Get (1));
    bridge.Install (bs3.Get(0), bs3Ports);

    // bs4 bridges: bs3, bs5
    NetDeviceContainer bs4Ports;
    bs4Ports.Add (dev_bs3_bs4.Get (1));
    bs4Ports.Add (dev_bs4_bs5.Get (0));
    bridge.Install (bs4.Get(0), bs4Ports);

    // bs5 bridges: bs4, br
    NetDeviceContainer bs5Ports;
    bs5Ports.Add (dev_bs4_bs5.Get (1));
    bs5Ports.Add (dev_bs5_br.Get (0));
    bridge.Install (bs5.Get(0), bs5Ports);

    // --- INTERNET ---
    InternetStackHelper internet;
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    internet.Install (t2);
    internet.Install (t3);
    internet.Install (tr);
    internet.Install (b2);
    internet.Install (b3);
    internet.Install (br);

    // --- ADDRESSING ---
    Ipv4AddressHelper address;

    // Top LAN: 192.168.1.0/24
    address.SetBase ("192.168.1.0", "255.255.255.0");
    // Assign one interface to t2, t3, tr on LAN side (router LAN interface)
    NetDeviceContainer topLanDevs;
    // t2: dev_t2_ts1.Get(0)
    // t3: dev_t3_ts2.Get(0)
    // tr: dev_ts4_tr.Get(1)
    topLanDevs.Add (dev_t2_ts1.Get(0));
    topLanDevs.Add (dev_t3_ts2.Get(0));
    topLanDevs.Add (dev_ts4_tr.Get(1));
    Ipv4InterfaceContainer if_topLan = address.Assign (topLanDevs);

    // WAN subnet: 76.1.1.0/30
    address.SetBase ("76.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer if_wan = address.Assign (dev_tr_br);

    // Bottom LAN: 192.168.2.0/24
    address.SetBase ("192.168.2.0", "255.255.255.0");
    NetDeviceContainer botLanDevs;
    // b2: dev_b2_bs1.Get(0)
    // b3: dev_b3_bs3.Get(0)
    // br: dev_bs5_br.Get(1)
    botLanDevs.Add (dev_b2_bs1.Get(0));
    botLanDevs.Add (dev_b3_bs3.Get(0));
    botLanDevs.Add (dev_bs5_br.Get(1));
    Ipv4InterfaceContainer if_botLan = address.Assign (botLanDevs);

    // --- APPLICATIONS ---

    uint16_t echoPort = 9;

    // b2 runs UDP Echo server
    UdpEchoServerHelper echoServer1 (echoPort);
    ApplicationContainer serverApps1 = echoServer1.Install (b2.Get (0));
    serverApps1.Start (Seconds (1.0));
    serverApps1.Stop (Seconds (10.0));

    // b3 runs UDP Echo server
    UdpEchoServerHelper echoServer2 (echoPort);
    ApplicationContainer serverApps2 = echoServer2.Install (b3.Get (0));
    serverApps2.Start (Seconds (1.0));
    serverApps2.Stop (Seconds (10.0));

    // t2 sends echo to b2 (path: t2-ts1-ts2-ts3-ts4-tr-WAN-br-bs5-bs4-bs3-bs2-bs1-b2)
    UdpEchoClientHelper echoClient1 (if_botLan.GetAddress (0), echoPort);
    echoClient1.SetAttribute ("MaxPackets", UintegerValue (3));
    echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient1.SetAttribute ("PacketSize", UintegerValue (64));

    ApplicationContainer clientApps1 = echoClient1.Install (t2.Get (0));
    clientApps1.Start (Seconds (2.0));
    clientApps1.Stop (Seconds (10.0));

    // t3 sends echo to b3 (path: t3-ts2-tr-WAN-br-bs5-bs4-bs3-b3)
    UdpEchoClientHelper echoClient2 (if_botLan.GetAddress (1), echoPort);
    echoClient2.SetAttribute ("MaxPackets", UintegerValue (3));
    echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient2.SetAttribute ("PacketSize", UintegerValue (64));

    ApplicationContainer clientApps2 = echoClient2.Install (t3.Get (0));
    clientApps2.Start (Seconds (2.0));
    clientApps2.Stop (Seconds (10.0));

    // -- TRACING --
    csma.EnablePcap ("top-lan-t2", dev_t2_ts1.Get(0), true, true);
    csma.EnablePcap ("top-lan-t3", dev_t3_ts2.Get(0), true, true);
    csma.EnablePcap ("top-lan-router", dev_ts4_tr.Get(1), true, true);
    csma.EnablePcap ("bottom-lan-b2", dev_b2_bs1.Get(0), true, true);
    csma.EnablePcap ("bottom-lan-b3", dev_b3_bs3.Get(0), true, true);
    csma.EnablePcap ("bottom-lan-router", dev_bs5_br.Get(1), true, true);
    p2p.EnablePcap ("wan", dev_tr_br.Get(0), true, true);
    p2p.EnablePcap ("wan", dev_tr_br.Get(1), true, true);

    // Schedule populating global routing after all interfaces are up
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    Simulator::Stop (Seconds (11.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}