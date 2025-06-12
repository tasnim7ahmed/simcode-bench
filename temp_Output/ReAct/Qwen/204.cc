#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSNSimulation");

int main(int argc, char *argv[]) {
    bool verbose = true;
    uint32_t numSensors = 5;
    uint32_t packetSize = 1024;
    double interval = 1.0;
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("numSensors", "Number of sensor nodes", numSensors);
    cmd.AddValue("interval", "UDP client packet interval", interval);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer sensors;
    sensors.Create(numSensors);

    NodeContainer baseStation;
    baseStation.Create(1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer sensorDevices;
    NetDeviceContainer baseStationDevice;

    for (uint32_t i = 0; i < numSensors; ++i) {
        NetDeviceContainer link = csma.Install(NodeContainer(sensors.Get(i), baseStation.Get(0)));
        sensorDevices.Add(link.Get(0));
        baseStationDevice.Add(link.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(sensors);
    stack.Install(baseStation);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer sensorInterfaces;
    Ipv4InterfaceContainer baseStationInterfaces;

    for (uint32_t i = 0; i < numSensors; ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(sensorDevices.Get(i), baseStationDevice.Get(i)));
        sensorInterfaces.Add(interfaces.Get(0));
        baseStationInterfaces.Add(interfaces.Get(1));
        address.NewNetwork();
    }

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(baseStation.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper client(baseStationInterfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numSensors; ++i) {
        clientApps = client.Install(sensors.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simulationTime));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensors);
    mobility.Install(baseStation);

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wsn.tr");
    csma.EnableAsciiAll(stream);
    csma.EnablePcapAll("wsn", false);

    AnimationInterface anim("wsn.xml");
    anim.SetConstantPosition(baseStation.Get(0), 30.0, 30.0);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}