#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ssid.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedWiredWirelessTopology");

void CourseChangeCallback (std::string context, Ptr<const MobilityModel> model)
{
    Vector pos = model->GetPosition ();
    NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s: "
                   << context << ", Position: "
                   << pos.x << ", " << pos.y << ", " << pos.z);
}

int main (int argc, char *argv[])
{
    // Default simulation parameters (can be overridden via command-line)
    uint32_t numBackboneRouters = 3;
    uint32_t numLanNodesPerBackbone = 2;
    uint32_t numStaPerInfra = 2;
    bool enablePcap = true;
    bool enableCourseChangeTrace = true;
    double simTime = 20.0;

    CommandLine cmd;
    cmd.AddValue ("numBackboneRouters", "Number of backbone routers (default: 3)", numBackboneRouters);
    cmd.AddValue ("numLanNodesPerBackbone", "Number of CSMA nodes per backbone (default: 2)", numLanNodesPerBackbone);
    cmd.AddValue ("numStaPerInfra", "Number of STA nodes per infrastructure (default: 2)", numStaPerInfra);
    cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue ("enableCourseChangeTrace", "Enable course change trace", enableCourseChangeTrace);
    cmd.AddValue ("simTime", "Simulation time (s)", simTime);
    cmd.Parse (argc, argv);

    NodeContainer backboneRouters;
    backboneRouters.Create (numBackboneRouters);

    // For LAN nodes and infrastructure WiFi STA/AP
    std::vector<NodeContainer> lanNodes (numBackboneRouters);
    std::vector<NodeContainer> infraStaNodes (numBackboneRouters);
    std::vector<Ptr<Node>> infraApNodes (numBackboneRouters);

    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        lanNodes[i].Create (numLanNodesPerBackbone);
        infraStaNodes[i].Create (numStaPerInfra);
        infraApNodes[i] = backboneRouters.Get (i);
    }

    // 1. Configure Ad-hoc WiFi for backbone
    YansWifiChannelHelper adhocChannel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper adhocPhy = YansWifiPhyHelper::Default ();
    adhocPhy.SetChannel (adhocChannel.Create ());

    WifiHelper adhocWifi;
    adhocWifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    WifiMacHelper adhocMac;
    adhocMac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer adhocDevices = adhocWifi.Install (adhocPhy, adhocMac, backboneRouters);

    // 2. Configure infrastructure WiFi for each backbone router
    std::vector<NetDeviceContainer> infraApDevices (numBackboneRouters);
    std::vector<NetDeviceContainer> infraStaDevices (numBackboneRouters);

    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        // WiFi channel and phy (shared within each BSS)
        YansWifiChannelHelper infraChannel = YansWifiChannelHelper::Default ();
        YansWifiPhyHelper infraPhy = YansWifiPhyHelper::Default ();
        infraPhy.SetChannel (infraChannel.Create ());

        WifiHelper infraWifi;
        infraWifi.SetStandard (WIFI_PHY_STANDARD_80211g);
        WifiMacHelper infraMac;

        std::ostringstream ssid;
        ssid << "infra-ssid-" << i;
        Ssid apSsid = Ssid (ssid.str ());

        // AP MAC
        infraMac.SetType ("ns3::ApWifiMac",
                          "Ssid", SsidValue(apSsid));
        infraApDevices[i] = infraWifi.Install (infraPhy, infraMac, infraApNodes[i]);

        // STA MAC
        infraMac.SetType ("ns3::StaWifiMac",
                          "Ssid", SsidValue(apSsid),
                          "ActiveProbing", BooleanValue(false));
        infraStaDevices[i] = infraWifi.Install (infraPhy, infraMac, infraStaNodes[i]);
    }

    // 3. CSMA LAN for each backbone router
    std::vector<NetDeviceContainer> csmaLanDevices (numBackboneRouters);

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        NodeContainer csmaNodes;
        csmaNodes.Add (infraApNodes[i]);
        csmaNodes.Add (lanNodes[i]);
        csmaLanDevices[i] = csma.Install (csmaNodes);
    }

    // 4. Install Internet Stack & OLSR on backbone/ad-hoc
    InternetStackHelper stack;
    OlsrHelper olsr;
    Ipv4ListRoutingHelper list;
    list.Add (olsr, 10);
    list.Add (Ipv4StaticRoutingHelper (), 1);

    // Install OLSR on backbone routers (all interfaces!), and static on all other nodes
    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        stack.SetRoutingHelper (list);
        stack.Install (backboneRouters.Get (i));
    }
    stack.SetRoutingHelper (Ipv4StaticRoutingHelper ());
    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        stack.Install (infraStaNodes[i]);
        stack.Install (lanNodes[i]);
    }

    // 5. Assign IP addresses
    Ipv4AddressHelper address;
    // Ad-hoc backbone network
    address.SetBase ("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer backboneInterfaces = address.Assign (adhocDevices);

    // Each infra WiFi
    std::vector<Ipv4InterfaceContainer> infraApInterfaces (numBackboneRouters);
    std::vector<Ipv4InterfaceContainer> infraStaInterfaces (numBackboneRouters);

    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i << ".0";
        address.SetBase (subnet.str ().c_str (), "255.255.255.0");
        infraApInterfaces[i]  = address.Assign (infraApDevices[i]);
        infraStaInterfaces[i] = address.Assign (infraStaDevices[i]);
    }

    // Each csma LAN
    std::vector<Ipv4InterfaceContainer> csmaLanInterfaces (numBackboneRouters);
    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.3." << i << ".0";
        address.SetBase (subnet.str ().c_str (), "255.255.255.0");
        csmaLanInterfaces[i] = address.Assign (csmaLanDevices[i]);
    }

    // 6. Mobility setup

    // Backbone Routers: fix their positions in a line
    Ptr<ListPositionAllocator> backbonePos = CreateObject<ListPositionAllocator> ();
    for (uint32_t i = 0; i < numBackboneRouters; ++i)
        backbonePos->Add (Vector (50.0 * i, 0, 0));
    MobilityHelper backboneMobility;
    backboneMobility.SetPositionAllocator (backbonePos);
    backboneMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    backboneMobility.Install (backboneRouters);

    // Infrastructure APs: use the positions of backbone routers (already set)

    // Infrastructure STAs: random walk around the AP
    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        MobilityHelper infraStaMobility;
        std::ostringstream stream;
        stream << "ns3::RandomRectanglePositionAllocator";
        infraStaMobility.SetPositionAllocator (
            "ns3::RandomRectanglePositionAllocator",
            "X", StringValue (std::to_string(50.0 * i) + "|Uniform:0:10"),
            "Y", StringValue ("0|Uniform:0:30")
        );
        infraStaMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                           "Bounds", RectangleValue (Rectangle (50.0 * i - 10, 50.0 * i + 10, -10, 10)));
        infraStaMobility.Install (infraStaNodes[i]);
    }

    // LAN nodes: cluster near each backbone router
    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        Ptr<ListPositionAllocator> lanPos = CreateObject<ListPositionAllocator> ();
        for (uint32_t j = 0; j < lanNodes[i].GetN (); ++j)
            lanPos->Add (Vector (50.0 * i + 5 + 2*j, -10.0, 0.0));
        MobilityHelper lanMobility;
        lanMobility.SetPositionAllocator (lanPos);
        lanMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        lanMobility.Install (lanNodes[i]);
    }

    // 7. Course change trace
    if (enableCourseChangeTrace)
    {
        for (uint32_t i = 0; i < numBackboneRouters; ++i)
        {
            // Infra STAs
            for (uint32_t s = 0; s < infraStaNodes[i].GetN (); ++s)
            {
                std::ostringstream context;
                context << "/NodeList/" << infraStaNodes[i].Get (s)->GetId () << "/$ns3::MobilityModel/CourseChange";
                Config::Connect (context.str (), MakeCallback (&CourseChangeCallback));
            }
        }
    }

    // 8. UDP application: LAN node (first backbone) -> Wireless STA (last infrastructure)
    Ptr<Node> srcNode = lanNodes[0].Get (0);
    Ptr<Node> dstNode = infraStaNodes[numBackboneRouters-1].Get (0);

    // The receiving STA's last assigned WiFi address
    Ipv4Address dstAddr = infraStaInterfaces[numBackboneRouters-1].GetAddress (0);

    uint16_t udpPort = 5000;
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer serverApps = udpServer.Install (dstNode);
    serverApps.Start (Seconds (2.0));
    serverApps.Stop (Seconds (simTime));

    UdpClientHelper udpClient (dstAddr, udpPort);
    udpClient.SetAttribute ("MaxPackets", UintegerValue (10000));
    udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    udpClient.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApps = udpClient.Install (srcNode);
    clientApps.Start (Seconds (3.0));
    clientApps.Stop (Seconds (simTime));

    // 9. Enable pcap/tracing
    if (enablePcap)
    {
        for (uint32_t i = 0; i < numBackboneRouters; ++i)
        {
            csma.EnablePcapAll ("mixed-topo-csma-" + std::to_string(i), false);
            YansWifiPhyHelper::Default().EnablePcap ("mixed-topo-adhoc-" + std::to_string(i), adhocDevices.Get(i), false);
            YansWifiPhyHelper::Default().EnablePcap ("mixed-topo-infra-ap-" + std::to_string(i), infraApDevices[i].Get(0), false);
            for (uint32_t s = 0; s < infraStaDevices[i].GetN (); ++s)
                YansWifiPhyHelper::Default().EnablePcap ("mixed-topo-infra-sta-" + std::to_string(i) + "-" + std::to_string(s),
                                            infraStaDevices[i].Get (s), false);
        }
    }

    // 10. FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> flowMonitor = flowmon.InstallAll();

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    flowMonitor->SerializeToXmlFile("mixed-topo.flowmon.xml", true, true);

    Simulator::Destroy ();
    return 0;
}