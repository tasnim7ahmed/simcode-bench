// wifi-three-ap-six-sta.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <vector>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiThreeApSixStaExample");

int main(int argc, char *argv[])
{
    uint32_t nAps = 3;
    uint32_t nStas = 6;
    uint32_t nTotalNodes = nAps + nStas;

    double simTime = 20.0; // seconds
    double distanceBetweenAps = 30.0; // meters

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create AP and STA nodes
    NodeContainer apNodes;
    apNodes.Create(nAps);

    NodeContainer staNodes;
    staNodes.Create(nStas);

    // Create remote server node
    NodeContainer serverNode;
    serverNode.Create(1);

    // Setup Wi-Fi channel and helper
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("RxGain", DoubleValue(0));

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper wifiMac;

    // SSID list for three APs
    std::vector<Ssid> ssidList;
    for (uint32_t i = 0; i < nAps; ++i)
    {
        std::ostringstream ss;
        ss << "wifi-ssid-ap" << i;
        ssidList.push_back(Ssid(ss.str()));
    }

    // Divide stations among APs: 2 per AP
    std::vector<NodeContainer> staGroups(nAps);
    for (uint32_t i = 0; i < nStas; ++i)
    {
        staGroups[i % nAps].Add(staNodes.Get(i));
    }

    NetDeviceContainer apDevices;
    std::vector<NetDeviceContainer> staDevicesByAp(nAps);

    for (uint32_t i = 0; i < nAps; ++i)
    {
        // Configure AP device
        wifiMac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssidList[i]));

        NetDeviceContainer apDevice = wifiHelper.Install(wifiPhy, wifiMac, apNodes.Get(i));
        apDevices.Add(apDevice);

        // Configure STA devices associated to this AP
        wifiMac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssidList[i]),
                        "ActiveProbing", BooleanValue(false));

        NetDeviceContainer staDevices = wifiHelper.Install(wifiPhy, wifiMac, staGroups[i]);
        staDevicesByAp[i] = staDevices;
    }

    // Mobility model for APs: fixed positions
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nAps; ++i)
    {
        apPosAlloc->Add(Vector(distanceBetweenAps * i, 0.0, 0.0));
    }
    mobilityAp.SetPositionAllocator(apPosAlloc);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(apNodes);

    // Mobility for STAs: use RandomWaypointMobilityModel within area encompassing the APs
    MobilityHelper mobilitySta;
    mobilitySta.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=60.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=20.0]"));
    mobilitySta.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                 "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                                 "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                 "PositionAllocator", PointerValue (mobilitySta.GetPositionAllocator ()));
    mobilitySta.Install(staNodes);

    // Mobility for server node: remains fixed, off to the side
    MobilityHelper mobilityServer;
    Ptr<ListPositionAllocator> serverPosAlloc = CreateObject<ListPositionAllocator>();
    serverPosAlloc->Add(Vector(30.0, -20.0, 0.0));
    mobilityServer.SetPositionAllocator(serverPosAlloc);
    mobilityServer.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityServer.Install(serverNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);
    stack.Install(serverNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> apIfaces(nAps);
    std::vector<Ipv4InterfaceContainer> staIfacesByAp(nAps);

    for (uint32_t i = 0; i < nAps; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        apIfaces[i] = address.Assign(apDevices.Get(i));
        staIfacesByAp[i] = address.Assign(staDevicesByAp[i]);
        address.NewNetwork();
    }

    // Use CSMA or PointToPoint to connect all APs to remote server (backbone)
    NodeContainer backboneNodes;
    for (uint32_t i = 0; i < nAps; ++i) {
        backboneNodes.Add(apNodes.Get(i));
    }
    backboneNodes.Add(serverNode.Get(0));

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
    csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
    NetDeviceContainer csmaDevices = csma.Install(backboneNodes);

    // Assign IP addresses for backbone
    address.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer backboneIfaces = address.Assign(csmaDevices);

    // Populate routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP traffic:
    // Each STA sends to remote server (uplink)
    // Remote server sends downlink to each STA

    uint16_t serverPortBase = 4000;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    // One UDP server app for each flow direction (STA->Server and Server->STA)
    for (uint32_t i = 0; i < nStas; ++i)
    {
        // Assign each STA an index and AP number for addressing
        uint32_t apIdx = i % nAps;
        Ptr<Node> staNode = staNodes.Get(i);

        // Uplink: STA->Server (UDP client on STA, UDP server on Server)
        uint16_t uplinkPort = serverPortBase + i;
        UdpServerHelper uplinkServer(uplinkPort);
        serverApps.Add(uplinkServer.Install(serverNode.Get(0)));

        UdpClientHelper uplinkClient(backboneIfaces.GetAddress(nAps), uplinkPort);
        uplinkClient.SetAttribute("MaxPackets", UintegerValue (4294967295u));
        uplinkClient.SetAttribute("Interval", TimeValue (MilliSeconds(50)));
        uplinkClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer app = uplinkClient.Install(staNode);
        app.Start(Seconds(2.0));
        app.Stop(Seconds(simTime - 1));
        clientApps.Add(app);

        // Downlink: Server->STA (UDP client on Server, UDP server on STA)
        uint16_t downlinkPort = serverPortBase + nStas + i;
        UdpServerHelper downlinkServer(downlinkPort);
        serverApps.Add(downlinkServer.Install(staNode));

        Ipv4Address staAddr = staIfacesByAp[apIdx].GetAddress(i / nAps);

        UdpClientHelper downlinkClient(staAddr, downlinkPort);
        downlinkClient.SetAttribute("MaxPackets", UintegerValue (4294967295u));
        downlinkClient.SetAttribute("Interval", TimeValue (MilliSeconds(50)));
        downlinkClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer app2 = downlinkClient.Install(serverNode.Get(0));
        app2.Start(Seconds(2.0));
        app2.Stop(Seconds(simTime - 1));
        clientApps.Add(app2);
    }

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // NetAnim
    AnimationInterface anim("wifi-three-ap-six-sta.xml");

    // Annotate APs and STA nodes for NetAnim
    for (uint32_t i = 0; i < nAps; ++i)
    {
        std::ostringstream oss;
        oss << "AP" << i;
        anim.UpdateNodeDescription(apNodes.Get(i), oss.str());
        anim.UpdateNodeColor(apNodes.Get(i), 0, 255, 0); // Green
    }
    for (uint32_t i = 0; i < nStas; ++i)
    {
        std::ostringstream oss;
        oss << "STA" << i;
        anim.UpdateNodeDescription(staNodes.Get(i), oss.str());
        anim.UpdateNodeColor(staNodes.Get(i), 0, 0, 255); // Blue
    }
    anim.UpdateNodeDescription(serverNode.Get(0), "Server");
    anim.UpdateNodeColor(serverNode.Get(0), 255, 0, 0); // Red

    // Run simulation
    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}