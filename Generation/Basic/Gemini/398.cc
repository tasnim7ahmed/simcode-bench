#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

int
main(int argc, char* argv[])
{
    ns3::Config::SetDefault("ns3::TcpL4Protocol::SocketType", ns3::StringValue("ns3::TcpReno"));

    ns3::NodeContainer nodes;
    nodes.Create(2);

    ns3::Ptr<ns3::Node> serverNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> clientNode = nodes.Get(1);

    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(serverNode);
    mobility.Install(clientNode);

    serverNode->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(
        ns3::Vector(0.0, 0.0, 0.0));
    clientNode->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(
        ns3::Vector(1.0, 0.0, 0.0));

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211a);

    ns3::YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.Set("RxGain", ns3::DoubleValue(0));
    wifiPhy.Set("TxPowerStart", ns3::DoubleValue(7.5));
    wifiPhy.Set("TxPowerEnd", ns3::DoubleValue(7.5));

    ns3::YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    ns3::NqosWifiMacHelper wifiMac = ns3::NqosWifiMacHelper::Default();
    ns3::Ssid ssid = ns3::Ssid("ns3-wifi");

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", ns3::SsidValue(ssid), "BeaconInterval", ns3::TimeValue(ns3::MilliSeconds(100)));
    ns3::NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, serverNode);

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", ns3::SsidValue(ssid));
    ns3::NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMac, clientNode);

    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);
    ns3::Ipv4InterfaceContainer staInterfaces = address.Assign(staDevice);

    uint16_t port = 9;
    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                           ns3::InetSocketAddress(ns3::Ipv4Address::Any(), port));
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install(serverNode);
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(10.0));

    ns3::OnOffHelper onoffHelper("ns3::TcpSocketFactory",
                                 ns3::InetSocketAddress(apInterfaces.GetAddress(0), port));
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    onoffHelper.SetAttribute("DataRate", ns3::DataRateValue("81920bps")); // 1024 bytes / 0.1 s = 81920 bps

    ns3::ApplicationContainer clientApps = onoffHelper.Install(clientNode);
    clientApps.Start(ns3::Seconds(1.0));
    clientApps.Stop(ns3::Seconds(9.0));

    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}