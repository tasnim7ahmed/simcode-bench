#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LanWanLanWithMultipleSwitches");

int 
main (int argc, char *argv[])
{
    std::string csmaRate = "100Mbps";
    std::string csmaDelay = "2us";
    std::string wanRate = "10Mbps";
    std::string wanDelay = "10ms";

    CommandLine cmd;
    cmd.AddValue ("csmaRate", "Data rate for LAN CSMA links (100Mbps or 10Mbps)", csmaRate);
    cmd.AddValue ("csmaDelay", "Delay for LAN CSMA links", csmaDelay);
    cmd.AddValue ("wanRate",  "Data rate for WAN point-to-point link", wanRate);
    cmd.AddValue ("wanDelay", "Delay for WAN link", wanDelay);
    cmd.Parse (argc, argv);

    // Create all nodes
    NodeContainer topHosts; topHosts.Create(2); // t2 (client), t3 (server)
    NodeContainer topRouter; topRouter.Create(1); // tr
    NodeContainer bottomHosts; bottomHosts.Create(2); // b2 (server), b3 (client)
    NodeContainer bottomRouter; bottomRouter.Create(1); // br

    // Top LAN switches: sw1, sw2, sw3
    NodeContainer topSwitches; topSwitches.Create(3);
    // Bottom LAN switches: sw4, sw5, sw6
    NodeContainer bottomSwitches; bottomSwitches.Create(3);

    // Alias nodes for convenience
    Ptr<Node> t2 = topHosts.Get(0);
    Ptr<Node> t3 = topHosts.Get(1);
    Ptr<Node> tr = topRouter.Get(0);
    Ptr<Node> b2 = bottomHosts.Get(0);
    Ptr<Node> b3 = bottomHosts.Get(1);
    Ptr<Node> br = bottomRouter.Get(0);

    // CSMA helpers for LANs
    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (csmaRate)));
    csma.SetChannelAttribute ("Delay", TimeValue (Time (csmaDelay)));

    // === Create and connect TOP LAN ===
    // Switches: sw1 (core), sw2, sw3
   
    // t2 <--sw2--sw1--sw3--> tr
    // t3 <--sw3--> sw1

    // CSMA devices for t2 <-> sw2
    NodeContainer top_t2_sw2;
    top_t2_sw2.Add (t2);
    top_t2_sw2.Add (topSwitches.Get(1)); // sw2
    NetDeviceContainer ndc_t2_sw2 = csma.Install (top_t2_sw2);

    // sw2 <-> sw1
    NodeContainer top_sw2_sw1;
    top_sw2_sw1.Add (topSwitches.Get(1)); // sw2
    top_sw2_sw1.Add (topSwitches.Get(0)); // sw1
    NetDeviceContainer ndc_sw2_sw1 = csma.Install (top_sw2_sw1);

    // sw1 <-> sw3
    NodeContainer top_sw1_sw3;
    top_sw1_sw3.Add (topSwitches.Get(0)); // sw1
    top_sw1_sw3.Add (topSwitches.Get(2)); // sw3
    NetDeviceContainer ndc_sw1_sw3 = csma.Install (top_sw1_sw3);

    // sw3 <-> tr
    NodeContainer top_sw3_tr;
    top_sw3_tr.Add (topSwitches.Get(2)); // sw3
    top_sw3_tr.Add (tr);
    NetDeviceContainer ndc_sw3_tr = csma.Install (top_sw3_tr);

    // t3 <-> sw1
    NodeContainer top_t3_sw1;
    top_t3_sw1.Add (t3);
    top_t3_sw1.Add (topSwitches.Get(0)); // sw1
    NetDeviceContainer ndc_t3_sw1 = csma.Install (top_t3_sw1);

    // Assign switch bridge netdevs (top)
    BridgeHelper bridge;
    NetDeviceContainer sw1_ports, sw2_ports, sw3_ports;
    // sw1: connects (sw2, sw3, t3)
    sw1_ports.Add (ndc_sw2_sw1.Get(1)); // sw2->sw1
    sw1_ports.Add (ndc_sw1_sw3.Get(0)); // sw1->sw3
    sw1_ports.Add (ndc_t3_sw1.Get(1));  // t3->sw1
    bridge.Install (topSwitches.Get(0), sw1_ports);
    // sw2: connects (t2, sw1)
    sw2_ports.Add (ndc_t2_sw2.Get(1)); // t2->sw2
    sw2_ports.Add (ndc_sw2_sw1.Get(0)); // sw2->sw1
    bridge.Install (topSwitches.Get(1), sw2_ports);
    // sw3: connects (sw1, tr)
    sw3_ports.Add (ndc_sw1_sw3.Get(1)); // sw1->sw3
    sw3_ports.Add (ndc_sw3_tr.Get(0));  // sw3->tr
    bridge.Install (topSwitches.Get(2), sw3_ports);

    // === Create and connect BOTTOM LAN ===
    // Switches: sw4 (core), sw5, sw6

    // b2 <--sw5--sw4--sw6--> br
    // b3 <--sw4--> (single switch path)

    // b2 <-> sw5
    NodeContainer bot_b2_sw5;
    bot_b2_sw5.Add (b2);
    bot_b2_sw5.Add (bottomSwitches.Get(1)); // sw5
    NetDeviceContainer ndc_b2_sw5 = csma.Install (bot_b2_sw5);

    // sw5 <-> sw4
    NodeContainer bot_sw5_sw4;
    bot_sw5_sw4.Add (bottomSwitches.Get(1)); // sw5
    bot_sw5_sw4.Add (bottomSwitches.Get(0)); // sw4
    NetDeviceContainer ndc_sw5_sw4 = csma.Install (bot_sw5_sw4);

    // sw4 <-> sw6
    NodeContainer bot_sw4_sw6;
    bot_sw4_sw6.Add (bottomSwitches.Get(0)); // sw4
    bot_sw4_sw6.Add (bottomSwitches.Get(2)); // sw6
    NetDeviceContainer ndc_sw4_sw6 = csma.Install (bot_sw4_sw6);

    // sw6 <-> br
    NodeContainer bot_sw6_br;
    bot_sw6_br.Add (bottomSwitches.Get(2)); // sw6
    bot_sw6_br.Add (br);
    NetDeviceContainer ndc_sw6_br = csma.Install (bot_sw6_br);

    // b3 <-> sw4 (single switch for b3)
    NodeContainer bot_b3_sw4;
    bot_b3_sw4.Add (b3);
    bot_b3_sw4.Add (bottomSwitches.Get(0)); // sw4
    NetDeviceContainer ndc_b3_sw4 = csma.Install (bot_b3_sw4);

    // Assign switch bridge netdevs (bottom)
    NetDeviceContainer sw4_ports, sw5_ports, sw6_ports;
    // sw4: connects (sw5, sw6, b3)
    sw4_ports.Add (ndc_sw5_sw4.Get(1)); // sw5->sw4
    sw4_ports.Add (ndc_sw4_sw6.Get(0)); // sw4->sw6
    sw4_ports.Add (ndc_b3_sw4.Get(1));  // b3->sw4
    bridge.Install (bottomSwitches.Get(0), sw4_ports);
    // sw5: connects (b2, sw4)
    sw5_ports.Add (ndc_b2_sw5.Get(1));   // b2->sw5
    sw5_ports.Add (ndc_sw5_sw4.Get(0));  // sw5->sw4
    bridge.Install (bottomSwitches.Get(1), sw5_ports);
    // sw6: connects (sw4, br)
    sw6_ports.Add (ndc_sw4_sw6.Get(1));   // sw4->sw6
    sw6_ports.Add (ndc_sw6_br.Get(0));    // sw6->br
    bridge.Install (bottomSwitches.Get(2), sw6_ports);

    // === WAN link: tr <-> br ===
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue (wanRate));
    p2p.SetChannelAttribute ("Delay", StringValue (wanDelay));
    NetDeviceContainer ndc_tr_br = p2p.Install (tr, br);

    // === Internet stack and addressing ===
    InternetStackHelper stack;
    stack.Install (topHosts);
    stack.Install (tr);
    stack.Install (bottomHosts);
    stack.Install (br);

    // IP addressing
    Ipv4AddressHelper ipv4;

    // Top LAN: 192.168.1.0/24
    // Gather all non-bridge devices: t2, t3, tr
    NetDeviceContainer topLanHostsDevs;
    topLanHostsDevs.Add (ndc_t2_sw2.Get(0)); // t2
    topLanHostsDevs.Add (ndc_t3_sw1.Get(0)); // t3
    topLanHostsDevs.Add (ndc_sw3_tr.Get(1)); // tr (LAN side)

    ipv4.SetBase ("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topLanIfs = ipv4.Assign (topLanHostsDevs);

    // WAN: 10.0.0.0/30
    ipv4.SetBase ("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer wanIfs = ipv4.Assign (ndc_tr_br);

    // Bottom LAN: 192.168.2.0/24
    NetDeviceContainer bottomLanHostsDevs;
    bottomLanHostsDevs.Add (ndc_b2_sw5.Get(0)); // b2
    bottomLanHostsDevs.Add (ndc_b3_sw4.Get(0)); // b3
    bottomLanHostsDevs.Add (ndc_sw6_br.Get(1)); // br (LAN side)

    ipv4.SetBase ("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomLanIfs = ipv4.Assign (bottomLanHostsDevs);

    // Enable global IP routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // === Applications ===

    // 1. t2 (top client) -> b2 (bottom server)
    uint16_t echoPort1 = 9;
    UdpEchoServerHelper echoServer1 (echoPort1);
    ApplicationContainer serverApps1 = echoServer1.Install (b2);
    serverApps1.Start (Seconds (1.0));
    serverApps1.Stop (Seconds (11.0));

    UdpEchoClientHelper echoClient1 (bottomLanIfs.GetAddress(0), echoPort1);
    echoClient1.SetAttribute ("MaxPackets", UintegerValue (3));
    echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient1.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApps1 = echoClient1.Install (t2);
    clientApps1.Start (Seconds (2.0));
    clientApps1.Stop (Seconds (11.0));

    // 2. b3 (bottom client) -> t3 (top server)
    uint16_t echoPort2 = 10;
    UdpEchoServerHelper echoServer2 (echoPort2);
    ApplicationContainer serverApps2 = echoServer2.Install (t3);
    serverApps2.Start (Seconds (1.0));
    serverApps2.Stop (Seconds (11.0));

    UdpEchoClientHelper echoClient2 (topLanIfs.GetAddress(1), echoPort2);
    echoClient2.SetAttribute ("MaxPackets", UintegerValue (3));
    echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient2.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApps2 = echoClient2.Install (b3);
    clientApps2.Start (Seconds (2.0));
    clientApps2.Stop (Seconds (11.0));

    // pcap tracing for debugging
    csma.EnablePcapAll ("lan-wan-lan", false);
    p2p.EnablePcapAll ("lan-wan-lan", false);

    Simulator::Stop (Seconds (12.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}