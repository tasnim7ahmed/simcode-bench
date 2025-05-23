#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    // 1. Command Line Arguments
    std::string lanDataRate = "100Mbps";
    std::string wanDataRate = "10Mbps";
    std::string wanDelay = "2ms";
    double simulationTime = 10.0; // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("lanRate", "LAN data rate (e.g., 10Mbps, 100Mbps)", lanDataRate);
    cmd.AddValue("wanRate", "WAN data rate (e.g., 1Mbps, 10Mbps)", wanDataRate);
    cmd.AddValue("wanDelay", "WAN delay (e.g., 1ms, 10ms)", wanDelay);
    cmd.AddValue("simTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // 2. Node Creation
    // Routers
    Ptr<Node> tr = CreateObject<Node>(); // top router
    Ptr<Node> br = CreateObject<Node>(); // bottom router

    // End hosts
    Ptr<Node> t2 = CreateObject<Node>(); // top client (multi-switch path)
    Ptr<Node> t3 = CreateObject<Node>(); // top server (single-switch path)
    Ptr<Node> b2 = CreateObject<Node>(); // bottom server (multi-switch path)
    Ptr<Node> b3 = CreateObject<Node>(); // bottom client (single-switch path)

    // Switches
    // Top LAN switches
    Ptr<Node> tsw1 = CreateObject<Node>(); // Main switch connecting tr, tsw2, tsw3
    Ptr<Node> tsw2 = CreateObject<Node>(); // Switch for t2 path
    Ptr<Node> tsw3 = CreateObject<Node>(); // Switch for t3 path

    // Bottom LAN switches
    Ptr<Node> bsw1 = CreateObject<Node>(); // Main switch connecting br, bsw2, bsw3
    Ptr<Node> bsw2 = CreateObject<Node>(); // Switch for b2 path
    Ptr<Node> bsw3 = CreateObject<Node>(); // Switch for b3 path

    // 3. Internet Stack Installation
    InternetStackHelper internet;
    internet.Install(tr);
    internet.Install(br);
    internet.Install(t2);
    internet.Install(t3);
    internet.Install(b2);
    internet.Install(b3);
    // Switches do not need InternetStack, as they operate at L2

    // 4. WAN Link Setup (Point-to-Point)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(wanDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(wanDelay));

    NetDeviceContainer wanDevices = p2p.Install(tr, br);

    Ipv4AddressHelper wanIpv4;
    wanIpv4.SetBase("10.0.0.0", "255.255.255.0");
    wanIpv4.Assign(wanDevices);

    // 5. LAN Link Setup (CSMA & Bridging)
    CsmaHelper csma;
    csma.SetDeviceAttribute("DataRate", StringValue(lanDataRate));
    csma.SetChannelAttribute("Delay", StringValue("20us")); // A small delay for LAN segments

    // 5.1. Top LAN (192.168.1.0/24)
    // Create CSMA segments connecting nodes/routers to switches
    NetDeviceContainer tr_tsw1_devs = csma.Install(NodeContainer(tr, tsw1));      // tr <-> tsw1
    NetDeviceContainer tsw1_tsw2_devs = csma.Install(NodeContainer(tsw1, tsw2));  // tsw1 <-> tsw2
    NetDeviceContainer tsw2_t2_devs = csma.Install(NodeContainer(tsw2, t2));      // tsw2 <-> t2
    NetDeviceContainer tsw1_tsw3_devs = csma.Install(NodeContainer(tsw1, tsw3));  // tsw1 <-> tsw3 (interconnecting switches)
    NetDeviceContainer tsw3_t3_devs = csma.Install(NodeContainer(tsw3, t3));      // tsw3 <-> t3

    // Install BridgeNetDevice on switch nodes to bridge their ports
    Ptr<BridgeNetDevice> tsw1_bridge = CreateObject<BridgeNetDevice>();
    tsw1->AddDevice(tsw1_bridge);
    tsw1_bridge->AddLink(tr_tsw1_devs.Get(1));     // tsw1's end of tr-tsw1 link
    tsw1_bridge->AddLink(tsw1_tsw2_devs.Get(0));   // tsw1's end of tsw1-tsw2 link
    tsw1_bridge->AddLink(tsw1_tsw3_devs.Get(0));   // tsw1's end of tsw1-tsw3 link

    Ptr<BridgeNetDevice> tsw2_bridge = CreateObject<BridgeNetDevice>();
    tsw2->AddDevice(tsw2_bridge);
    tsw2_bridge->AddLink(tsw1_tsw2_devs.Get(1));   // tsw2's end of tsw1-tsw2 link
    tsw2_bridge->AddLink(tsw2_t2_devs.Get(0));     // tsw2's end of tsw2-t2 link

    Ptr<BridgeNetDevice> tsw3_bridge = CreateObject<BridgeNetDevice>();
    tsw3->AddDevice(tsw3_bridge);
    tsw3_bridge->AddLink(tsw1_tsw3_devs.Get(1));   // tsw3's end of tsw1-tsw3 link
    tsw3_bridge->AddLink(tsw3_t3_devs.Get(0));     // tsw3's end of tsw3-t3 link

    // Assign IP addresses for Top LAN
    Ipv4AddressHelper topIpv4;
    topIpv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topInterfaces;
    topInterfaces.Add(topIpv4.Assign(tr_tsw1_devs.Get(0)));    // Router tr's LAN interface (192.168.1.1)
    topInterfaces.Add(topIpv4.Assign(tsw2_t2_devs.Get(1)));    // Client t2's interface (192.168.1.2)
    topInterfaces.Add(topIpv4.Assign(tsw3_t3_devs.Get(1)));    // Server t3's interface (192.168.1.3)

    // 5.2. Bottom LAN (192.168.2.0/24)
    // Create CSMA segments
    NetDeviceContainer br_bsw1_devs = csma.Install(NodeContainer(br, bsw1));      // br <-> bsw1
    NetDeviceContainer bsw1_bsw2_devs = csma.Install(NodeContainer(bsw1, bsw2));  // bsw1 <-> bsw2
    NetDeviceContainer bsw2_b2_devs = csma.Install(NodeContainer(bsw2, b2));      // bsw2 <-> b2
    NetDeviceContainer bsw1_bsw3_devs = csma.Install(NodeContainer(bsw1, bsw3));  // bsw1 <-> bsw3 (interconnecting switches)
    NetDeviceContainer bsw3_b3_devs = csma.Install(NodeContainer(bsw3, b3));      // bsw3 <-> b3

    // Install BridgeNetDevice on switch nodes
    Ptr<BridgeNetDevice> bsw1_bridge = CreateObject<BridgeNetDevice>();
    bsw1->AddDevice(bsw1_bridge);
    bsw1_bridge->AddLink(br_bsw1_devs.Get(1));     // bsw1's end of br-bsw1 link
    bsw1_bridge->AddLink(bsw1_bsw2_devs.Get(0));   // bsw1's end of bsw1-bsw2 link
    bsw1_bridge->AddLink(bsw1_bsw3_devs.Get(0));   // bsw1's end of bsw1-bsw3 link

    Ptr<BridgeNetDevice> bsw2_bridge = CreateObject<BridgeNetDevice>();
    bsw2->AddDevice(bsw2_bridge);
    bsw2_bridge->AddLink(bsw1_bsw2_devs.Get(1));   // bsw2's end of bsw1-bsw2 link
    bsw2_bridge->AddLink(bsw2_b2_devs.Get(0));     // bsw2's end of bsw2-b2 link

    Ptr<BridgeNetDevice> bsw3_bridge = CreateObject<BridgeNetDevice>();
    bsw3->AddDevice(bsw3_bridge);
    bsw3_bridge->AddLink(bsw1_bsw3_devs.Get(1));   // bsw3's end of bsw1-bsw3 link
    bsw3_bridge->AddLink(bsw3_b3_devs.Get(0));     // bsw3's end of bsw3-b3 link

    // Assign IP addresses for Bottom LAN
    Ipv4AddressHelper bottomIpv4;
    bottomIpv4.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomInterfaces;
    bottomInterfaces.Add(bottomIpv4.Assign(br_bsw1_devs.Get(0)));    // Router br's LAN interface (192.168.2.1)
    bottomInterfaces.Add(bottomIpv4.Assign(bsw2_b2_devs.Get(1)));    // Server b2's interface (192.168.2.2)
    bottomInterfaces.Add(bottomIpv4.Assign(bsw3_b3_devs.Get(1)));    // Client b3's interface (192.168.2.3)

    // 6. Routing
    // Enable global static routing so that applications can send packets
    // and they can be routed between networks.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 7. Applications (UDP Echo Clients and Servers)
    // Server addresses (using assigned IP addresses from Ipv4InterfaceContainer)
    Ipv4Address t3Addr = topInterfaces.GetAddress(2);    // t3 is the 3rd assigned IP for top LAN (192.168.1.3)
    Ipv4Address b2Addr = bottomInterfaces.GetAddress(1); // b2 is the 2nd assigned IP for bottom LAN (192.168.2.2)

    uint16_t echoPort = 9;
    uint32_t maxPackets = 3;
    Time packetInterval = Seconds(1.0);
    uint32_t packetSize = 1024;

    // UDP Echo Servers
    UdpEchoServerHelper echoServerT3(echoPort);
    ApplicationContainer serverAppsT3 = echoServerT3.Install(t3);
    serverAppsT3.Start(Seconds(1.0));
    serverAppsT3.Stop(Seconds(simulationTime - 1.0));

    UdpEchoServerHelper echoServerB2(echoPort);
    ApplicationContainer serverAppsB2 = echoServerB2.Install(b2);
    serverAppsB2.Start(Seconds(1.0));
    serverAppsB2.Stop(Seconds(simulationTime - 1.0));

    // UDP Echo Clients
    // Client t2 (top LAN) to Server t3 (top LAN)
    UdpEchoClientHelper echoClientT2ToT3(t3Addr, echoPort);
    echoClientT2ToT3.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClientT2ToT3.SetAttribute("Interval", TimeValue(packetInterval));
    echoClientT2ToT3.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientAppsT2T3 = echoClientT2ToT3.Install(t2);
    clientAppsT2T3.Start(Seconds(2.0));
    clientAppsT2T3.Stop(Seconds(simulationTime - 1.0));

    // Client t2 (top LAN) to Server b2 (bottom LAN) - Cross-LAN
    UdpEchoClientHelper echoClientT2ToB2(b2Addr, echoPort);
    echoClientT2ToB2.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClientT2ToB2.SetAttribute("Interval", TimeValue(packetInterval));
    echoClientT2ToB2.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientAppsT2B2 = echoClientT2ToB2.Install(t2);
    clientAppsT2B2.Start(Seconds(3.0)); // Stagger start times
    clientAppsT2B2.Stop(Seconds(simulationTime - 1.0));

    // Client b3 (bottom LAN) to Server b2 (bottom LAN)
    UdpEchoClientHelper echoClientB3ToB2(b2Addr, echoPort);
    echoClientB3ToB2.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClientB3ToB2.SetAttribute("Interval", TimeValue(packetInterval));
    echoClientB3ToB2.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientAppsB3B2 = echoClientB3ToB2.Install(b3);
    clientAppsB3B2.Start(Seconds(2.0));
    clientAppsB3B2.Stop(Seconds(simulationTime - 1.0));

    // Client b3 (bottom LAN) to Server t3 (top LAN) - Cross-LAN
    UdpEchoClientHelper echoClientB3ToT3(t3Addr, echoPort);
    echoClientB3ToT3.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClientB3ToT3.SetAttribute("Interval", TimeValue(packetInterval));
    echoClientB3ToT3.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientAppsB3T3 = echoClientB3ToT3.Install(b3);
    clientAppsB3T3.Start(Seconds(4.0)); // Stagger start times
    clientAppsB3T3.Stop(Seconds(simulationTime - 1.0));

    // 8. Tracing
    p2p.EnablePcapAll("wan-link-trace");
    csma.EnablePcapAll("lan-link-trace"); // Generates traces for all CSMA segments

    // Enable NetAnim visualization (optional)
    AnimationInterface anim("multi-switch-bridge.xml");
    
    // Top LAN nodes
    anim.SetConstantPosition(tr, 10.0, 70.0);
    anim.SetConstantPosition(tsw1, 25.0, 70.0);
    anim.SetConstantPosition(tsw2, 40.0, 60.0);
    anim.SetConstantPosition(tsw3, 40.0, 80.0);
    anim.SetConstantPosition(t2, 55.0, 60.0);
    anim.SetConstantPosition(t3, 55.0, 80.0);

    // Bottom LAN nodes
    anim.SetConstantPosition(br, 90.0, 30.0);
    anim.SetConstantPosition(bsw1, 75.0, 30.0);
    anim.SetConstantPosition(bsw2, 60.0, 20.0);
    anim.SetConstantPosition(bsw3, 60.0, 40.0);
    anim.SetConstantPosition(b2, 45.0, 20.0);
    anim.SetConstantPosition(b3, 45.0, 40.0);

    // 9. Run Simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}