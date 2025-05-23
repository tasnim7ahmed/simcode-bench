#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    double simulationTime = 10.0; // seconds
    uint32_t payloadSize = 1024;  // bytes
    uint32_t numPackets = 1000;
    double packetInterval = 0.01; // seconds

    // Create nodes
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2); // STA0 and STA1

    // Configure Mobility
    // AP: Fixed position
    Ptr<ConstantPositionMobilityModel> apMobility = CreateObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    apNode.Get(0)->AggregateObject(apMobility);

    // STAs: RandomWalk2dMobilityModel
    Ptr<RandomWalk2dMobilityModel> sta0Mobility = CreateObject<RandomWalk2dMobilityModel>();
    sta0Mobility->SetBounds(Box(0, 50, 0, 50, 0, 0)); // 50x50m area
    sta0Mobility->SetSpeed(UniformRandomVariable(0.5, 2.0)); // 0.5 to 2.0 m/s
    sta0Mobility->SetPause(ConstantRandomVariable(0.5));     // 0.5s pause
    staNodes.Get(0)->AggregateObject(sta0Mobility);

    Ptr<RandomWalk2dMobilityModel> sta1Mobility = CreateObject<RandomWalk2dMobilityModel>();
    sta1Mobility->SetBounds(Box(0, 50, 0, 50, 0, 0)); // 50x50m area
    sta1Mobility->SetSpeed(UniformRandomVariable(0.5, 2.0)); // 0.5 to 2.0 m/s
    sta1Mobility->SetPause(ConstantRandomVariable(0.5));     // 0.5s pause
    staNodes.Get(1)->AggregateObject(sta1Mobility);

    // Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ); // 802.11n standard (5GHz band)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmHtMcs7")); // OfdmHtMcs7 for 802.11n

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                               "Exponent", DoubleValue(3.0),
                               "ReferenceDistance", DoubleValue(1.0),
                               "ReferenceLoss", DoubleValue(46.677)); // Typical loss for 5GHz at 1m

    Ptr<YansWifiChannel> wifiChannel = channel.Create();
    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-wifi");

    // AP MAC
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconInterval", TimeValue(MicroSeconds(102400)));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, apNode);

    // STA MACs
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // Get IP addresses for client/server setup
    Ipv4Address sta0Ip = staInterfaces.GetAddress(0);

    // Configure Applications
    uint16_t port = 9; // UDP Port for the server

    // STA 0 (Server) - PacketSink
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sink.Install(staNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // STA 1 (Client) - UdpClient
    UdpClientHelper client(sta0Ip, port);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApps = client.Install(staNodes.Get(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(1.0 + numPackets * packetInterval + 0.1)); // Stop after all packets are sent + a small buffer

    // Simulation Setup
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}