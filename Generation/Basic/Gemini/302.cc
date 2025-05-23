#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

int main(int argc, char *argv[])
{
    // 1. Set up Wi-Fi standards and channel
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // 2. Create nodes: one AP, two STAs
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes; // staNodes.Get(0) is server STA, staNodes.Get(1) is client STA
    staNodes.Create(2);

    NodeContainer allNodes;
    allNodes.Add(apNode);
    allNodes.Add(staNodes);

    // 3. Configure Mobility: static positions for simplicity
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // 4. Configure MAC for AP and STAs
    Ssid ssid = Ssid("my-wifi");

    // AP MAC configuration
    WifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconInterval", TimeValue(NanoSeconds(102400))); 

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, apMac, apNode);

    // STA MAC configuration
    WifiMacHelper staMac;
    staMac.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, staMac, staNodes);

    // 5. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // 6. Assign IP addresses from 192.168.1.0/24
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces; // staInterfaces.Get(0) for server STA, staInterfaces.Get(1) for client STA
    staInterfaces = address.Assign(staDevices);

    // 7. Setup UDP Server and Client Applications

    // UDP Server (STA node 0, i.e., staNodes.Get(0))
    // IP address of server STA will be staInterfaces.GetAddress(0) (e.g., 192.168.1.2)
    uint16_t serverPort = 8080;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    ApplicationContainer serverApps = sink.Install(staNodes.Get(0)); // STA 1 (index 0 in staNodes) as server
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client (STA node 1, i.e., staNodes.Get(1))
    // Client sends to server's IP (staInterfaces.GetAddress(0)) and port (serverPort)
    OnOffHelper client("ns3::UdpSocketFactory",
                       InetSocketAddress(staInterfaces.GetAddress(0), serverPort));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); // Keep client on for the entire duration of sending (1s per packet interval)
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // No off time
    client.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet
    // DataRate calculated for sending one 1024-byte packet per second: 1024 bytes/s * 8 bits/byte = 8192 bps
    client.SetAttribute("DataRate", StringValue("8192bps")); 
    client.SetAttribute("MaxPackets", UintegerValue(10)); // Send 10 packets

    ApplicationContainer clientApps = client.Install(staNodes.Get(1)); // STA 2 (index 1 in staNodes) as client
    clientApps.Start(Seconds(2.0));
    // Client should stop after sending 10 packets, which will take 9 seconds from start (2s + 9s = 11s)
    // or by simulation end if 10 packets are sent earlier.
    // Setting stop slightly beyond expected last packet transmission (2s + 9s = 11s) but capped by simulation end (10s).
    clientApps.Stop(Seconds(10.0)); 

    // 8. Enable PCAP tracing for Wi-Fi devices
    phy.EnablePcap("wifi-ap", apDevices.Get(0));
    phy.EnablePcap("wifi-sta-server", staDevices.Get(0)); // Server STA
    phy.EnablePcap("wifi-sta-client", staDevices.Get(1)); // Client STA

    // 9. Run simulation
    Simulator::Stop(Seconds(10.0)); // Simulation runs for 10 seconds
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}