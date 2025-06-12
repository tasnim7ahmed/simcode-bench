#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-server-client-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshThreeNodesSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Setup mobility with static positions
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Wi-Fi mesh devices
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211s);

    wifiMac.SetType("ns3::MeshWifiInterfaceMac",
                    "Ssid", SsidValue(Ssid("meshNetwork")),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer meshDevices = wifiHelper.Install(wifiMac, nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(meshDevices);

    // Setup UDP traffic between nodes
    uint16_t port = 9; // Discard port (RFC 863)

    // Server (PacketSink) on node 0
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Client on node 1 sending to node 0
    UdpClientHelper udpClient(interfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(20));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = udpClient.Install(nodes.Get(1));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(10.0));

    // Client on node 2 sending to node 0
    ApplicationContainer clientApp2 = udpClient.Install(nodes.Get(2));
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(10.0));

    // Enable PCAP tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mesh-three-nodes.tr");
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        wifiHelper.EnablePcap("mesh-three-nodes-" + std::to_string(i), nodes.Get(i)->GetId(), true, true);
        meshDevices.Get(i)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&DhcpHeader::Print, stream));
        meshDevices.Get(i)->TraceConnectWithoutContext("PhyTxDrop", MakeBoundCallback(&DhcpHeader::Print, stream));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}