#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Wi-Fi with 802.11b standard
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211b);

    // Set data rate to auto and txPower to max for simplicity
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("auto"),
                                       "ControlMode", StringValue("auto"));

    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifiHelper.Install(wifiMac, nodes);

    // Mobility model - constant position
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP client-server application
    uint16_t port = 9;

    // Receiver (Node 1) as UDP Echo Server
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Sender (Node 0) as UDP Echo Client
    Ipv4Address receiverIp = interfaces.GetAddress(1);
    UdpEchoClientHelper client(receiverIp, port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF)); // Unlimited packets
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));     // Will be overridden by data rate
    client.SetAttribute("PacketSize", UintegerValue(1024));

    // Rate-based application
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(receiverIp, port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("500kb/s")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}