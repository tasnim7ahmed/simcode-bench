#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpExample");

int main(int argc, char *argv[]) {
    bool verbose = false;
    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application containers to log if set to true", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    // Create nodes: AP and stations
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(3);

    // Create channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    // Configure AP
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    // Configure STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Create UDP server on AP
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Create UDP clients on STAs
    UdpClientHelper client(apInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        clientApps.Add(client.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}