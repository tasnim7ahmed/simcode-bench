#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 1 AP + 3 STAs
    NodeContainer apNodes;
    apNodes.Create(1);
    NodeContainer staNodes;
    staNodes.Create(3);

    // Configure Wi-Fi PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    // Configure AP
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MicroSeconds(102400)),
                "BeaconGeneration", BooleanValue(true));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);

    // Configure STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    // UDP Echo Server on AP
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(apNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Echo Clients on STAs
    UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        clientApps.Add(echoClient.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}