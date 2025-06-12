#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Default timing parameters
    double slot = 9.0;   // in microseconds
    double sifs = 16.0;  // in microseconds
    double pifs = 25.0;  // in microseconds

    // Parse command-line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("slot", "Slot time (microseconds)", slot);
    cmd.AddValue("sifs", "SIFS duration (microseconds)", sifs);
    cmd.AddValue("pifs", "PIFS duration (microseconds)", pifs);
    cmd.Parse(argc, argv);

    // Enable logging if needed
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: AP and STA
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Create channel and PHY helpers
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Set up Wi-Fi MAC with timing attributes
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    // Modify timing parameters via Attributes
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    // Configure MAC layer timing parameters
    mac.SetType("ns3::ApWifiMac",
                "Slot", TimeValue(MicroSeconds(slot)),
                "Sifs", TimeValue(MicroSeconds(sifs)),
                "Pifs", TimeValue(MicroSeconds(pifs)));

    // Install devices
    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac");
    apDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    NetDeviceContainer staDevice;
    mac.SetType("ns3::StaWifiMac");
    staDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // UDP Server on STA
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on AP
    UdpClientHelper client(staInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF)); // Unlimited packets
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output throughput
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStats stats = monitor->GetFlowStats().begin()->second;
    double throughput = stats.rxBytes * 8.0 / (stats.timeLastRxPacket.GetSeconds() - stats.timeFirstTxPacket.GetSeconds()) / 1e6;
    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}