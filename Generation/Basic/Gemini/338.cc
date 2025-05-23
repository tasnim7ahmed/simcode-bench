#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // 1. Create nodes
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2);

    // 2. Configure Mobility
    // AP is stationary at (0,0,0)
    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(apNode);
    apNode.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // STAs use Random Walk 2D mobility in a 100m x 100m area
    MobilityHelper mobilitySta;
    mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Mode", StringValue("ns3::RandomWalk2dMobilityModel::MODE_RANDOM_DIRECTION"),
                                 "Bounds", RectangleValue(Rectangle(-50.0, 50.0, -50.0, 50.0)));
    mobilitySta.Install(staNodes);

    // 3. Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ); // 802.11n 5GHz

    YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("my-wifi-network");

    // AP configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MicroSeconds(102400))); 
    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    // STA configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // 4. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // 5. Assign IP Addresses (10.1.1.0/24)
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // 6. Setup UDP Applications
    // Server on AP (port 9)
    uint16_t port = 9;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sink.Install(apNode);
    serverApps.Start(Seconds(1.0)); // Start server early
    serverApps.Stop(Seconds(10.0));

    // Clients on STAs (sending to AP's IP on port 9)
    // Packet size: 1024 bytes
    // Interval: 1 second between each packet, implies a data rate of 1024 bytes/sec = 8192 bps
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));  // Send for 1 second
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // No off time
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("8192bps"))); // 1024 bytes/sec = 8192 bits/sec

    ApplicationContainer clientApps;
    clientApps.Add(onoff.Install(staNodes.Get(0))); // Install on STA 0
    clientApps.Add(onoff.Install(staNodes.Get(1))); // Install on STA 1
    clientApps.Start(Seconds(2.0)); // Start clients after server
    clientApps.Stop(Seconds(9.0));  // Stop clients before simulation end

    // 7. Simulation Setup and Run
    Simulator::Stop(Seconds(10.0)); // Simulate for 10 seconds
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}