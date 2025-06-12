#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lora-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LoRaWanSensorToSink");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: one sensor node and one sink node
    NodeContainer nodes;
    nodes.Create(2);

    // Setup LoRa channel
    Ptr<LoRaChannel> channel = CreateObject<LoRaChannel>();

    // Setup PHY and MAC for both nodes
    LoRaPhyHelper phyHelper = LoRaPhyHelper();
    phyHelper.SetChannel(channel);
    phyHelper.Set("TxDbm", DoubleValue(14.0));
    phyHelper.Set("RxSensitivityDbm", DoubleValue(-137.0));

    LoRaMacHelper macHelper = LoRaMacHelper();

    NetDeviceContainer devices;
    devices = phyHelper.Install(nodes);
    macHelper.Install(devices);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t sinkPort = 8000;

    // Sink node listens on UDP port
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Sensor node sends UDP packets periodically
    OnOffHelper onOffHelper("ns3::UdpSocketFactory",
                            InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(50));

    ApplicationContainer sensorApp = onOffHelper.Install(nodes.Get(0));
    sensorApp.Start(Seconds(1.0));
    sensorApp.Stop(Seconds(10.0));

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}