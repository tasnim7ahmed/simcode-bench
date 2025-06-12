#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshNetworkSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Set up mesh positions in a 2x2 grid layout
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Setup Wi-Fi for mesh communication
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211s);

    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("OfdmRate6Mbps"),
                                       "ControlMode", StringValue("OfdmRate6Mbps"));

    wifiMac.SetType("ns3::MeshWifiInterfaceMac",
                    "Ssid", SsidValue(Ssid("mesh-network")),
                    "ActiveProbing", BooleanValue(true));

    NetDeviceContainer devices = wifiMac.Install(wifiHelper, nodes);

    // Install Internet stack and assign IP addresses
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP client-server applications
    uint16_t port = 9; // well-known echo port

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));

    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(nodes.Get(2));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(15.0));

    // Enable PCAP tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mesh-network.tr");
    wifiHelper.EnableAsciiAll(stream);

    // Enable PCAP on all devices
    wifiHelper.EnablePcapAll("mesh-network");

    // Start simulation
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}