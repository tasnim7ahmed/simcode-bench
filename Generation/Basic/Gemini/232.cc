#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"

int main(int argc, char *argv[])
{
    uint32_t numClients = 5;
    double simulationTime = 10.0;
    uint32_t payloadSize = 100;
    uint32_t maxBytesToSend = 1000;

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("numClients", "Number of client nodes", numClients);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Size of application packet payload in bytes", payloadSize);
    cmd.AddValue("maxBytesToSend", "Total bytes each client sends", maxBytesToSend);
    cmd.Parse(argc, argv);

    ns3::NodeContainer apNode;
    apNode.Create(1);
    ns3::NodeContainer serverNode;
    serverNode.Create(1);
    ns3::NodeContainer clientNodes;
    clientNodes.Create(numClients);

    ns3::NodeContainer allNodes;
    allNodes.Add(apNode);
    allNodes.Add(serverNode);
    allNodes.Add(clientNodes);

    ns3::MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", ns3::DoubleValue(0.0),
                                  "MinY", ns3::DoubleValue(0.0),
                                  "DeltaX", ns3::DoubleValue(5.0),
                                  "DeltaY", ns3::DoubleValue(5.0),
                                  "GridWidth", ns3::UintegerValue(5),
                                  "LayoutType", ns3::StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n_5GHZ);

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.SetPropagationLoss("ns3::FriisPropagationLossModel");
    ns3::Ptr<ns3::YansWifiChannel> wifiChannel = channel.Create();

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    phy.Set("TxPowerStart", ns3::DoubleValue(10.0));
    phy.Set("TxPowerEnd", ns3::DoubleValue(10.0));

    ns3::WifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac",
                  "Ssid", ns3::SsidValue("my-ssid"),
                  "BeaconInterval", ns3::TimeValue(ns3::NanoSeconds(102400)));

    ns3::NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, apMac, apNode);

    ns3::WifiMacHelper staMac;
    staMac.SetType("ns3::StaWifiMac",
                   "Ssid", ns3::SsidValue("my-ssid"),
                   "ActiveProbing", ns3::BooleanValue(false));

    ns3::NetDeviceContainer serverDevice;
    serverDevice = wifi.Install(phy, staMac, serverNode);

    ns3::NetDeviceContainer clientDevices;
    clientDevices = wifi.Install(phy, staMac, clientNodes);

    ns3::InternetStackHelper stack;
    stack.Install(allNodes);

    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    ns3::Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    ns3::Ipv4InterfaceContainer serverInterface = address.Assign(serverDevice);
    ns3::Ipv4InterfaceContainer clientInterfaces = address.Assign(clientDevices);

    ns3::Ipv4Address serverIpAddress = serverInterface.GetAddress(0);
    uint16_t serverPort = 9;

    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                           ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), serverPort));
    ns3::ApplicationContainer serverApp = packetSinkHelper.Install(serverNode);
    serverApp.Start(ns3::Seconds(0.0));
    serverApp.Stop(ns3::Seconds(simulationTime));

    ns3::OnOffHelper onoffHelper("ns3::TcpSocketFactory",
                                 ns3::Address(ns3::InetSocketAddress(serverIpAddress, serverPort)));
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoffHelper.SetAttribute("DataRate", ns3::DataRateValue(ns3::DataRate("1Mbps")));
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(payloadSize));
    onoffHelper.SetAttribute("MaxBytes", ns3::UintegerValue(maxBytesToSend));

    ns3::ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numClients; ++i)
    {
        clientApps.Add(onoffHelper.Install(clientNodes.Get(i)));
    }
    
    double clientStartTime = 1.0;
    for (uint32_t i = 0; i < clientApps.GetN(); ++i)
    {
        clientApps.Get(i)->SetStartTime(ns3::Seconds(clientStartTime + i * 0.1));
        clientApps.Get(i)->SetStopTime(ns3::Seconds(simulationTime));
    }

    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}