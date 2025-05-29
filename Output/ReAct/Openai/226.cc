#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/olsr-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkOlsrUdp");

int main(int argc, char *argv[])
{
    uint32_t numSensors = 10;
    double simTime = 50.0;

    CommandLine cmd;
    cmd.AddValue("numSensors", "Number of sensor nodes", numSensors);
    cmd.Parse(argc, argv);

    // Create nodes: (numSensors) + 1 sink
    NodeContainer sensorNodes;
    sensorNodes.Create(numSensors);

    Ptr<Node> sinkNode = CreateObject<Node>();
    NodeContainer allNodes;
    allNodes.Add(sensorNodes);
    allNodes.Add(sinkNode);

    // Set up Wi-Fi (simple adhoc configuration)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // Mobility: random grid for sensors, static sink at center
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(20.0),
                                 "DeltaY", DoubleValue(20.0),
                                 "GridWidth", UintegerValue(5),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    Ptr<ListPositionAllocator> sinkPos = CreateObject<ListPositionAllocator>();
    sinkPos->Add(Vector(50.0, 50.0, 0.0));
    MobilityHelper sinkMobility;
    sinkMobility.SetPositionAllocator(sinkPos);
    sinkMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    sinkMobility.Install(sinkNode);

    // Install energy model
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(5.0));
    EnergySourceContainer sources = energySourceHelper.Install(sensorNodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices.GetN() - 1, devices.GetN(), sources);

    // Internet stack with OLSR
    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Server app on sink node
    uint16_t udpPort = 4000;
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApps = server.Install(sinkNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Client apps on each sensor node
    double interPacketInterval = 2.0; // seconds
    uint32_t packetSize = 64; // bytes
    uint32_t numPackets = static_cast<uint32_t>(simTime / interPacketInterval);

    for (uint32_t i = 0; i < numSensors; ++i)
    {
        UdpClientHelper client(interfaces.GetAddress(numSensors), udpPort);
        client.SetAttribute("MaxPackets", UintegerValue(numPackets));
        client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = client.Install(sensorNodes.Get(i));
        clientApp.Start(Seconds(1.0 + i * 0.1));
        clientApp.Stop(Seconds(simTime));
    }

    // Optional: Enable energy tracing and ascii
    //wifiPhy.EnablePcapAll("wsn-olsr");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}