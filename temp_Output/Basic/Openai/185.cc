#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t nAps = 3;
    uint32_t nStas = 6;
    uint32_t nStasPerAp = nStas / nAps;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer wifiApNodes;
    wifiApNodes.Create(nAps);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStas);

    NodeContainer serverNode;
    serverNode.Create(1); // Remote UDP server

    // Create channel and phy
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi-net");

    NetDeviceContainer staDevices[nAps];
    NetDeviceContainer apDevices[nAps];

    // Divide stations across APs
    for (uint32_t i = 0; i < nAps; ++i)
    {
        NodeContainer staGroup;
        for (uint32_t j = 0; j < nStasPerAp; ++j)
        {
            staGroup.Add(wifiStaNodes.Get(i * nStasPerAp + j));
        }
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
        staDevices[i] = wifi.Install(phy, mac, staGroup);

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
        apDevices[i] = wifi.Install(phy, mac, wifiApNodes.Get(i));
    }

    // Server uses CSMA for simplicity and connectivity
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NodeContainer apAndServer;
    for (uint32_t i = 0; i < nAps; ++i)
        apAndServer.Add(wifiApNodes.Get(i));
    apAndServer.Add(serverNode);

    NetDeviceContainer csmaDevices = csma.Install(apAndServer);

    // Mobility: APs are fixed, STAs move
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> posAp = CreateObject<ListPositionAllocator>();
    posAp->Add(Vector(0.0, 0.0, 0.0));
    posAp->Add(Vector(50.0, 0.0, 0.0));
    posAp->Add(Vector(100.0, 0.0, 0.0));
    mobilityAp.SetPositionAllocator(posAp);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNodes);

    MobilityHelper mobilitySta;
    mobilitySta.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(10.0),
                                    "MinY", DoubleValue(30.0),
                                    "DeltaX", DoubleValue(30.0),
                                    "DeltaY", DoubleValue(20.0),
                                    "GridWidth", UintegerValue(3),
                                    "LayoutType", StringValue("RowFirst"));
    mobilitySta.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                                "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                                "PositionAllocator", PointerValue(mobilitySta.GetPositionAllocator()));
    mobilitySta.Install(wifiStaNodes);

    MobilityHelper mobilityServer;
    Ptr<ListPositionAllocator> posServer = CreateObject<ListPositionAllocator>();
    posServer->Add(Vector(50.0, 50.0, 0.0));
    mobilityServer.SetPositionAllocator(posServer);
    mobilityServer.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityServer.Install(serverNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNodes);
    stack.Install(serverNode);

    Ipv4AddressHelper address;

    // WiFi-Network: Assign different subnets for each AP/STA group
    Ipv4InterfaceContainer staInterfaces[nAps];
    Ipv4InterfaceContainer apInterfaces[nAps];
    for (uint32_t i = 0; i < nAps; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        staInterfaces[i] = address.Assign(staDevices[i]);
        apInterfaces[i] = address.Assign(apDevices[i]);
        address.NewNetwork();
    }

    // CSMA backbone for server/APs
    address.SetBase("10.10.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaIfs = address.Assign(csmaDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Server on remote node
    uint16_t port = 9000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(30.0));

    // Start UDP traffic from each STA to the server
    for (uint32_t i = 0; i < nAps; ++i)
    {
        for (uint32_t j = 0; j < nStasPerAp; ++j)
        {
            uint32_t staIdx = i * nStasPerAp + j;
            UdpClientHelper udpClient(csmaIfs.GetAddress(nAps), port);
            udpClient.SetAttribute("MaxPackets", UintegerValue(3200));
            udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
            udpClient.SetAttribute("PacketSize", UintegerValue(512));

            ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(staIdx));
            clientApp.Start(Seconds(2.0 + staIdx));
            clientApp.Stop(Seconds(29.0));
        }
    }

    // NetAnim
    AnimationInterface anim("wifi-multiap.xml");
    for (uint32_t i = 0; i < wifiApNodes.GetN(); ++i)
        anim.UpdateNodeDescription(wifiApNodes.Get(i), "AP");
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
        anim.UpdateNodeDescription(wifiStaNodes.Get(i), "STA");
    anim.UpdateNodeDescription(serverNode.Get(0), "Server");
    anim.SetMobilityPollInterval(Time("100ms"));

    Simulator::Stop(Seconds(31.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}