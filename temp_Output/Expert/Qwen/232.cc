#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWifiSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 1 server, 3 clients
    NodeContainer serverNode;
    serverNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3);

    NodeContainer allNodes = NodeContainer(serverNode, clientNodes);

    // Setup Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
    NetDeviceContainer clientDevices = wifi.Install(phy, mac, clientNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
    NetDeviceContainer serverDevice = wifi.Install(phy, mac, serverNode);

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
    mobility.Install(allNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverInterface = address.Assign(serverDevice);
    Ipv4InterfaceContainer clientInterfaces = address.Assign(clientDevices);

    // Server application: PacketSink
    uint16_t sinkPort = 8080;
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(serverNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Client applications: OnOffApplication over TCP
    uint32_t payloadSize = 512; // bytes
    DataRate dataRate("100kbps");

    for (uint32_t i = 0; i < clientNodes.GetN(); ++i) {
        OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(serverInterface.GetAddress(0), sinkPort));
        onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onOffHelper.SetAttribute("DataRate", DataRateValue(dataRate));
        onOffHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));

        ApplicationContainer app = onOffHelper.Install(clientNodes.Get(i));
        app.Start(Seconds(1.0 + i)); // staggered start times
        app.Stop(Seconds(9.0));
    }

    // Enable logging
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Set simulation time and run
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}