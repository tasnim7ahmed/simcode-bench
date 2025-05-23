#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

// ns-3 logging
NS_LOG_COMPONENT_DEFINE("SimpleWifiTcp");

int main(int argc, char *argv[])
{
    // Enable logging for this example
    NS_LOG_INFO("Creating simple wireless network with TCP.");

    // 1. Create Nodes
    NodeContainer nodes;
    nodes.Create(2); // Node 0: Client, Node 1: Server

    // 2. Configure Mobility Model (static positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0), // Nodes 5m apart
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 3. Configure WiFi Channel and Physical Layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b); // Use 802.11b

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0)); // 16 dBm
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));   // 16 dBm

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLoss("ns3::LogDistancePropagationLossModel",
                                  "Exponent", DoubleValue(3.0));
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // 4. Configure MAC Layer (Ad-hoc mode with specified SSID)
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetSsid(ssid);

    // 5. Install WiFi Devices
    NetDeviceContainer wifiDevices;
    wifiDevices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 6. Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // 7. Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer nodeInterfaces = ipv4.Assign(wifiDevices);

    // 8. Configure TCP Server (PacketSink)
    uint16_t port = 9; // TCP Port 9
    Address serverAddress = InetSocketAddress(nodeInterfaces.GetAddress(1), port); // Node 1 is server

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(1)); // Install on Node 1
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // 9. Configure TCP Client (OnOffApplication)
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", StringValue("1Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet

    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0)); // Install on Node 0
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(Seconds(10.0)); // Client stops at 10 seconds

    // 10. Run Simulation
    Simulator::Stop(Seconds(11.0)); // Stop simulation slightly after applications
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");

    return 0;
}