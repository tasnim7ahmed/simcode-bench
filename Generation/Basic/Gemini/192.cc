#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("WirelessRouter", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);

    double simulationTime = 10.0;
    uint32_t packetSize = 1024;
    std::string dataRate = "1Mbps";
    uint16_t sinkPort = 9;

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue("dataRate", "CBR data rate", dataRate);
    cmd.Parse(argc, argv);

    ns3::NodeContainer nodes;
    nodes.Create(3);

    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(nodes.Get(0));
    nodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));

    mobility.Install(nodes.Get(1));
    nodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(50.0, 0.0, 0.0));

    mobility.Install(nodes.Get(2));
    nodes.Get(2)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(100.0, 0.0, 0.0));

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    ns3::YansWifiPhyHelper phyHelper;
    ns3::YansWifiChannelHelper channelHelper;
    channelHelper.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    channelHelper.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");

    ns3::NqosWifiMacHelper macHelper = ns3::NqosWifiMacHelper::Default();
    macHelper.SetType("ns3::AdhocWifiMac");

    ns3::Ptr<ns3::YansWifiChannel> channel01 = channelHelper.Create();
    phyHelper.SetChannel(channel01);
    ns3::NetDeviceContainer devices01;
    devices01.Add(wifi.Install(phyHelper, macHelper, nodes.Get(0)));
    devices01.Add(wifi.Install(phyHelper, macHelper, nodes.Get(1)));

    ns3::Ptr<ns3::YansWifiChannel> channel12 = channelHelper.Create();
    phyHelper.SetChannel(channel12);
    ns3::NetDeviceContainer devices12;
    devices12.Add(wifi.Install(phyHelper, macHelper, nodes.Get(1)));
    devices12.Add(wifi.Install(phyHelper, macHelper, nodes.Get(2)));

    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces01 = ipv4.Assign(devices01);

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces12 = ipv4.Assign(devices12);

    ns3::Ptr<ns3::Ipv4> ipv4Router = nodes.Get(1)->GetObject<ns3::Ipv4>();
    ipv4Router->SetIpForward(true);

    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    ns3::PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), sinkPort));
    ns3::ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(2));
    sinkApps.Start(ns3::Seconds(0.0));
    sinkApps.Stop(ns3::Seconds(simulationTime + 1.0));

    ns3::Ipv4Address destAddr = interfaces12.GetAddress(1);
    ns3::UdpClientHelper udpClient(destAddr, sinkPort);
    udpClient.SetAttribute("MaxPackets", ns3::UintegerValue(4294967295U));
    udpClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(packetSize * 8 / (1000000.0 * ns3::DataRate(dataRate).GetBitRate()))));
    udpClient.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(1.0));
    clientApps.Stop(ns3::Seconds(simulationTime));

    ns3::AsciiTraceHelper ascii;
    ns3::Ptr<ns3::OutputStreamWrapper> stream = ascii.CreateFileStream("wireless-router.tr");
    wifi.EnableAsciiAll(stream);

    phyHelper.EnablePcap("wireless-router-node0", devices01.Get(0));
    phyHelper.EnablePcap("wireless-router-node1-if0", devices01.Get(1));
    phyHelper.EnablePcap("wireless-router-node1-if1", devices12.Get(0));
    phyHelper.EnablePcap("wireless-router-node2", devices12.Get(1));

    ns3::AnimationInterface anim("wireless-router.xml");
    anim.SetConstantPosition(nodes.Get(0), ns3::Vector(0.0, 0.0, 0.0));
    anim.SetConstantPosition(nodes.Get(1), ns3::Vector(50.0, 0.0, 0.0));
    anim.SetConstantPosition(nodes.Get(2), ns3::Vector(100.0, 0.0, 0.0));
    anim.EnableWifiPhyCounters(devices01, "wireless-router-wifi-01.xml");
    anim.EnableWifiPhyCounters(devices12, "wireless-router-wifi-12.xml");

    ns3::Simulator::Stop(ns3::Seconds(simulationTime + 2.0));
    ns3::Simulator::Run();

    ns3::Ptr<ns3::PacketSink> sink = ns3::DynamicCast<ns3::PacketSink>(sinkApps.Get(0));
    uint64_t totalRxBytes = sink->GetTotalRxBytes();
    double actualThroughput = (double)totalRxBytes * 8 / (simulationTime * 1000000.0);

    std::cout << "Total Rx Bytes: " << totalRxBytes << " bytes" << std::endl;
    std::cout << "Throughput: " << actualThroughput << " Mbps" << std::endl;

    ns3::Simulator::Destroy();

    return 0;
}