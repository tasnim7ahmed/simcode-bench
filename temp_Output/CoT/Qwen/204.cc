#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSNSimulation");

int main(int argc, char *argv[]) {
    uint32_t numSensorNodes = 5;
    uint32_t numBaseStations = 1;
    double simulationTime = 10.0;
    std::string animFile = "wsn-animation.xml";

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer sensorNodes;
    sensorNodes.Create(numSensorNodes);

    NodeContainer baseStation;
    baseStation.Create(numBaseStations);

    NodeContainer allNodes;
    allNodes.Add(sensorNodes);
    allNodes.Add(baseStation);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices;
    devices = csma.Install(allNodes);

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpServerHelper serverHelper(9);
    ApplicationContainer serverApps = serverHelper.Install(baseStation.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper clientHelper(interfaces.GetAddress(numSensorNodes), 9);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(1000));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numSensorNodes; ++i) {
        clientApps.Add(clientHelper.Install(sensorNodes.Get(i)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    AnimationInterface anim(animFile);
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routingtable-wsn.xml", Seconds(0), Seconds(simulationTime), Seconds(0.5));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    Time totalE2EDelay = Seconds(0);

    for (uint32_t i = 0; i < clientApps.GetN(); ++i) {
        Ptr<UdpClient> client = DynamicCast<UdpClient>(clientApps.Get(i));
        totalTxPackets += client->GetSent();
        totalRxPackets += client->GetReceived();
        totalE2EDelay += client->GetAverageRtt();
    }

    double packetDeliveryRatio = static_cast<double>(totalRxPackets) / totalTxPackets;
    Time averageE2EDelay = totalE2EDelay / clientApps.GetN();

    NS_LOG_UNCOND("Packet Delivery Ratio: " << packetDeliveryRatio * 100 << "% (" << totalRxPackets << "/" << totalTxPackets << ")");
    NS_LOG_UNCOND("Average End-to-End Delay: " << averageE2EDelay.As(Time::MS));

    Simulator::Destroy();
    return 0;
}