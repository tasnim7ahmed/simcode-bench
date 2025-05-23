#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

Ptr<ns3::PacketSink> g_serverApp;

void CalculateThroughput()
{
    if (g_serverApp)
    {
        double totalRxBytes = static_cast<double>(g_serverApp->GetTotalRx());
        double simulationTime = ns3::Simulator::Now().GetSeconds(); 
        double throughputMbps = (totalRxBytes * 8.0) / (simulationTime * 1000000.0);
        
        NS_LOG_UNCOND ("Simulation finished.");
        NS_LOG_UNCOND ("Server received " << totalRxBytes << " bytes in " << simulationTime << " seconds.");
        NS_LOG_UNCOND ("Throughput at server: " << throughputMbps << " Mbps");
    }
}

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("WifiSimpleInfra", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("BulkSendApplication", ns3::LOG_LEVEL_INFO);

    uint32_t numSta = 2; 
    double simulationTime = 10.0; 
    std::string phyMode = "OfdmRate54Mbps"; 
    double channelDelayMs = 2.0; 

    // Create nodes
    ns3::NodeContainer apNode;
    apNode.Create(1);
    ns3::NodeContainer staNodes;
    staNodes.Create(numSta);

    ns3::NodeContainer allNodes;
    allNodes.Add(apNode);
    allNodes.Add(staNodes);

    // Set up Wi-Fi channel with specified delay
    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.SetAttribute("PropagationDelayModel", ns3::StringValue("ns3::ConstantSpeedPropagationDelayModel"));
    ns3::Config::SetDefault("ns3::ConstantSpeedPropagationDelayModel::Delay", ns3::TimeValue(ns3::MilliSeconds(channelDelayMs)));
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    // Set up Wi-Fi PHY and MAC
    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", ns3::StringValue(phyMode),
                                 "ControlMode", ns3::StringValue(phyMode));

    ns3::WifiMacHelper mac;
    ns3::Ssid ssid = ns3::Ssid("ns3-wifi");

    // Configure AP
    mac.SetType("ns3::ApWifiMac", "Ssid", ns3::SsidValue(ssid), "BeaconInterval", ns3::TimeValue(ns3::MicroSeconds(102400)));
    ns3::NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode.Get(0));

    // Configure Stations
    mac.SetType("ns3::StaWifiMac", "Ssid", ns3::SsidValue(ssid), "ActiveProbing", ns3::BooleanValue(false));
    ns3::NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Install Internet stack
    ns3::InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(apDevice);
    ns3::Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Configure Mobility Model and positions
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    apNode.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    staNodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(1.0, 0.0, 0.0)); // Server client
    staNodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(2.0, 0.0, 0.0)); // UDP client

    // Setup UDP Applications
    uint16_t port = 9; 

    // Server: PacketSink on staNodes.Get(0)
    ns3::Address serverAddress = ns3::InetSocketAddress(staInterfaces.GetAddress(0), port);
    ns3::PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", serverAddress);
    ns3::ApplicationContainer serverApps = sinkHelper.Install(staNodes.Get(0));
    g_serverApp = ns3::StaticCast<ns3::PacketSink>(serverApps.Get(0)); 
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(simulationTime));

    // Client: BulkSendApplication on staNodes.Get(1)
    ns3::BulkSendHelper clientHelper("ns3::UdpSocketFactory", serverAddress);
    clientHelper.SetAttribute("MaxBytes", ns3::UintegerValue(0)); 
    clientHelper.SetAttribute("SendBufferSize", ns3::UintegerValue(65536)); 
    ns3::ApplicationContainer clientApps = clientHelper.Install(staNodes.Get(1));
    clientApps.Start(ns3::Seconds(1.0)); 
    clientApps.Stop(ns3::Seconds(simulationTime));

    // Enable tracing
    std::string traceFilename = "wifi-throughput";
    phy.EnablePcapAll(traceFilename);
    ns3::AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream(traceFilename + ".tr"));

    // Schedule throughput calculation at the end of the simulation
    ns3::Simulator::Schedule(ns3::Seconds(simulationTime), &CalculateThroughput);

    // Run simulation
    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}