#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    // Parameters
    uint32_t packetSize = 512;
    uint32_t numPackets = 10;
    double interval = 0.1;
    uint16_t port = 9;
    double simulationTime = 20.0;

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);

    // Wifi channel and phy/mac configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", 
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi-grid");

    // Install wifi devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Assign mobility: 3x3 grid, take first three positions
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(30.0),
                                  "DeltaY", DoubleValue(30.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(staDevices);

    // TCP Server (Node 2) - use PacketSink
    uint32_t serverNodeIdx = 2;
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(wifiStaNodes.Get(serverNodeIdx));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // TCP Clients (Node 0 and 1)
    for (uint32_t clientNodeIdx = 0; clientNodeIdx < 2; ++clientNodeIdx)
    {
        BulkSendHelper bulkSend("ns3::TcpSocketFactory",
                                InetSocketAddress(interfaces.GetAddress(serverNodeIdx), port));
        bulkSend.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
        bulkSend.SetAttribute("SendSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = bulkSend.Install(wifiStaNodes.Get(clientNodeIdx));
        clientApps.Start(Seconds(1.0 + clientNodeIdx * 0.01));
        clientApps.Stop(Seconds(simulationTime - 1));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}