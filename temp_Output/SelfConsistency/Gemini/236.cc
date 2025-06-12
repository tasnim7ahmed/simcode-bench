#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpClientServer");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Disable fragmentation
    UdpClient::SetDefault("FragmentSize", UintegerValue(65535));

    // Create nodes: AP and STA (station)
    NodeContainer apNodes, staNodes;
    apNodes.Create(1);
    staNodes.Create(1);

    // Configure Wi-Fi PHY and channel
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure MAC layer (AP and STA)
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // AP configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);

    // STA configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Install the internet stack on the nodes
    InternetStackHelper internet;
    internet.Install(apNodes);
    internet.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Create UDP client application
    uint16_t port = 9; // Discard port (RFC 863)

    UdpClientHelper client(apInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(staNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Create UDP server application
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(apNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(11.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}