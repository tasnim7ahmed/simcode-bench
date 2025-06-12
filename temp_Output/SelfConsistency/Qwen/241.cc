#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/wave-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetWaveSimulation");

int main(int argc, char *argv[]) {
    // Log component settings
    LogComponentEnable("VanetWaveSimulation", LOG_LEVEL_INFO);

    // Create nodes: one vehicle and one RSU
    NodeContainer nodes;
    nodes.Create(2);
    Ptr<Node> vehicleNode = nodes.Get(0);
    Ptr<Node> rsuNode = nodes.Get(1);

    // Set up mobility for the nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Start vehicle movement
    vehicleNode->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20, 0, 0)); // Moving at 20 m/s along x-axis

    // Install WAVE (802.11p) devices
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211_10MHZ);

    // Use default MAC layer configuration
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices = wifi.Install(wifiPhy, nodes);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP communication from vehicle to RSU
    uint16_t port = 9; // Discard port (commonly used for testing)

    // Server (RSU) side: receive packets
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(rsuNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Client (Vehicle) side: send periodic packets
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(128));

    ApplicationContainer clientApp = onoff.Install(vehicleNode);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    wifiPhy.EnablePcap("vanet-wave-simulation", devices);

    // Simulation setup complete
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}