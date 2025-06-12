#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/lorawan-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LorawanUdpSensorSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create sensor node and sink node
    NodeContainer sensorNodes;
    sensorNodes.Create(1);
    NodeContainer sinkNodes;
    sinkNodes.Create(1);

    // Create gateway node (to forward packets from sensor to sink)
    NodeContainer gateways;
    gateways.Create(1);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0,0.0,0.0)); // Sensor
    positionAlloc->Add(Vector(30.0,0.0,0.0)); // Sink
    positionAlloc->Add(Vector(15.0,15.0,10.0)); // Gateway
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(sensorNodes);
    mobility.Install(sinkNodes);
    mobility.Install(gateways);

    // Install LoRaWAN and Internet stack on sensor, sink, and gateway
    LorawanHelper lorawan;
    lorawan.EnableGateway(gateways);

    lorawan.InstallEndDevices(sensorNodes);
    lorawan.InstallGateways(gateways);

    InternetStackHelper internet;
    internet.Install(sensorNodes);
    internet.Install(sinkNodes);
    internet.Install(gateways);

    // Assign IP addresses via a simple CSMA network at the gateway and sink for simplicity
    NodeContainer backbone;
    backbone.Add(gateways.Get(0));
    backbone.Add(sinkNodes.Get(0));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    NetDeviceContainer csmaDevices = csma.Install(backbone);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(csmaDevices);

    // Assign loopback address for sensor node (sensor node has no direct wired IP connectivity, but for UDP App we emulate IP)
    Ipv4Address sensorAddress("10.2.1.1");
    Ptr<Ipv4> ipv4Sensor = sensorNodes.Get(0)->GetObject<Ipv4>();
    int32_t ifIndex = ipv4Sensor->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4Sensor->AddAddress(ifIndex, Ipv4InterfaceAddress(sensorAddress, Ipv4Mask("255.255.255.255")));
    ipv4Sensor->SetMetric(ifIndex, 1);
    ipv4Sensor->SetUp(ifIndex);

    // Sink: UDP Server Application
    uint16_t sinkPort = 9000;
    UdpServerHelper udpServer(sinkPort);
    ApplicationContainer serverApp = udpServer.Install(sinkNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(30.0));

    // Sensor: UDP Client Application (Sending to sink)
    UdpClientHelper udpClient(interfaces.GetAddress(1), sinkPort); // sink is backbone.Get(1) => index 1
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(32));

    ApplicationContainer clientApp = udpClient.Install(sensorNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(30.0));

    // Run simulation
    Simulator::Stop(Seconds(31.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}