#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // 1. Create Nodes
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNode;
    staNode.Create(1);

    // 2. Mobility
    // AP: Stationary
    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(apNode);
    apNode.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // Client (STA): Random Walk in 100x100 m^2 area
    MobilityHelper mobilitySta;
    mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(-50.0, 50.0, -50.0, 50.0)));
    mobilitySta.Install(staNode);
    // Set initial position for STA (e.g., center of the area)
    staNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // 3. Wi-Fi Setup
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    YansWifiPhyHelper phy;
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();
    phy.SetChannel(wifiChannel);

    Ssid ssid = Ssid("my-wifi-network");

    // Configure AP
    NqosWifiMacHelper apMac = NqosWifiMacHelper::Default();
    apMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconInterval", TimeValue(MicroSeconds(102400)));
    NetDeviceContainer apDevices = wifi.Install(phy, apMac, apNode);

    // Configure STA
    NqosWifiMacHelper staMac = NqosWifiMacHelper::Default();
    staMac.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid),
                   "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, staMac, staNode);

    // 4. Internet Stack & IP Addressing
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // 5. Applications
    uint16_t port = 9; // Access Point listens on port 9

    // Server (PacketSink on AP)
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(apNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(20.0)); 

    // Client (OnOffApplication on STA)
    // Client sends 10 TCP packets (1024 bytes each) at 1-second interval.
    // Achieved by setting DataRate to 1024 bytes/s and MaxBytes to 10*1024 bytes.
    OnOffHelper client("ns3::TcpSocketFactory", Address(InetSocketAddress(apInterfaces.GetAddress(0), port)));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("DataRate", DataRateValue(DataRate("8192bps"))); // 1024 bytes/sec = 8192 bits/sec
    client.SetAttribute("MaxBytes", UintegerValue(10 * 1024)); // Total 10 packets of 1024 bytes

    ApplicationContainer clientApp = client.Install(staNode);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(15.0));

    // 6. Tracing (Optional)
    // phy.EnablePcap("wifi-ap", apDevices.Get(0));
    // phy.EnablePcap("wifi-sta", staDevices.Get(0));

    // 7. Simulation
    Simulator::Stop(Seconds(16.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}