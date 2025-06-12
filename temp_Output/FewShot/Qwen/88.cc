#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergyExample");

static void RemainingEnergy(double oldValue, double remainingEnergy) {
    NS_LOG_INFO("Remaining energy = " << remainingEnergy << " J");
}

static void TotalEnergyConsumption(double oldValue, double totalEnergy) {
    NS_LOG_INFO("Total energy consumed = " << totalEnergy << " J");
}

int main(int argc, char *argv[]) {
    uint32_t packetSize = 1024;
    double simulationStartTime = 1.0;
    double simulationStopTime = 10.0;
    double nodeDistance = 5.0; // meters

    CommandLine cmd;
    cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue("simulationStartTime", "Start time for the simulation (seconds)", simulationStartTime);
    cmd.AddValue("nodeDistance", "Distance between nodes (meters)", nodeDistance);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("WifiPhy", LOG_LEVEL_DEBUG);

    NodeContainer nodes;
    nodes.Create(2);

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(nodeDistance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Wi-Fi setup
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.Set("ChannelWidth", UintegerValue(20));
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0206)); // ~40 mW
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0206));
    wifiPhy.Set("RxGain", DoubleValue(0));

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifiPhy.Install(nodes);

    // Energy source and device energy model
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(10.0)); // 10 Joules initial energy

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.350)); // Transmission current in Amperes
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.195)); // Reception current in Amperes

    EnergySourceContainer sources = energySourceHelper.Install(nodes);
    radioEnergyHelper.Install(devices, sources);

    // Connect trace sources
    for (uint32_t i = 0; i < sources.GetN(); ++i) {
        Ptr<BasicEnergySource> basicSource = DynamicCast<BasicEnergySource>(sources.Get(i));
        basicSource->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergy));
        basicSource->TraceConnectWithoutContext("TotalEnergyConsumption", MakeCallback(&TotalEnergyConsumption));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Server application on node 1
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(simulationStartTime));
    serverApp.Stop(Seconds(simulationStopTime));

    // Client application on node 0
    UdpEchoClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(simulationStartTime + 1.0));
    clientApp.Stop(Seconds(simulationStopTime));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}