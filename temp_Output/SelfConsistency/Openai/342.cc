#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiGridTcpExample");

int
main(int argc, char *argv[])
{
    // Enable logging for UdpClient and UdpServer
    //LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    //LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Set simulation parameters
    double simulationTime = 20.0; // seconds
    uint32_t packetSize = 512; // bytes
    uint32_t numPackets = 100;
    double interval = 0.1; // seconds
    uint16_t port = 9;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up WiFi 802.11b
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper wifiMac;
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    // Each node is a sta and ap (adhoc)
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility (grid positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::ListPositionAllocator",
        "PositionList", VectorValue({Vector(0.0, 0.0, 0.0), Vector(5.0, 10.0, 0.0)}));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP server (PacketSink) on node 1
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    // Set up TCP client (BulkSendApplication) on node 0
    BulkSendHelper bulkSender("ns3::TcpSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSender.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
    bulkSender.SetAttribute("SendSize", UintegerValue(packetSize));
    bulkSender.SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));

    ApplicationContainer clientApp = bulkSender.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));

    // Limit sending rate to match 0.1s interval between packets (use a custom app)
    // Use BulkSend, but throttle via socket callbacks

    // Alternatively, use OnOffApplication with TCP:
    /*
    OnOffHelper onoff("ns3::TcpSocketFactory",
          InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(packetSize * 8 / interval) + "bps")));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));
    */

    // Enable pcap tracing
    wifiPhy.EnablePcapAll("wifi-tcp-grid");

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}