#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpMultiClientServer");

int main(int argc, char *argv[]) {
    uint32_t nClients = 3;
    double simTime = 10.0;
    uint16_t port = 8080;

    CommandLine cmd;
    cmd.AddValue("nClients", "Number of client nodes", nClients);
    cmd.Parse(argc, argv);

    // Enable TCP logging for the server
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: server (AP) + clients (STAs)
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nClients);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure Wi-Fi PHY and Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi standard and MAC
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid),
        "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility: static positions
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nClients; ++i) {
        positionAlloc->Add(Vector(0.0 + i*2.0, 0.0, 0.0));
    }
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    NodeContainer allNodes;
    allNodes.Add(wifiStaNodes);
    allNodes.Add(wifiApNode);
    mobility.Install(allNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Install PacketSink Application (TCP Server) on AP
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(simTime));

    // Install OnOffClient apps on client STAs
    for (uint32_t i = 0; i < nClients; ++i) {
        OnOffHelper client("ns3::TcpSocketFactory",
            InetSocketAddress(apInterface.GetAddress(0), port));
        client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
        client.SetAttribute("PacketSize", UintegerValue(512));
        client.SetAttribute("MaxBytes", UintegerValue(2048));
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i));
        clientApp.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}