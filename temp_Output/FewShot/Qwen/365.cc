#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set the simulation seed for reproducibility
    RngSeedManager::SetSeed(1);

    // Create two nodes: AP and STA
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Create a Wi-Fi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC layer
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);  // Use 802.11a for 54Mbps
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue("OfdmRate54Mbps"),
                                  "ControlMode", StringValue("OfdmRate54Mbps"));

    // Set up SSID
    Ssid ssid = Ssid("ns3-wifi");

    // Install AP node
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    // Install STA node
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Set up mobility model (static positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // Setup UDP packet sink on AP (server side)
    uint16_t port = 9;
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                      InetSocketAddress(port));
    ApplicationContainer serverApp = packetSinkHelper.Install(wifiNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP echo client on STA (client side)
    InetSocketAddress remoteAddress(staInterface.GetAddress(0), port);
    remoteAddress.SetTos(0xb8); // Set DSCP value CS7

    OnOffHelper clientHelper("ns3::UdpSocketFactory", remoteAddress);
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = clientHelper.Install(wifiNodes.Get(1));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}