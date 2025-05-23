#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("WifiMac", ns3::LOG_LEVEL_INFO);

    ns3::CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    ns3::NodeContainer nodes;
    nodes.Create(2); // node 0: AP, node 1: STA
    ns3::Ptr<ns3::Node> apNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> staNode = nodes.Get(1);

    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    apNode->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    staNode->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(1.0, 0.0, 0.0));

    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211b);

    ns3::YansWifiChannelHelper channel;
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    channel.AddPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    ns3::Ptr<ns3::YansWifiChannel> wifiChannel = channel.Create();

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);

    ns3::NqosWifiMacHelper mac = ns3::NqosWifiMacHelper::Default();
    ns3::Ssid ssid = ns3::Ssid("ns3-wifi");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "BeaconInterval", ns3::TimeValue(ns3::MicroSeconds(102400)));
    ns3::NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "ActiveProbing", ns3::BooleanValue(false));
    ns3::NetDeviceContainer staDevices = wifi.Install(phy, mac, staNode);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apIpInterface = ipv4.Assign(apDevices);
    ns3::Ipv4InterfaceContainer staIpInterface = ipv4.Assign(staDevices);

    uint16_t port = 9;

    ns3::PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                     ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer sinkApps = sinkHelper.Install(apNode);
    sinkApps.Start(ns3::Seconds(0.0));
    sinkApps.Stop(ns3::Seconds(10.0));

    ns3::OnOffHelper onoffHelper("ns3::UdpSocketFactory",
                                 ns3::InetSocketAddress(apIpInterface.GetAddress(0), port));
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(512));
    
    // Rate: 1 packet every 0.1 seconds = 10 packets per second (pps)
    // DataRate = PacketSize (bytes) * 8 (bits/byte) * pps
    // DataRate = 512 * 8 * 10 = 40960 bps
    onoffHelper.SetAttribute("DataRate", ns3::DataRateValue("40960bps"));
    
    // Set OnTime to a constant value of 1 (always on during active period)
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    // Set OffTime to a constant value of 0 (never off during active period)
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ns3::ApplicationContainer clientApps = onoffHelper.Install(staNode);
    // Client starts at 0.0 seconds and runs for 10.0 seconds to send 100 packets
    // (10 packets/second * 10 seconds = 100 packets)
    clientApps.Start(ns3::Seconds(0.0));
    clientApps.Stop(ns3::Seconds(10.0));

    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}