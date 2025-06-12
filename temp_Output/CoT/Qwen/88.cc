#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergySimulation");

class EnergyTracer {
public:
    EnergyTracer(Ptr<OutputStreamWrapper> stream)
        : m_stream(stream) {}

    void RemainingEnergy(double oldValue, double remainingEnergy) {
        *m_stream->GetStream() << Simulator::Now().GetSeconds() << " " << remainingEnergy << std::endl;
    }

    void TotalEnergyConsumed(double oldValue, double totalEnergy) {
        NS_LOG_INFO("Total Energy Consumed: " << totalEnergy << " J");
    }

private:
    Ptr<OutputStreamWrapper> m_stream;
};

int main(int argc, char *argv[]) {
    double simulationTime = 10.0;
    double startTime = 1.0;
    uint32_t packetSize = 1024;
    double nodeDistance = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("startTime", "Start time for applications", startTime);
    cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue("nodeDistance", "Distance between nodes in meters", nodeDistance);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

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

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(startTime));
    clientApps.Stop(Seconds(simulationTime));

    BasicEnergySourceHelper energySource;
    energySource.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
    energySource.Set("BasicEnergySourceSupplyVoltageV", DoubleValue(3.7));

    WifiRadioEnergyModelHelper radioEnergyHelper;
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, energySource);

    EnergySourceContainer sources = energySource.Install(nodes);

    AsciiTraceHelper asciiTraceHelper;
    auto stream = asciiTraceHelper.CreateFileStream("remaining-energy-trace.txt");

    EnergyTracer tracer(stream);

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<BasicEnergySource> basicSource = DynamicCast<BasicEnergySource>(sources.Get(i));
        basicSource->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&EnergyTracer::RemainingEnergy, &tracer));
        basicSource->TraceConnectWithoutContext("TotalEnergyConsumed", MakeCallback(&EnergyTracer::TotalEnergyConsumed, &tracer));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}